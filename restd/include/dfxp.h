/**************************************************************
 * dfxp.h - DFXP file format functions for igolgi software
 *
 * Copyright (C) 2009 Igolgi Inc.
 * 
 * Confidential and Proprietary Source Code
 *
 * Authors: Joshua Horvath (joshua.horvath@igolgi.com)
 **************************************************************/

#ifndef DFXP_H 
#define DFXP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int dfxp_write_header(FILE *fp, char *channel_id);
int dfxp_write_caption(FILE *fp, char *caption, 
		       double begin_ts,
                       double end_ts, 
		       double abs_start_time,
		       uint8_t *background_music,
		       uint8_t *quit_without_hyp,
		       int *num_blocks_to_skip);
int dfxp_write_xds(FILE *fp, char *xds_info, unsigned int begin_ts,
                   unsigned int end_ts);
int dfxp_write_footer(FILE *fp);

  int dfxp_write_header_buf(char* buf, char *channel_id, int font_size, int opaque_level, int rollup_captions);
  int dfxp_write_caption_buf(char* buf, char *caption, double begin_ts, double end_ts, int rollup_captions);
int dfxp_write_footer_buf(char* buf);

#ifdef __cplusplus
}
#endif

#endif /* DFXP_H */
