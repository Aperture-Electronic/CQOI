// QOI encoder/decoder core
// Designer: Deng LiWei
// Date: 2022/01
// Description: QOI encoder/decoder library
#include <malloc.h>
#include <memory.h>
#include "qoi.h"

uint8_t colorEqual(color_u a, color_u b)
{
#if defined(QOI_ENABLE_RGBA)
    return (a.color_32b == b.color_32b);
#else
    return ((a.color_32b & 0x00FFFFFF) == (b.color_32b & 0x00FFFFFF));
#endif
}


uint8_t alphaEqual(color_u a, color_u b)
{
#if defined(QOI_ENABLE_RGBA)
    return a.rgba.alpha == b.rgba.alpha;
#else
    return 1;
#endif
}

void colorDiff(color_u a, color_u b, color_diff *diff)
{
    diff->red = a.rgba.red - b.rgba.red;
    diff->green = a.rgba.green - b.rgba.green;
    diff->blue = a.rgba.blue - b.rgba.blue;
}

#define PINT16_MASK_RANGE(_DATA_, _MASK_) !((_DATA_) & (_MASK_))
#define NINT16_MASK_RANGE(_DATA_, _MASK_) !(((_DATA_) & (_MASK_)) ^ (_MASK_))

uint8_t colorSmallDiff(color_u a, color_u b, color_diff *diff)
{
    int16_t dRed, dGreen, dBlue;
    colorDiff(a, b, diff);

    dRed = diff->red;
    dGreen = diff->green;
    dBlue = diff->blue;

    // Mask data
    diff->red &= 0x03;
    diff->green &= 0x03;
    diff->blue &= 0x03;

    // Check in range
    // If d > 0, the MSB is 0, then if d > 1, the masked value > 0
    // If d < 0, the MSB is 1, then if d < -2, the masked value > 0
    uint8_t dRedInRange = (dRed & 0x8000) ? NINT16_MASK_RANGE(dRed, 0xFFFE) : PINT16_MASK_RANGE(dRed, 0xFFFE);
    uint8_t dGreenInRange = (dGreen & 0x8000) ? NINT16_MASK_RANGE(dGreen, 0xFFFE) : PINT16_MASK_RANGE(dGreen, 0xFFFE);
    uint8_t dBlueInRange = (dBlue & 0x8000) ? NINT16_MASK_RANGE(dBlue, 0xFFFE) : PINT16_MASK_RANGE(dBlue, 0xFFFE);

    return dRedInRange && dGreenInRange && dBlueInRange;
}

uint8_t colorLumaDiff(color_u a, color_u b, color_diff *ycbcr)
{
    color_diff diff;
    colorDiff(a, b, &diff);

    int16_t dGreen, cRed, cBlue;
    dGreen = diff.green;
    cRed = diff.red - dGreen;
    cBlue = diff.blue - dGreen;

    // Check in range
    // If c > 0, the MSB is 0, then if c > 7, the masked value > 0
    // If c < 0, the MSB is 1, then if c < -8, the masked value > 0
    uint8_t dGreenInRange = (dGreen & 0x8000) ? NINT16_MASK_RANGE(dGreen, 0xFFE0) : PINT16_MASK_RANGE(dGreen, 0xFFE0);
    uint8_t cRedInRange = (cRed & 0x8000) ? NINT16_MASK_RANGE(cRed, 0xFFF8) : PINT16_MASK_RANGE(cRed, 0xFFF8);
    uint8_t cBlueInRange = (cBlue & 0x8000) ? NINT16_MASK_RANGE(cBlue, 0xFFF8) : PINT16_MASK_RANGE(cBlue, 0xFFF8);

    ycbcr->red = cRed & 0x0F;
    ycbcr->green = diff.green & 0x3F;
    ycbcr->blue = cBlue & 0x0F;

    return dGreenInRange && cRedInRange && cBlueInRange;
}

qoi_hash_t qoiHash(color_u color)
{
#if defined(QOI_ENABLE_RGBA)
    int hash = color.rgba.red * 3  + color.rgba.green * 5 + 
               color.rgba.blue * 7 + color.rgba.alpha * 11;
#else
    int hash = color.rgba.red * 3 + color.rgba.green * 5 + color.rgba.blue * 7;
#endif
    return (hash % QOI_HASH_LUT_SIZE);
}

