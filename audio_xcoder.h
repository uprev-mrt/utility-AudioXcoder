/*
 * xcoder.h
 *
/**
 * @file audio_xcoder.h
 * @author Jason Berger
 * @brief 
 * @version 0.1
 * @date 2020-02-04
 * 
 */
#pragma once

#ifdef __cplusplus
 extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "Utilities/Fifo/fifo.h"

#define XCODER_RATE_48k 48000
#define XCODER_RATE_56k 56000
#define XCODER_RATE_64k 64000
#define XCODER_RATE_NONE 0

#define XCODER_OPT_NONE	0x00
#define XCODER_OPT_PACKED 0x02
#define XCODER_OPT_8k		0x01

#define XCODER_PORT_AMP   0x01
#define XCODER_PORT_CODE  0x02


typedef struct
    {
        int s;
        int sp;
        int sz;
        int r[3];
        int a[3];
        int ap[3];
        int p[3];
        int d[7];
        int b[7];
        int bp[7];
        int sg[7];
        int nb;
        int det;
    } xcoder_band_t;

typedef struct {

	bool itu_test_mode;
	bool 		packed;
	bool 		eight_k;
	uint8_t 	bits_per_sample;
  /*! Signal history for the QMF */
  int x[24];
	xcoder_band_t band[2];

	uint32_t 	in_buffer;
	int		 	in_bits;
	uint32_t 	out_buffer;
	int 		out_bits;


	fifo_t* 	code_fifo;
	fifo_t*		amp_fifo;

  int16_t* amp_buffer;
  uint32_t amp_idx;
  uint8_t* code_buffer;
  uint32_t code_idx;

  //properties for processing pcm/amp data
  uint16_t amp_mask;
  int     amp_offset;       //offset signal
  int     amp_averaging;    //how many samples to do running avg on
  int16_t amp_window[50];   //window of vals for running avg
  int16_t amp_win_idx;  //idx in window
  int32_t amp_sum;       //sum of window


}xcoder_ctx_t;


void xcoder_ctx_init(xcoder_ctx_t* ctx, int rate, uint8_t options);
void xcoder_set_fifo(xcoder_ctx_t* ctx, fifo_t* fifo, uint8_t port);
void xcoder_set_buffer(xcoder_ctx_t* ctx, void* buffer, uint8_t port);
int xcoder_encode_g722(xcoder_ctx_t* ctx, int len);
int xcoder_decode_g722(xcoder_ctx_t* ctx, int len);
int xcoder_decode_ulaw(xcoder_ctx_t* ctx, int len);
int xcoder_encode_ulaw(xcoder_ctx_t* ctx, int len);

#ifdef __cplusplus
}
#endif
