/**
 * @file audio_xcoder.c
 * @author Jason Berger
 * @brief 
 * @version 0.1
 * @date 2020-02-04
 * 
 */
#include "audio_xcoder.h"



int16_t ulaw_decoded[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
     -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
     -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
     -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
     -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
     -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
     -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
      -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
      -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
      -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
      -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
      -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
       -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
     32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
     23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
     15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
     11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
      7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
      5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
      3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
      2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
      1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
      1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
       876,    844,    812,    780,    748,    716,    684,    652,
       620,    588,    556,    524,    492,    460,    428,    396,
       372,    356,    340,    324,    308,    292,    276,    260,
       244,    228,    212,    196,    180,    164,    148,    132,
       120,    112,    104,     96,     88,     80,     72,     64,
	56,     48,     40,     32,     24,     16,      8,      0 };

void xcoder_ctx_init(xcoder_ctx_t* ctx , int rate, uint8_t options)
{

    memset(ctx, 0, sizeof(*ctx));
    if (rate == 48000)
        ctx->bits_per_sample = 6;
    else if (rate == 56000)
        ctx->bits_per_sample = 7;
    else
        ctx->bits_per_sample = 8;

    if ((options & XCODER_OPT_8k))
        ctx->eight_k = true;
    if ((options & XCODER_OPT_PACKED)  &&  ctx->bits_per_sample != 8)
        ctx->packed = true;
    else
        ctx->packed = false;
    ctx->band[0].det = 32;
    ctx->band[1].det = 8;

    ctx->amp_mask = 0xFFFF;
}

static __inline__ int16_t saturate(int32_t amp)
{
    int16_t amp16;

    /* Hopefully this is optimised for the common case - not clipping */
    amp16 = (int16_t) amp;
    if (amp == amp16)
        return amp16;
    if (amp > INT16_MAX)
        return  INT16_MAX;
    return  INT16_MIN;
}

void xcoder_set_fifo(xcoder_ctx_t* ctx, fifo_t* fifo, uint8_t port)
{
  if(port == XCODER_PORT_AMP)
  {
    ctx->amp_fifo = fifo;
  }
  else
  {
    ctx->code_fifo = fifo;
  }
}

void xcoder_set_buffer(xcoder_ctx_t* ctx, void* buffer, uint8_t port)
{
  if(port == XCODER_PORT_AMP)
  {
    ctx->amp_fifo = NULL;
    ctx->amp_buffer = buffer;
  }
  else
  {
    ctx->code_fifo = NULL;
    ctx->code_buffer = buffer;
  }
}

static __inline__ uint8_t get_next_code(xcoder_ctx_t* ctx)
{
  uint8_t val;
  if(ctx->code_fifo != NULL)
  {
    fifo_pop(ctx->code_fifo, &val);
    ctx->code_idx++;
  }
  else
  {
    val = ctx->code_buffer[ctx->code_idx++];
  }
  return val;
}

static __inline__ int16_t get_next_amp(xcoder_ctx_t* ctx)
{
  int16_t val;
  if(ctx->amp_fifo != NULL)
  {
    fifo_pop(ctx->amp_fifo, &val);
    ctx->amp_idx++;
  }
  else
  {
    val = ctx->amp_buffer[ctx->amp_idx++];
  }


 // if we are smoothing the signal
   if(ctx->amp_averaging > 1)
   {
     ctx->amp_sum -= ctx->amp_window[ctx->amp_win_idx];  //subtracr oldest val from sum
     ctx->amp_window[ctx->amp_win_idx++] = val;          //put val in window
     ctx->amp_sum += val;                                //add val to sum
     if(ctx->amp_win_idx > ctx->amp_averaging)           //wrap idx
       ctx->amp_win_idx = 0;
     val = ctx->amp_sum/ ctx->amp_averaging;             //get window average
   }

  return val + ctx->amp_offset;
}