color_u smallDiffDecode(uint8_t data, color_u lastColor)
{
    uint8_t delta = data & QOI_DATA_MASK;
    uint8_t dr = (delta & 0x30) >> 4;
    uint8_t dg = (delta & 0x0C) >> 2;
    uint8_t db = (delta & 0x03) >> 0;

    int16_t dr_s = (dr & 0x2) ? (dr | 0xFFFC) : dr;
    int16_t dg_s = (dg & 0x2) ? (dg | 0xFFFC) : dg;
    int16_t db_s = (db & 0x2) ? (db | 0xFFFC) : db;

    color_u newColor;
    newColor.rgba.red = lastColor.rgba.red + dr_s;
    newColor.rgba.green = lastColor.rgba.green + dg_s;
    newColor.rgba.blue = lastColor.rgba.blue + db_s;
    newColor.rgba.alpha = lastColor.rgba.alpha;

    return newColor;
}

color_u lumaDecode(uint8_t data, uint8_t cbcr, color_u lastColor)
{
    uint8_t dg = data & QOI_DATA_MASK;
    uint8_t cr = (cbcr & 0xF0) >> 4;
    uint8_t cb = (cbcr & 0x0F) >> 0;

    int16_t dg_s = (dg & 0x20) ? (dg | 0xFFC0) : dg;
    int16_t cr_s = (cr & 0x08) ? (cr | 0xFFF8) : cr;
    int16_t cb_s = (cb & 0x08) ? (cb | 0xFFF8) : cb;

    int8_t dr_s = dg_s + cr_s;
    int8_t db_s = dg_s + cb_s;

    color_u newColor;
    newColor.rgba.red = lastColor.rgba.red + dr_s;
    newColor.rgba.green = lastColor.rgba.green + dg_s;
    newColor.rgba.blue = lastColor.rgba.blue + db_s;
    newColor.rgba.alpha = lastColor.rgba.alpha;

    return newColor;
}


