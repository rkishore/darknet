/**************************************************************
 * dfxp.c - DFXP file format functions for igolgi software
 *
 * Copyright (C) 2009-2013 Igolgi Inc.
 * 
 * Confidential and Proprietary Source Code
 *
 * Authors: Joshua Horvath (joshua.horvath@igolgi.com)
 *          David Anderson (david.anderson@igolgi.com)
 **************************************************************/

#include <stdio.h>
#include <string.h>
#include "dfxp.h"
#include "common.h"

//#define ROLL_UP
static int black_background= 0;

int dfxp_write_header(FILE *fp, char *channel_id)
{
    if (fp == NULL)
    {
        return -1;
    }

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(fp, "<tt xml:lang=\"EN\" xmlns=\"http://www.w3.org/2006/10/ttaf1\">\n");
    fprintf(fp, "<head>\n");
    fprintf(fp, "<metadata xmlns:ttm=\"http://www.w3.org/2006/10/ttaf1#metadata\">\n");
    fprintf(fp, "<ttm:title>%s</ttm:title>\n", channel_id);
    fprintf(fp, "</metadata>\n");
    fprintf(fp, "</head>\n");
    fprintf(fp, "<body region=\"subtitleArea\">\n");
    fprintf(fp, "<div>\n");

    fflush(fp);

    return 0;
}

int dfxp_write_caption(FILE *fp, char *caption, 
		       double begin_ts,
                       double end_ts, 
		       double abs_start_time,
		       uint8_t *background_music,
		       uint8_t *quit_without_hyp,
		       int *num_blocks_to_skip)
{
    int ret = 0;
    char *ptr = caption;
    char *line;
    const char *delim = "\n";
    int64_t start_ts;
    int64_t cur_ts = 0;
    char *saveptr;

    if (fp == NULL)
    {
        return -1;
    }

    start_ts = (int64_t)begin_ts;

    if (caption != NULL) {

      // Break up multi-line captions into individual lines
      line = strtok_r(caption, delim, &saveptr);
      while (line)
	{
	  ptr = line;

	  // remove any leading whitespace
	  while (*ptr == ' ')
	    {
	      ptr++;
	      if (*ptr == '\0')
		{
		  break;
		}
	    }

	  ret = fprintf(fp, "<p xml:id=\"s%"PRId64"\" begin=\"%0.3fs\" end=\"%0.3fs\" timed_out=\"%d\">%s</p>\n", 
			start_ts, begin_ts, end_ts, *background_music, ptr);

	  /* ret = fprintf(fp, "<p xml:id=\"s%u\" start_time=\"%s\" end_time=\"%s\" absolute_start_time=\"%s\" background_music=\"%d\" skip_block=\"%d\" num_blocks_to_skip=\"%d\">%s</p>\n", 
			(unsigned int)begin_ts,
			start_time_string, 
			end_time_string, 
			abs_start_time_string, 
			*background_music,
			*quit_without_hyp,
			*num_blocks_to_skip,
			ptr);
	  */

	  if (ret < 0)
	    {
	      return ret;
	    }

	  // For any subsequent lines, the start time is set to the end time
	  start_ts = end_ts;
	  line = strtok_r(NULL, delim, &saveptr);
	}

    } else {

      ret = fprintf(fp, "<p xml:id=\"s%"PRId64"\" begin=\"%0.3fs\" end=\"%0.3fs\" timed_out=\"%d\">NULL</p>\n", 
		    start_ts, begin_ts, end_ts, *background_music);
      /* ret = fprintf(fp, "<p xml:id=\"s%u\" start_time=\"%s\" end_time=\"%s\" absolute_start_time=\"%s\" background_music=\"%d\" skip_block=\"%d\" num_blocks_to_skip=\"%d\">%s</p>\n", 
		    (unsigned int)begin_ts, 
		    start_time_string, 
		    end_time_string, 
		    abs_start_time_string, 
		    *background_music,
		    *quit_without_hyp,
		    *num_blocks_to_skip,
		    ptr);
      */
    }
    
    fflush(fp);

    return ret;
}

int dfxp_write_xds(FILE *fp, char *xds_info, unsigned int begin_ts,
                   unsigned int end_ts)
{
    int ret;
    char *ptr = xds_info;

    if (fp == NULL)
    {
        return -1;
    }

    if (xds_info == NULL)
    {
        return -1;
    }

    // remove any leading whitespace
    while (*ptr++ == ' ')
    {
        ptr++;

        if (*ptr == '\0')
        {
            break;
        }
    }

    ret = fprintf(fp, "<p xml:id=\"s%u\" begin=\"%us\" end=\"%us\">%s</p>\n", begin_ts, begin_ts, end_ts, xds_info);

    fflush(fp);

    return ret;
}

