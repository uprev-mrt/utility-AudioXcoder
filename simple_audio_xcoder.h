/**
 * @file simple_audio_xcoder.h
 * @author Jason Berger
 * @brief basic audio encode/decode functions
 * @version 0.1
 * @date 2020-02-05
 * 
 */

#ifdef __cplusplus
 extern "C" {
#endif

#include <string.h>
#include <stdint.h>

/**
 * @brief encodes  buffer to ulaw and copies output to new buffer
 * @param in ptr to int16 buffer
 * @param out ptr to int8_t buffer
 * @param len number of samples to copy
 * @return int 
 */
int encode_ulaw_buf(int16_t* in, int8_t* out, int len);


#ifdef __cplusplus
}
#endif