#if defined(QOI_ENABLE_STAT)
uint32_t qoiEncode(ImageMat *imageMat, uint8_t **pDataBuffer, uint32_t **qoi_stat)
#else
uint32_t qoiEncode(ImageMat *imageMat, uint8_t **pDataBuffer)
#endif
{
    // 1. Alloc the maximum data space in buffer
    //  The space is same as the image matrix
    uint32_t imageSize = imageMat->height * imageMat->width;
    uint8_t *dataBuffer = (uint8_t *)malloc(sizeof(uint8_t) * imageSize * QOI_WORST_BUFFER_COEFF);
    *pDataBuffer = dataBuffer; // Assign the buffer to the pointer

    // 2. Perpare of color hash LUT and buffers
    color_u lastColor;
    lastColor.color_32b = 0x00000000; // Set initial value

    color_u hashLUT[QOI_HASH_LUT_SIZE];
    for (int i = 0; i < QOI_HASH_LUT_SIZE; hashLUT[i].color_32b = 0x00000000, i++)
        ; // Clear LUT and disable the flag

#if defined(QOI_ENABLE_STAT)
    // Perpare the statistic buffer
    *qoi_stat = (uint32_t *)malloc(QOI_ENCODING_METHOD_COUNT * sizeof(uint32_t));
    memset(*qoi_stat, 0, QOI_ENCODING_METHOD_COUNT * sizeof(uint32_t));
#endif

    // 3. Pixel-wise scan and encode
    int fileOffset = 0;
    int width = imageMat->width;
    int height = imageMat->height;

    uint8_t run_mode = 0;
    uint8_t run_length = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
#if defined(QOI_ENABLE_STAT)
            qoi_op encode_op;
#endif

            color_u color;
            color_diff color_d; // Delta color
            color.color_32b = GetPixel(imageMat, x, y);

            // Get color hash
            qoi_hash_t hash = qoiHash(color);

        
            // a). Check if the color is same as last
            if (colorEqual(color, lastColor))
            {
                if (!run_mode) // Enter run mode when first meet same pixel
                {
                    run_mode = 1;
                    run_length = 0; // Clear the run length
                }
                else // Increase the run length when in the run mode
                {
                    run_length++;

                    // But the running length can not greater than QOI_MAX_RUN_LENGTH
                    if (run_length == QOI_MAX_RUN_LENGTH - 1)
                    {
                        // If the maximum running length excceed,
                        // we must write a QOI_OP_RUN
                        *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_RUN | run_length;
                        fileOffset += QOI_LEN_RUN;
                        run_mode = 0; // And, clear the run mode

#if defined(QOI_ENABLE_STAT)
                        // Statistic
                        encode_op = QOI_ENCODE_RUN;
#endif
                    }
                }

                continue;
            }
            else
            {
                // If the color changed,
                // we must write a QOI_OP_RUN if we are in run mode
                if (run_mode)
                {
                    *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_RUN | run_length;
                    fileOffset += QOI_LEN_RUN;
                    run_mode = 0; // And, clear the run mode

#if defined(QOI_ENABLE_STAT)
                    // Statistic
                    (*qoi_stat)[QOI_ENCODE_RUN]++;
#endif

                    // We need to execute next codes for current pixel
                }
            }

            // b). Try to use different to encode small difference pixels
            if (alphaEqual(color, lastColor) && colorSmallDiff(color, lastColor, &color_d))
            {
                // Use DIFF to encode
#ifndef QOI_ENABLE_TWOS_COMPLEMENT
                color_d.red   = (color_d.red + 2) & 0x03;            
                color_d.green = (color_d.green + 2) & 0x03;
                color_d.blue  = (color_d.blue + 2) & 0x03;
#endif             
                uint8_t diff = (color_d.red << 4) | (color_d.green << 2) | (color_d.blue << 0);
                *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_DIFF | diff;
                fileOffset += QOI_LEN_DIFF;

#if defined(QOI_ENABLE_STAT)
                // Statistic
                encode_op = QOI_ENCODE_DIFF;
#endif

                goto LAST_COLOR_UPDATE;
            }

            // c). Query the LUT and try to use Hash to encode
            if (colorEqual(hashLUT[hash], color))
            {
                // When encoding, we must check the color in LUT
                // is or not equal to the pixel color.
                // If the color is same, use INDEX to encode.
                // Otherwise, use other encoding method and update the LUT.
                *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_INDEX | hash;
                fileOffset += QOI_LEN_INDEX;

#if defined(QOI_ENABLE_STAT)
                // Statistic
                encode_op = QOI_ENCODE_INDEX;
#endif
                goto LAST_COLOR_UPDATE;
            }

            // d). Try to use luma different
            if (alphaEqual(color, lastColor) && colorLumaDiff(color, lastColor, &color_d))
            {
                // Use LUMA to encode
#ifndef QOI_ENABLE_TWOS_COMPLEMENT
                color_d.green = (color_d.green + 32) & 0x3F;
                color_d.red   = (color_d.red + 8) & 0x0F;
                color_d.blue  = (color_d.blue + 8) & 0x0F;
#endif
                *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_LUMA | color_d.green;
                *(uint8_t *)(dataBuffer + fileOffset + 1) = (color_d.red << 4) | (color_d.blue);
                fileOffset += QOI_LEN_LUMA;

#if defined(QOI_ENABLE_STAT)
                // Statistic
                encode_op = QOI_ENCODE_LUMA;
#endif

                goto LAST_COLOR_UPDATE;
            }

            // e). The worst case: original RGB/RGBA
            if (alphaEqual(color, lastColor))
            {
                *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_RGB;
                *(uint8_t *)(dataBuffer + fileOffset + 1) = color.rgba.red;
                *(uint8_t *)(dataBuffer + fileOffset + 2) = color.rgba.green;
                *(uint8_t *)(dataBuffer + fileOffset + 3) = color.rgba.blue;
                fileOffset += QOI_LEN_RGB;

#if defined(QOI_ENABLE_STAT)
                // Statistic
                encode_op = QOI_ENCODE_RGB;
#endif
            }
#if defined(QOI_ENABLE_RGBA)
            else
            {
                *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_RGBA;
                *(uint8_t *)(dataBuffer + fileOffset + 1) = color.rgba.red;
                *(uint8_t *)(dataBuffer + fileOffset + 2) = color.rgba.green;
                *(uint8_t *)(dataBuffer + fileOffset + 3) = color.rgba.blue;
                *(uint8_t *)(dataBuffer + fileOffset + 4) = color.rgba.alpha;
                fileOffset += QOI_LEN_RGBA;

#if defined(QOI_ENABLE_STAT)
                // Statistic
                encode_op = QOI_ENCODE_RGBA;
#endif
            }
#endif

            goto HASH_UPDATE;

        HASH_UPDATE:
            // Update and active hash LUT
#if defined(QOI_ENABLE_RGBA)
            hashLUT[hash].color_32b = color.color_32b;
#else
            hashLUT[hash].color_32b = color.color_32b | 0xFF000000;
#endif

        LAST_COLOR_UPDATE:
            lastColor = color;

#if defined(QOI_ENABLE_STAT)
        STAT_UPDATE:
            (*qoi_stat)[encode_op]++;
#endif
        } // x
    } // y

    if (run_mode) // If we still in running mode after all pixels 
    {
        // Save the last running encode
        *(uint8_t *)(dataBuffer + fileOffset) = QOI_OP_RUN | run_length;
        fileOffset += QOI_LEN_RUN;
        run_mode = 0;
    }

    return fileOffset;
}