static __inline__ void set_next_amp(xcoder_ctx_t* ctx, int16_t val)
{
  //if we are smoothing the signal
  if(ctx->amp_averaging > 1)
  {
    ctx->amp_sum -= ctx->amp_window[ctx->amp_win_idx];  //subtracr oldest val from sum
    ctx->amp_window[ctx->amp_win_idx++] = val;          //put val in window
    ctx->amp_sum += val;                                //add val to sum
    if(ctx->amp_win_idx > ctx->amp_averaging)           //wrap idx
      ctx->amp_win_idx = 0;
    val = ctx->amp_sum/ ctx->amp_averaging;             //get window average
  }

  val += ctx->amp_offset;
  if(ctx->amp_fifo != NULL)
  {
    fifo_push(ctx->amp_fifo, &val);
    ctx->amp_idx++;
  }
  else
  {
    ctx->amp_buffer[ctx->amp_idx++] = val;
  }
}

static __inline__ void set_next_code(xcoder_ctx_t* ctx, uint8_t val)
{
  if(ctx->code_fifo != NULL)
  {
    fifo_push(ctx->code_fifo, &val);
    ctx->code_idx++;
  }
  else
  {
    ctx->code_buffer[ctx->code_idx++] = val;
  }
}

static void block4(xcoder_ctx_t* ctx, int band, int d)
{
    int wd1;
    int wd2;
    int wd3;
    int i;

    /* Block 4, RECONS */
    ctx->band[band].d[0] = d;
    ctx->band[band].r[0] = saturate(ctx->band[band].s + d);

    /* Block 4, PARREC */
    ctx->band[band].p[0] = saturate(ctx->band[band].sz + d);

    /* Block 4, UPPOL2 */
    for (i = 0;  i < 3;  i++)
        ctx->band[band].sg[i] = ctx->band[band].p[i] >> 15;
    wd1 = saturate(ctx->band[band].a[1] << 2);

    wd2 = (ctx->band[band].sg[0] == ctx->band[band].sg[1])  ?  -wd1  :  wd1;
    if (wd2 > 32767)
        wd2 = 32767;
    wd3 = (ctx->band[band].sg[0] == ctx->band[band].sg[2])  ?  128  :  -128;
    wd3 += (wd2 >> 7);
    wd3 += (ctx->band[band].a[2]*32512) >> 15;
    if (wd3 > 12288)
        wd3 = 12288;
    else if (wd3 < -12288)
        wd3 = -12288;
    ctx->band[band].ap[2] = wd3;

    /* Block 4, UPPOL1 */
    ctx->band[band].sg[0] = ctx->band[band].p[0] >> 15;
    ctx->band[band].sg[1] = ctx->band[band].p[1] >> 15;
    wd1 = (ctx->band[band].sg[0] == ctx->band[band].sg[1])  ?  192  :  -192;
    wd2 = (ctx->band[band].a[1]*32640) >> 15;

    ctx->band[band].ap[1] = saturate(wd1 + wd2);
    wd3 = saturate(15360 - ctx->band[band].ap[2]);
    if (ctx->band[band].ap[1] > wd3)
        ctx->band[band].ap[1] = wd3;
    else if (ctx->band[band].ap[1] < -wd3)
        ctx->band[band].ap[1] = -wd3;

    /* Block 4, UPZERO */
    wd1 = (d == 0)  ?  0  :  128;
    ctx->band[band].sg[0] = d >> 15;
    for (i = 1;  i < 7;  i++)
    {
        ctx->band[band].sg[i] = ctx->band[band].d[i] >> 15;
        wd2 = (ctx->band[band].sg[i] == ctx->band[band].sg[0])  ?  wd1  :  -wd1;
        wd3 = (ctx->band[band].b[i]*32640) >> 15;
        ctx->band[band].bp[i] = saturate(wd2 + wd3);
    }

    /* Block 4, DELAYA */
    for (i = 6;  i > 0;  i--)
    {
        ctx->band[band].d[i] = ctx->band[band].d[i - 1];
        ctx->band[band].b[i] = ctx->band[band].bp[i];
    }

    for (i = 2;  i > 0;  i--)
    {
        ctx->band[band].r[i] = ctx->band[band].r[i - 1];
        ctx->band[band].p[i] = ctx->band[band].p[i - 1];
        ctx->band[band].a[i] = ctx->band[band].ap[i];
    }

    /* Block 4, FILTEP */
    wd1 = saturate(ctx->band[band].r[1] + ctx->band[band].r[1]);
    wd1 = (ctx->band[band].a[1]*wd1) >> 15;
    wd2 = saturate(ctx->band[band].r[2] + ctx->band[band].r[2]);
    wd2 = (ctx->band[band].a[2]*wd2) >> 15;
    ctx->band[band].sp = saturate(wd1 + wd2);

    /* Block 4, FILTEZ */
    ctx->band[band].sz = 0;
    for (i = 6;  i > 0;  i--)
    {
        wd1 = saturate(ctx->band[band].d[i] + ctx->band[band].d[i]);
        ctx->band[band].sz += (ctx->band[band].b[i]*wd1) >> 15;
    }
    ctx->band[band].sz = saturate(ctx->band[band].sz);

    /* Block 4, PREDIC */
    ctx->band[band].s = saturate(ctx->band[band].sp + ctx->band[band].sz);
}


