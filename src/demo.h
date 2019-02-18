#ifndef DEMO
#define DEMO

#include "image.h"
void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
    int frame_skip, char *prefix, char *out_filename, int mjpeg_port, int json_port, int dont_show, int ext_output, const char *json_filename);
#ifdef CLASSIFYAPP
#include "classifyapp.h"
void process_video(struct prep_network_info *prep_netinfo, char *filename, float thresh, float hier_thresh,
		   char *output_img_prefix, char *out_filename, int dont_show, char *json_filename, int http_stream_port, int frame_skip);
#endif
#endif
