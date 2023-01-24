// QOI encoder/decoder library
#include <stdint.h>
#include "../image/image_mat.h"

// Settings
#define QOI_ENABLE_STAT

// Worst QOI buffer coefficient
#define QOI_WORST_BUFFER_COEFF 4

// QOI operation code
#define QOI_OP_INDEX    0x00
#define QOI_OP_DIFF     0x40
#define QOI_OP_LUMA     0x80
#define QOI_OP_RUN      0xC0
#define QOI_OP_RGB      0xFE

// QOI operation length
#define QOI_LEN_INDEX    1
#define QOI_LEN_DIFF     1
#define QOI_LEN_LUMA     2
#define QOI_LEN_RUN      1
#define QOI_LEN_RGB      4

// QOI data mask
#define QOI_OPCODE_MASK 0xC0
#define QOI_DATA_MASK   0x3F

// Running length
#define QOI_MAX_RUN_LENGTH 62

// Hash LUT size
#define QOI_HASH_LUT_SIZE 64
typedef uint8_t qoi_hash_t;

// Union for color
typedef union 
{
    uint32_t color_32b;
    struct 
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;
    } rgba;
} color_u;

typedef struct
{
    int16_t red;
    int16_t green;
    int16_t blue;
    int16_t alpha; 
} color_diff;

typedef enum
{
    QOI_ENCODE_RGB = 0,
    QOI_ENCODE_INDEX = 1,
    QOI_ENCODE_DIFF = 2,
    QOI_ENCODE_LUMA = 3,
    QOI_ENCODE_RUN = 4
} qoi_op;


uint32_t qoiEncode(ImageMat* imageMat, uint8_t** pDataBuffer);
void qoiDecode(uint8_t* dataBuffer, ImageMat* imageMat);