int xcoder_decode_g722(xcoder_ctx_t* ctx, int len)
{
    static const int wl[8] = {-60, -30, 58, 172, 334, 538, 1198, 3042 };
    static const int rl42[16] = {0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3,  2, 1, 0 };
    static const int ilb[32] =
    {
        2048, 2093, 2139, 2186, 2233, 2282, 2332,
        2383, 2435, 2489, 2543, 2599, 2656, 2714,
        2774, 2834, 2896, 2960, 3025, 3091, 3158,
        3228, 3298, 3371, 3444, 3520, 3597, 3676,
        3756, 3838, 3922, 4008
    };
    static const int wh[3] = {0, -214, 798};
    static const int rh2[4] = {2, 1, 2, 1};
    static const int qm2[4] = {-7408, -1616,  7408,   1616};
    static const int qm4[16] =
    {
              0, -20456, -12896,  -8968,
          -6288,  -4240,  -2584,  -1200,
          20456,  12896,   8968,   6288,
           4240,   2584,   1200,      0
    };
    static const int qm5[32] =
    {
           -280,   -280, -23352, -17560,
         -14120, -11664,  -9752,  -8184,
          -6864,  -5712,  -4696,  -3784,
          -2960,  -2208,  -1520,   -880,
          23352,  17560,  14120,  11664,
           9752,   8184,   6864,   5712,
           4696,   3784,   2960,   2208,
           1520,    880,    280,   -280
    };
    static const int qm6[64] =
    {
           -136,   -136,   -136,   -136,
         -24808, -21904, -19008, -16704,
         -14984, -13512, -12280, -11192,
         -10232,  -9360,  -8576,  -7856,
          -7192,  -6576,  -6000,  -5456,
          -4944,  -4464,  -4008,  -3576,
          -3168,  -2776,  -2400,  -2032,
          -1688,  -1360,  -1040,   -728,
          24808,  21904,  19008,  16704,
          14984,  13512,  12280,  11192,
          10232,   9360,   8576,   7856,
           7192,   6576,   6000,   5456,
           4944,   4464,   4008,   3576,
           3168,   2776,   2400,   2032,
           1688,   1360,   1040,    728,
            432,    136,   -432,   -136
    };
    static const int qmf_coeffs[12] =
    {
           3,  -11,   12,   32, -210,  951, 3876, -805,  362, -156,   53,  -11,
    };

    int dlowt;
    int rlow;
    int ihigh;
    int dhigh;
    int rhigh;
    int xout1;
    int xout2;
    int wd1;
    int wd2;
    int wd3;
    int code =0;
    int i;
    int j;

    ctx->code_idx = 0;
    ctx->amp_idx = 0;

    rhigh = 0;
    for (j = 0;  j < len;  )
    {
        if (ctx->packed)
        {
            /* Unpack the code bits */
            if (ctx->in_bits < ctx->bits_per_sample)
            {
                ctx->in_buffer = get_next_code(ctx) << ctx->in_bits;
                j++;
                ctx->in_bits += 8;
            }
            code = ctx->in_buffer & ((1 << ctx->bits_per_sample) - 1);
            ctx->in_buffer >>= ctx->bits_per_sample;
            ctx->in_bits -= ctx->bits_per_sample;
        }
        else
        {
            ctx->in_buffer = get_next_code(ctx) << ctx->in_bits;
            j++;
        }

        switch (ctx->bits_per_sample)
        {
        default:
        case 8:
            wd1 = code & 0x3F;
            ihigh = (code >> 6) & 0x03;
            wd2 = qm6[wd1];
            wd1 >>= 2;
            break;
        case 7:
            wd1 = code & 0x1F;
            ihigh = (code >> 5) & 0x03;
            wd2 = qm5[wd1];
            wd1 >>= 1;
            break;
        case 6:
            wd1 = code & 0x0F;
            ihigh = (code >> 4) & 0x03;
            wd2 = qm4[wd1];
            break;
        }
        /* Block 5L, LOW BAND INVQBL */
        wd2 = (ctx->band[0].det*wd2) >> 15;
        /* Block 5L, RECONS */
        rlow = ctx->band[0].s + wd2;
        /* Block 6L, LIMIT */
        if (rlow > 16383)
            rlow = 16383;
        else if (rlow < -16384)
            rlow = -16384;

        /* Block 2L, INVQAL */
        wd2 = qm4[wd1];
        dlowt = (ctx->band[0].det*wd2) >> 15;

        /* Block 3L, LOGSCL */
        wd2 = rl42[wd1];
        wd1 = (ctx->band[0].nb*127) >> 7;
        wd1 += wl[wd2];
        if (wd1 < 0)
            wd1 = 0;
        else if (wd1 > 18432)
            wd1 = 18432;
        ctx->band[0].nb = wd1;

        /* Block 3L, SCALEL */
        wd1 = (ctx->band[0].nb >> 6) & 31;
        wd2 = 8 - (ctx->band[0].nb >> 11);
        wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
        ctx->band[0].det = wd3 << 2;

        block4(ctx, 0, dlowt);

        if (!ctx->eight_k)
        {
            /* Block 2H, INVQAH */
            wd2 = qm2[ihigh];
            dhigh = (ctx->band[1].det*wd2) >> 15;
            /* Block 5H, RECONS */
            rhigh = dhigh + ctx->band[1].s;
            /* Block 6H, LIMIT */
            if (rhigh > 16383)
                rhigh = 16383;
            else if (rhigh < -16384)
                rhigh = -16384;

            /* Block 2H, INVQAH */
            wd2 = rh2[ihigh];
            wd1 = (ctx->band[1].nb*127) >> 7;
            wd1 += wh[wd2];
            if (wd1 < 0)
                wd1 = 0;
            else if (wd1 > 22528)
                wd1 = 22528;
            ctx->band[1].nb = wd1;

            /* Block 3H, SCALEH */
            wd1 = (ctx->band[1].nb >> 6) & 31;
            wd2 = 10 - (ctx->band[1].nb >> 11);
            wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
            ctx->band[1].det = wd3 << 2;

            block4(ctx, 1, dhigh);
        }

        if (ctx->itu_test_mode)
        {
            set_next_amp(ctx,(int16_t) (rlow << 1) );
            set_next_amp(ctx,(int16_t) (rhigh << 1) );
        }
        else
        {
            if (ctx->eight_k)
            {
              set_next_amp(ctx,(int16_t) (rlow << 1) );
            }
            else
            {
                /* Apply the receive QMF */
                for (i = 0;  i < 22;  i++)
                    ctx->x[i] = ctx->x[i + 2];
                ctx->x[22] = rlow + rhigh;
                ctx->x[23] = rlow - rhigh;

                xout1 = 0;
                xout2 = 0;
                for (i = 0;  i < 12;  i++)
                {
                    xout2 += ctx->x[2*i]*qmf_coeffs[i];
                    xout1 += ctx->x[2*i + 1]*qmf_coeffs[11 - i];
                }
                set_next_amp(ctx,(int16_t) (xout1 >> 11));
                set_next_amp(ctx,(int16_t) (xout2 >> 11));
            }
        }
    }
    return ctx->amp_idx;
}

