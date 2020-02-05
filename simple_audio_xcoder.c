/**
 * @file simple_audio_xcoder.c
 * @author Jason Berger
 * @brief basic audio encode/decode functions
 * @version 0.1
 * @date 2020-02-05
 * 
 */

#include "simple_audio_xcoder.h"


int encode_ulaw_buf(int16_t* in, int8_t* out, int len)
{
    const uint16_t MULAW_MAX = 0x1FFF;
    const uint16_t MULAW_BIAS = 33;
    uint16_t mask;
    uint8_t sign;
    uint8_t position;
    uint8_t lsb ;
    int16_t amp;

    for(int i=0; i < len; i++)
    {
        mask = 0x1000;
        sign = 0;
        position = 12;
        lsb = 0;
        amp = in[i];
        if (amp < 0)
        {
            amp = -amp;
            sign = 0x80;
        }
        amp += MULAW_BIAS;
        if (amp > MULAW_MAX)
        {
            amp = MULAW_MAX;
        }
        for (; ((amp & mask) != mask && position >= 5); mask >>= 1, position--)
                ;
        lsb = (amp >> (position - 4)) & 0x0f;
        out[i] = (~(sign | ((position - 5) << 4) | lsb));
    }
    return len;
}


#ifdef __cplusplus
}
#endif