void qoiDecode(uint8_t *dataBuffer, ImageMat *imageMat)
{
    // 1. Perpare of color hash LUT and buffers
    color_u lastColor;
    lastColor.color_32b = 0x00000000; // Set initial value

    color_u hashLUT[QOI_HASH_LUT_SIZE];
    for (int i = 0; i < QOI_HASH_LUT_SIZE; hashLUT[i].color_32b = 0x00000000, i++)
        ; // Clear LUT and disable the flag

    // 2. Decode
    int fileOffset = 0;
    int width = imageMat->width;
    int height = imageMat->height;
    int x = 0, y = 0;

    uint8_t run_mode = 0;
    uint8_t run_length = 0;

    do
    {
        color_u color;
        qoi_hash_t hash;

        // a). Check if in running mode
        if (run_mode)
        {
            if (run_length != 0)
            {
                run_length--;
                color = lastColor; // Repeat last pixel
                goto COORDINATE_UPDATE;
            }
            else
            {
                fileOffset += QOI_LEN_RUN;
                run_mode = 0; // Exit running mode
            }
        }

        // b). Get a byte from buffer to analysis the opcode
        uint8_t fstData = *(uint8_t *)(dataBuffer + fileOffset);

        // c). Decode the pixels by opcode
#if defined(QOI_ENABLE_RGBA)
        if (!(fstData ^ QOI_OP_RGBA)) // RGBA
        {
            color.rgba.red   = *(uint8_t *)(dataBuffer + fileOffset + 1);
            color.rgba.green = *(uint8_t *)(dataBuffer + fileOffset + 2);
            color.rgba.blue  = *(uint8_t *)(dataBuffer + fileOffset + 3);
            color.rgba.alpha = *(uint8_t *)(dataBuffer + fileOffset + 4);

            fileOffset += QOI_LEN_RGBA;

            goto HASH_UPDATE;
        }
#endif
        if (!(fstData ^ QOI_OP_RGB)) // RGB
        {
            color.rgba.red   = *(uint8_t *)(dataBuffer + fileOffset + 1);
            color.rgba.green = *(uint8_t *)(dataBuffer + fileOffset + 2);
            color.rgba.blue  = *(uint8_t *)(dataBuffer + fileOffset + 3);
            color.rgba.alpha = 0xFF;

            fileOffset += QOI_LEN_RGB;

            goto HASH_UPDATE;
        }

        uint8_t fstDataOpcode = fstData & QOI_OPCODE_MASK;

        if (!(fstDataOpcode ^ QOI_OP_INDEX)) // INDEX
        {
            hash = fstData & QOI_DATA_MASK;
            color = hashLUT[hash];

            fileOffset += QOI_LEN_INDEX;

            goto LAST_COLOR_UPDATE;
        }

        if (!(fstDataOpcode ^ QOI_OP_DIFF)) // DIFF
        {
            color = smallDiffDecode(fstData, lastColor);
            fileOffset += QOI_LEN_DIFF;

            goto LAST_COLOR_UPDATE;
        }

        if (!(fstDataOpcode ^ QOI_OP_LUMA)) // LUMA
        {
            uint8_t cbcr = *(uint8_t *)(dataBuffer + fileOffset + 1);
            color = lumaDecode(fstData, cbcr, lastColor);
            fileOffset += QOI_LEN_LUMA;

            goto LAST_COLOR_UPDATE;
        }

        if (!(fstDataOpcode ^ QOI_OP_RUN)) // RUN
        {
            run_mode = 1;
            run_length = fstData & QOI_DATA_MASK;

            color = lastColor;
            goto COORDINATE_UPDATE;
        }

    HASH_UPDATE:
        // Update and active hash LUT
        hash = qoiHash(color);
        hashLUT[hash].color_32b = color.color_32b;

    LAST_COLOR_UPDATE:
        lastColor = color;

    COORDINATE_UPDATE:
        color.rgba.alpha = 0xFF;
        SetPixel(imageMat, x, y, color.color_32b);

        x++;
        if (x == width)
        {
            x = 0;
            y++;
        }
    } while (!((x == 0) && (y == height)));
}