int xcoder_encode_g722(xcoder_ctx_t* ctx, int len)
{
    static const int q6[32] =
    {
           0,   35,   72,  110,  150,  190,  233,  276,
         323,  370,  422,  473,  530,  587,  650,  714,
         786,  858,  940, 1023, 1121, 1219, 1339, 1458,
        1612, 1765, 1980, 2195, 2557, 2919,    0,    0
    };
    static const int iln[32] =
    {
         0, 63, 62, 31, 30, 29, 28, 27,
        26, 25, 24, 23, 22, 21, 20, 19,
        18, 17, 16, 15, 14, 13, 12, 11,
        10,  9,  8,  7,  6,  5,  4,  0
    };
    static const int ilp[32] =
    {
         0, 61, 60, 59, 58, 57, 56, 55,
        54, 53, 52, 51, 50, 49, 48, 47,
        46, 45, 44, 43, 42, 41, 40, 39,
        38, 37, 36, 35, 34, 33, 32,  0
    };
    static const int wl[8] =
    {
        -60, -30, 58, 172, 334, 538, 1198, 3042
    };
    static const int rl42[16] =
    {
        0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0
    };
    static const int ilb[32] =
    {
        2048, 2093, 2139, 2186, 2233, 2282, 2332,
        2383, 2435, 2489, 2543, 2599, 2656, 2714,
        2774, 2834, 2896, 2960, 3025, 3091, 3158,
        3228, 3298, 3371, 3444, 3520, 3597, 3676,
        3756, 3838, 3922, 4008
    };
    static const int qm4[16] =
    {
             0, -20456, -12896, -8968,
         -6288,  -4240,  -2584, -1200,
         20456,  12896,   8968,  6288,
          4240,   2584,   1200,     0
    };
    static const int qm2[4] =
    {
        -7408,  -1616,   7408,   1616
    };
    static const int qmf_coeffs[12] =
    {
           3,  -11,   12,   32, -210,  951, 3876, -805,  362, -156,   53,  -11,
    };
    static const int ihn[3] = {0, 1, 0};
    static const int ihp[3] = {0, 3, 2};
    static const int wh[3] = {0, -214, 798};
    static const int rh2[4] = {2, 1, 2, 1};

    int dlow;
    int dhigh;
    int el;
    int wd;
    int wd1;
    int ril;
    int wd2;
    int il4;
    int ih2;
    int wd3;
    int eh;
    int mih;
    int i;
    int j;
    /* Low and high band PCM from the QMF */
    int xlow;
    int xhigh;
    /* Even and odd tap accumulators */
    int sumeven;
    int sumodd;
    int ihigh;
    int ilow;
    int code;

    ctx->code_idx = 0;
    ctx->amp_idx = 0;

    xhigh = 0;
    for (j = 0;  j < len;  )
    {
        if (ctx->itu_test_mode)
        {
            xlow = 0;
            xhigh = get_next_amp(ctx) >> 1;
            j++;
        }
        else
        {
            if (ctx->eight_k)
            {
                xlow = get_next_amp(ctx) >>1;
                j++;
            }
            else
            {
                /* Apply the transmit QMF */
                /* Shuffle the buffer down */
                for (i = 0;  i < 22;  i++)
                    ctx->x[i] = ctx->x[i + 2];
                ctx->x[22] = get_next_amp(ctx);
                ctx->x[23] = get_next_amp(ctx);

                /* Discard every other QMF output */
                sumeven = 0;
                sumodd = 0;
                for (i = 0;  i < 12;  i++)
                {
                    sumodd += ctx->x[2*i]*qmf_coeffs[i];
                    sumeven += ctx->x[2*i + 1]*qmf_coeffs[11 - i];
                }
                xlow = (sumeven + sumodd) >> 14;
                xhigh = (sumeven - sumodd) >> 14;
            }
        }
        /* Block 1L, SUBTRA */
        el = saturate(xlow - ctx->band[0].s);

        /* Block 1L, QUANTL */
        wd = (el >= 0)  ?  el  :  -(el + 1);

        for (i = 1;  i < 30;  i++)
        {
            wd1 = (q6[i]*ctx->band[0].det) >> 12;
            if (wd < wd1)
                break;
        }
        ilow = (el < 0)  ?  iln[i]  :  ilp[i];

        /* Block 2L, INVQAL */
        ril = ilow >> 2;
        wd2 = qm4[ril];
        dlow = (ctx->band[0].det*wd2) >> 15;

        /* Block 3L, LOGSCL */
        il4 = rl42[ril];
        wd = (ctx->band[0].nb*127) >> 7;
        ctx->band[0].nb = wd + wl[il4];
        if (ctx->band[0].nb < 0)
            ctx->band[0].nb = 0;
        else if (ctx->band[0].nb > 18432)
            ctx->band[0].nb = 18432;

        /* Block 3L, SCALEL */
        wd1 = (ctx->band[0].nb >> 6) & 31;
        wd2 = 8 - (ctx->band[0].nb >> 11);
        wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
        ctx->band[0].det = wd3 << 2;

        block4(ctx, 0, dlow);

        if (ctx->eight_k)
        {
            /* Just leave the high bits as zero */
            code = (0xC0 | ilow) >> (8 - ctx->bits_per_sample);
        }
        else
        {
            /* Block 1H, SUBTRA */
            eh = saturate(xhigh - ctx->band[1].s);

            /* Block 1H, QUANTH */
            wd = (eh >= 0)  ?  eh  :  -(eh + 1);
            wd1 = (564*ctx->band[1].det) >> 12;
            mih = (wd >= wd1)  ?  2  :  1;
            ihigh = (eh < 0)  ?  ihn[mih]  :  ihp[mih];

            /* Block 2H, INVQAH */
            wd2 = qm2[ihigh];
            dhigh = (ctx->band[1].det*wd2) >> 15;

            /* Block 3H, LOGSCH */
            ih2 = rh2[ihigh];
            wd = (ctx->band[1].nb*127) >> 7;
            ctx->band[1].nb = wd + wh[ih2];
            if (ctx->band[1].nb < 0)
                ctx->band[1].nb = 0;
            else if (ctx->band[1].nb > 22528)
                ctx->band[1].nb = 22528;

            /* Block 3H, SCALEH */
            wd1 = (ctx->band[1].nb >> 6) & 31;
            wd2 = 10 - (ctx->band[1].nb >> 11);
            wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
            ctx->band[1].det = wd3 << 2;

            block4(ctx, 1, dhigh);
            code = ((ihigh << 6) | ilow) >> (8 - ctx->bits_per_sample);
        }

        if (ctx->packed)
        {
            /* Pack the code bits */
            ctx->out_buffer |= (code << ctx->out_bits);
            ctx->out_bits += ctx->bits_per_sample;
            if (ctx->out_bits >= 8)
            {
                set_next_code(ctx, (uint8_t) (ctx->out_buffer & 0xFF));
                ctx->out_bits -= 8;
                ctx->out_buffer >>= 8;
            }
        }
        else
        {
            set_next_code(ctx,  (uint8_t) code);
        }
    }
    return ctx->code_idx;
}