int dfxp_write_footer(FILE *fp)
{
    if (fp == NULL)
    {
        return -1;
    }

    fprintf(fp, "</div>\n");
    fprintf(fp, "</body>\n");
    fprintf(fp, "</tt>\n");

    fflush(fp);

    return 0;
}

int dfxp_write_header_buf(char* buf, char *channel_id, int font_size, int opaque_level, int rollup_captions)
{
    int written = 0;
    //int black_background = 0;

#if 0
    opaque_level = -1;
#endif

    if (opaque_level == -1) {
        black_background = 1;
        opaque_level = 0;
    }
    
    if (!buf)
    {
        return -1;
    }

    /*
    written += sprintf(buf, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    written += sprintf(buf+written, "<tt xml:lang=\"en\" xmlns=\"http://www.w3.org/2006/10/ttaf1\">\n");
    written += sprintf(buf+written, "<head>\n");    
    written += sprintf(buf+written, "<metadata>\n");
    written += sprintf(buf+written, "  <ttm:title xmlns:ttm=\"http://www.w3.org/2006/10/ttaf1#metadata\" />\n");
    written += sprintf(buf+written, "  <ttm:desc xmlns:ttm=\"http://www.w3.org/2006/10/ttaf1#metadata\" />\n");
    written += sprintf(buf+written, "  <ttm:copyright xmlns:ttm=\"http://www.w3.org/2006/10/ttaf1#metadata\" />\n");
    written += sprintf(buf+written, "</metadata>\n");
    written += sprintf(buf+written, "<styling>\n");   
    //written += sprintf(buf+written, "  <style xml:id=\"backgroundStyle\" p5:fontFamily=\"proportionalSansSerif\" p5:fontSize=\"16px\" p5:textAlign=\"center\" p5:origin=\"10% 85%\" p5:extent=\"80% 10%\" p5:backgroundColor=\"rgba(0,0,0,100)\" p5:displayAlign=\"center\" xmlns:p5=\"http://www.w3.org/2006/10/ttaf1#styling\" />\n");
    //written += sprintf(buf+written, "  <style xml:id=\"speakerStyle\" p5:style=\"backgroundStyle\" p6:color=\"white\" p6:backgroundColor=\"transparent\" xmlns:p6=\"http://www.w3.org/2006/10/ttaf1#styling\" xmlns:p5=\"http://www.w3.org/2006/10/ttaf1\" />\n");
    written += sprintf(buf+written, "</styling>\n");
    written += sprintf(buf+written, "<layout>\n");
    written += sprintf(buf+written, "  <region xml:id=\"speaker\" p5:style=\"speakerStyle\" p6:zIndex=\"1\" xmlns:p6=\"http://www.w3.org/2006/10/ttaf1#styling\" xmlns:p5=\"http://www.w3.org/2006/10/ttaf1\" />\n");
    written += sprintf(buf+written, "  <region xml:id=\"background\" p5:style=\"backgroundStyle\" p6:zIndex=\"0\" xmlns:p6=\"http://www.w3.org/2006/10/ttaf1#styling\" xmlns:p5=\"http://www.w3.org/2006/10/ttaf1\" />\n");
    written += sprintf(buf+written, "</layout>\n");
    written += sprintf(buf+written, "</head>\n");
    written += sprintf(buf+written, "<body region=\"subtitleArea\">\n");
    written += sprintf(buf+written, "<div>\n");
    */
    
    written += sprintf(buf+written, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    written += sprintf(buf+written, "<tt xml:lang=\"en\" xmlns=\"http://www.w3.org/ns/ttml\" xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\">\n");
    written += sprintf(buf+written, "  <head>\n");
    written += sprintf(buf+written, "    <metadata>\n");
    written += sprintf(buf+written, "      <ttm:title></ttm:title>\n");
    written += sprintf(buf+written, "      <ttm:desc></ttm:desc>\n");
    written += sprintf(buf+written, "      <ttm:copyright></ttm:copyright>\n");
    written += sprintf(buf+written, "    </metadata>\n");
    written += sprintf(buf+written, "    <styling>\n");

    //<style tts:wrapOption="noWrap"/>
    // changing the 85%% to 75%% moved it up
    // changing the 15%% to 25%% moved the text down - cut it off

    // was 10% now 25%
    
    //// changed 80%% to 70%% - cut text off on the right
#if 1
    written += sprintf(buf+written, "      <style tts:backgroundColor=\"rgba(0,0,0,%d)\" tts:wrapOption=\"wrap\" tts:displayAlign=\"center\" tts:origin=\"10%% 75%%\" tts:extent=\"80%% 25%%\" tts:fontWeight=\"bold\" tts:fontFamily=\"proportionalSansSerif\" tts:fontSize=\"%dpx\" tts:textAlign=\"center\" xml:id=\"backgroundStyle\"></style>\n", opaque_level, font_size);
#else
    written += sprintf(buf+written, "      <style tts:backgroundColor=\"rgba(0,0,0,%d)\" tts:wrapOption=\"wrap\" tts:displayAlign=\"center\" tts:origin=\"10%% 85%%\" tts:extent=\"80%% 15%%\" tts:fontWeight=\"bold\" tts:fontFamily=\"proportionalSansSerif\" tts:fontSize=\"%dpx\" tts:textAlign=\"center\" xml:id=\"backgroundStyle\"></style>\n", opaque_level, font_size);
#endif


    //  written += sprintf(buf+written, "      <style tts:backgroundColor=\"rgba(0,0,0,100)\" tts:displayAlign=\"center\" tts:origin=\"10% 85%\" tts:extent=\"80% 10%\" tts:fontFamily=\"proportionalSansSerif\" tts:fontSize=\"12px\" tts:textAlign=\"center\" xml:id=\"backgroundStyle\"></style>\n");

    //    written += sprintf(buf+written, "      <style tts:backgroundColor=\"rgba(0,0,0,100)\" tts:displayAlign=\"center\" tts:origin=\"50px 440px\" tts:extent=\"700px 42px\" tts:fontFamily=\"proportionalSansSerif\" tts:fontSize=\"12px\" tts:textAlign=\"center\" xml:id=\"backgroundStyle\"></style>\n");

    //written += sprintf(buf+written, "      <style tts:backgroundColor=\"rgba(0,0,0,100)\" tts:displayAlign=\"center\" tts:origin=\"10% 440px\" tts:extent=\"80% 42px\" tts:fontFamily=\"proportionalSansSerif\" tts:fontSize=\"12px\" tts:textAlign=\"center\" xml:id=\"backgroundStyle\"></style>\n");

    written += sprintf(buf+written, "      <style style=\"backgroundStyle\" tts:backgroundColor=\"transparent\" tts:color=\"white\" xml:id=\"speakerStyle\"></style>\n");
 
    /*
    written += sprintf(buf+written, "      <style tts:backgroundColor=\"rgba(0,0,0,100)\" tts:displayAlign=\"center\" tts:extent=\"80% 50px\" tts:fontFamily=\"proportionalSansSerif\" tts:fontSize=\"14px\" tts:origin=\"10% 425px\" tts:textAlign=\"center\" xml:id=\"backgroundStyle\"></style>\n");
    written += sprintf(buf+written, "      <style style=\"backgroundStyle\" tts:backgroundColor=\"transparent\" tts:color=\"white\" xml:id=\"speakerStyle\"></style>\n");
    */

    written += sprintf(buf+written, "    </styling>\n");
    written += sprintf(buf+written, "    <layout>\n");
    written += sprintf(buf+written, "      <region style=\"speakerStyle\" tts:zIndex=\"1\" xml:id=\"speaker1\"></region>\n");
    written += sprintf(buf+written, "      <region style=\"backgroundStyle\" tts:zIndex=\"0\" xml:id=\"background\"></region>\n");
    written += sprintf(buf+written, "    </layout>\n");
    written += sprintf(buf+written, "  </head>\n");
    written += sprintf(buf+written, "  <body>\n");
    if (rollup_captions) {
        written += sprintf(buf+written, "    <div region=\"speaker1\" xml:lang=\"en\">\n");
    } else {
        written += sprintf(buf+written, "    <div style=\"default\" xml:lang=\"en\">\n");
    }

    //fprintf(stderr, "[dfxp] open\n");
    return written;
}

int dfxp_write_caption_buf(char* buf, char *caption, double begin_ts, double end_ts, int rollup_captions)
{
    int ret;
    char *ptr = caption, *endptr;
    char *line;
    const char *delim = "\n";
    double start_ts;
    int start_h, start_m, start_s, start_ms;
    int end_h, end_m, end_s, end_ms;
    char *saveptr;
    int written = 0;
    int current_pop = 1;
    //int black_background = 0;

    if (!buf) {
        return -1;
    }

    if (caption == NULL)
    {
        return -1;
    }

    start_ts = begin_ts;

    // Break up multi-line captions into individual lines
    /*
    line = strtok_r(caption, delim, &saveptr);
    while (line)
    {
        ptr = line;
        */
        ptr = caption;
        endptr = caption + strlen(caption) - 1;

        // remove any leading whitespace
        while (*ptr == ' ' || *ptr == '\n')
        {
            ptr++;
            if (*ptr == '\0')
            {
                break;
            }
        }
        
        // remove trailing whitespace
        while ((*endptr == ' ' || *endptr == '\n') && endptr > ptr)
        {
            --endptr;
        }               
        *(endptr+1) = '\0';
        
        start_h = (int)start_ts / SECONDS_IN_HOUR;
        start_m = (int)(start_ts / 60) % 60;
        start_s = (int)start_ts % 60;
        start_ms = (int)(1000.0*start_ts) % 1000;
        end_h = (int)end_ts / SECONDS_IN_HOUR;
        end_m = (int)(end_ts / 60) % 60;
        end_s = (int)end_ts % 60;
        end_ms = (int)(1000.0*end_ts) % 1000;
        
        /*
        written += sprintf(buf+written, "<p p4:region=\"speaker\" p4:begin=\"%02d:%02d:%02d.%03d\" p4:end=\"%02d:%02d:%02d.%03d\" xmlns:p4=\"http://www.w3.org/2006/10/ttaf1\">%s</p>\n", 
            begin_ts*1000.0, start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, ptr);
        */
        /*
        written += sprintf(buf+written, "<p xml:id=\"s%f.\" begin=\"%02d:%02d:%02d.%03d\" end=\"%02d:%02d:%02d.%03d\">%s</p>\n", 
            begin_ts*1000.0, start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, ptr);
            */
            
        //fprintf(stderr, "\n\n\n[dba][dfxp] %0.03f %0.03f %s\n\n\n", begin_ts, end_ts, caption);
	//written += sprintf(buf+written, "    <div style=\"default\" xml:lang=\"en\">\n");
	if (rollup_captions) {
	    if (black_background) {
	        written += sprintf(buf+written, "      <p tts:backgroundColor=\"black\" begin=\"%02d:%02d:%02d.%03d\" end=\"%02d:%02d:%02d.%03d\">%s</p>\n", 
				   start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, ptr);  
	    } else {
	        written += sprintf(buf+written, "      <p begin=\"%02d:%02d:%02d.%03d\" end=\"%02d:%02d:%02d.%03d\">%s</p>\n", 
				   start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, ptr); 
	    }
	} else {
	    if (black_background) {
	        written += sprintf(buf+written, "      <p begin=\"%02d:%02d:%02d.%03d\" end=\"%02d:%02d:%02d.%03d\" tts:backgroundColor=\"black\" region=\"speaker%d\">%s</p>\n", start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, current_pop, ptr);
	    } else {
	        written += sprintf(buf+written, "      <p begin=\"%02d:%02d:%02d.%03d\" end=\"%02d:%02d:%02d.%03d\" region=\"speaker%d\">%s</p>\n", start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, current_pop, ptr);
	    }
	}

	//tts:backgroundColor=\"black\"
	//written += sprintf(buf+written, "    </div>\n");
        
	//written += sprintf(buf+written, "      <p begin=\"%02d:%02d:%02d.%03d\" end=\"%02d:%02d:%02d.%03d\" region=\"pop%d\">%s</p>\n", 
	//		   start_h, start_m, start_s, start_ms, end_h, end_m, end_s, end_ms, current_pop, ptr);
	current_pop++;

        // For any subsequent lines, the start time is set to the end time
        start_ts = end_ts;
        /*
        line = strtok_r(NULL, delim, &saveptr);
    }
    */

    return written;
}

int dfxp_write_footer_buf(char* buf)
{
    int written = 0;
    if (!buf) {
        return -1;
    }

    written += sprintf(buf+written, "    </div>\n");
    written += sprintf(buf+written, "  </body>\n");
    written += sprintf(buf+written, "</tt>\n");

    //fprintf(stderr, "[dfxp] close\n");    
    
    return written;
}