int xcoder_decode_ulaw(xcoder_ctx_t* ctx, int len)
{
  static int exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
	int sign, exponent, mantissa, sample;

  uint8_t number;
  ctx->amp_idx = 0;
  ctx->code_idx = 0;

  for(int i=0; i < len; i++)
  {

    number = ~((uint8_t)get_next_code(ctx));
    sign = (number & 0x80);
    exponent = (number >> 4) & 0x07;
    mantissa = number & 0x0F;
    sample = exp_lut[exponent] + (mantissa << (exponent + 3));
    if (sign != 0) sample = -sample;

    set_next_amp(ctx, sample);
  }
  return len; //TODO actually track for fifo overflow/underflow
}

int xcoder_encode_ulaw(xcoder_ctx_t* ctx, int len)
{
  const uint16_t MULAW_MAX = 0x1FFF;
  const uint16_t MULAW_BIAS = 33;
  uint16_t mask = 0x1000;
  int16_t number;
  uint8_t sign = 0;
  uint8_t position = 12;
  uint8_t lsb = 0;
  int8_t code;

  ctx->amp_idx = 0;
  ctx->code_idx = 0;

  for(int i=0; i< len; i++)
  {
    mask = 0x1000;
    number = get_next_amp(ctx);
    sign =0;
    position = 12;
    lsb = 0;
    if (number < 0)
    {
      number = -number;
      sign = 0x80;
    }
    number += MULAW_BIAS;
    if (number > MULAW_MAX)
    {
      number = MULAW_MAX;
    }
    for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--)
    ;
    lsb = (number >> (position - 4)) & 0x0f;
    code  = ~(sign | ((position - 5) << 4) | lsb);
    set_next_code(ctx, (uint8_t) code );
  }

  return len; //TODO actually track for fifo overflow/underflow
}