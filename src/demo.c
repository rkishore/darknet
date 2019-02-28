#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#ifdef WIN32
#include <time.h>
#include <winsock.h>
#include "gettimeofday.h"
#else
#include <sys/time.h>
#endif

#define FRAMES 3

#ifdef OPENCV
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/core/version.hpp"
#ifndef CV_VERSION_EPOCH
#include "opencv2/videoio/videoio_c.h"
#endif
#include "http_stream.h"

// For JSON output
#include "cJSON.h"

#include <syslog.h>
#include "classifyapp.h"
#include "restful.h"

image get_image_from_stream(CvCapture *cap);

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static int nboxes = 0;
static detection *dets = NULL;

static network net;
static image in_s ;
static image det_s;
static CvCapture * cap;
static int cpp_video_capture = 0;
static float fps = 0;
static float demo_thresh = 0;
static int demo_ext_output = 0;
static long long int frame_id = 0;
static int demo_json_port = -1;

static float *predictions[FRAMES];
static int demo_index = 0;
static image images[FRAMES];
static IplImage* ipl_images[FRAMES];
static float *avg;

void draw_detections_cv_v3(IplImage* show_img, detection *dets, int num, float thresh, char **names, image **alphabet, int classes, int ext_output);
void show_image_cv_ipl(IplImage *disp, const char *name);
void save_cv_png(IplImage *img, const char *name);
void save_cv_jpg(IplImage *img, const char *name);
image get_image_from_stream_resize(CvCapture *cap, int w, int h, int c, IplImage** in_img, int cpp_video_capture, int dont_close);
image get_image_from_stream_letterbox(CvCapture *cap, int w, int h, int c, IplImage** in_img, int cpp_video_capture, int dont_close);
int get_stream_fps(CvCapture *cap, int cpp_video_capture);
int get_stream_frame_count(CvCapture *cap, int cpp_video_capture);
int close_stream(CvCapture *cap, int cpp_video_capture);
IplImage* in_img;
IplImage* det_img;
IplImage* show_img;

static int flag_exit;
static int letter_box = 0;

cJSON *per_frame_json = NULL, *regions_json = NULL;
static int local_frame_count = -1, src_fps = 25;

void *fetch_in_thread(void *ptr)
{
    //in = get_image_from_stream(cap);
    int dont_close_stream = 0;    // set 1 if your IP-camera periodically turns off and turns on video-stream
    if(letter_box)
        in_s = get_image_from_stream_letterbox(cap, net.w, net.h, net.c, &in_img, cpp_video_capture, dont_close_stream);
    else
        in_s = get_image_from_stream_resize(cap, net.w, net.h, net.c, &in_img, cpp_video_capture, dont_close_stream);
    if(!in_s.data){
        //error("Stream closed.");
        syslog(LOG_INFO, "= Stream closed, %s:%d", __FILE__, __LINE__);
        flag_exit = 1;
        return EXIT_FAILURE;
    }
    //in_s = resize_image(in, net.w, net.h);

    return 0;
}

void *detect_in_thread(void *ptr)
{
    layer l = net.layers[net.n-1];
    float *X = det_s.data;
    float *prediction = network_predict(net, X);

    memcpy(predictions[demo_index], prediction, l.outputs*sizeof(float));
    mean_arrays(predictions, FRAMES, l.outputs, avg);
    l.output = avg;

    free_image(det_s);

    ipl_images[demo_index] = det_img;
    det_img = ipl_images[(demo_index + FRAMES / 2 + 1) % FRAMES];
    demo_index = (demo_index + 1) % FRAMES; // takes values between 0-2 as FRAMES = 3

    if (letter_box)
        dets = get_network_boxes(&net, in_img->width, in_img->height, demo_thresh, demo_thresh, 0, 1, &nboxes, 1); // letter box
    else
        dets = get_network_boxes(&net, net.w, net.h, demo_thresh, demo_thresh, 0, 1, &nboxes, 0); // resized

    return 0;
}

double get_wall_time()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

void add_info_to_per_frame_json(char *ptr, bool initial_call)
{
  
  cJSON *regions_array_json = NULL, *region_json = NULL, *bbox_json = NULL;
  char tmp_char_buff[128];
  int i,j, num_labels_detected = 0, cur_milliseconds = 0;
  int left, right, top, bot;
  box b;
  
  if (initial_call == true)
    {
      /* 
	 {
	 "/home/igolgi/shared/Day_Vig6_Open1_HDLL_HD380.mp4": {
	 }
	 }
       */
      cJSON_AddItemToObject(per_frame_json, ptr, regions_json=cJSON_CreateObject());

    }

  for(i = 0; i < nboxes; ++i){
    for(j = 0; j < demo_classes; ++j){
      if (dets[i].prob[j] > demo_thresh){
	num_labels_detected += 1;
      }
    }
  }
  
  if (num_labels_detected > 0)
    {

      cur_milliseconds = local_frame_count * src_fps;
      syslog(LOG_DEBUG, "= local_frame_count: %d, src_fps = %d, cur_ms: %d, %s:%d", local_frame_count, src_fps, cur_milliseconds, __FILE__, __LINE__);
      memset(tmp_char_buff, 0, 128);
      sprintf(tmp_char_buff, "%d", cur_milliseconds);
      /*
	{
	"/home/igolgi/shared/Day_Vig6_Open1_HDLL_HD380.mp4": {
	
	"5432" : [
	],
	
	}
	}
       */
      regions_array_json = cJSON_AddArrayToObject(regions_json, tmp_char_buff);
      
      for(i = 0; i < nboxes; ++i){
	for(j = 0; j < demo_classes; ++j){
	  if (dets[i].prob[j] > demo_thresh){

	    /*
	      {
	      "/home/igolgi/shared/Day_Vig6_Open1_HDLL_HD380.mp4": {
	      
	      "5432" : [
	      {
	    
	      },
	      ],
	      }
	      }
	    */
	    region_json = cJSON_CreateObject();

	    /*
	      {
	      "/home/igolgi/shared/Day_Vig6_Open1_HDLL_HD380.mp4": {
	      
	      "5432" : [
	      {
	      "confidence": 98.0,
	      "timestamp": 5432,
	      "category": "vehicle",
	      "bounding_box": {
	      },
	      ],
	      }
	      }
	    */
	    cJSON_AddNumberToObject(region_json, "confidence", dets[i].prob[j]);
	    cJSON_AddNumberToObject(region_json, "timestamp", cur_milliseconds);
	    cJSON_AddStringToObject(region_json, "category", demo_names[j]);
	    cJSON_AddItemToObject(region_json, "bounding_box", bbox_json=cJSON_CreateObject());
	    
	    b = dets[i].bbox;
	    
	    // results_info->confidence[k] = dets[i].prob[j]*100;
	    
	    left  = (b.x-b.w/2.)*det_img->width;
	    right = (b.x+b.w/2.)*det_img->width;
	    top   = (b.y-b.h/2.)*det_img->height;
	    bot   = (b.y+b.h/2.)*det_img->height;
	    
	    if(left < 0) left = 0;
	    if(right > det_img->width-1) right = det_img->width-1;
	    if(top < 0) top = 0;
	    if(bot > det_img->height-1) bot = det_img->height-1;

	    /*
	      {
	      "/home/igolgi/shared/Day_Vig6_Open1_HDLL_HD380.mp4": {
	      
	      "5432" : [
	      {
	      "confidence": 98.0,
	      "timestamp": 5432,
	      "category": "vehicle",
	      "bounding_box": {
	      "height": 22,
	      "width": 67,
	      "x": 1527,
	      "y": 643
	      },
	      ],
	      }
	      }
	    */

	    cJSON_AddNumberToObject(bbox_json, "height", (int)(b.h * det_img->height));
	    cJSON_AddNumberToObject(bbox_json, "width", (int)(b.w * det_img->width));
	    cJSON_AddNumberToObject(bbox_json, "x", (int)(left));
	    cJSON_AddNumberToObject(bbox_json, "y", (int)(top));
	    
	    cJSON_AddItemToArray(regions_array_json, region_json);
	    
	  }
	}
      }
    }

  
}

void add_info_to_per_frame_json_old(char *ptr)
{
  cJSON *regions_json = NULL, *regions_array_json = NULL, *region_json = NULL;
  char tmp_char_buff[128];
  int i,j, num_labels_detected = 0;
  int left, right, top, bot;
  box b;
  
  memset(tmp_char_buff, 0, 128);
  if (ptr != NULL)
    sprintf(tmp_char_buff, "%s/%08d.jpg", (char *)ptr, local_frame_count);
  else
    sprintf(tmp_char_buff, "%08d.jpg", local_frame_count);
  
  cJSON_AddItemToObject(per_frame_json, tmp_char_buff, regions_json=cJSON_CreateObject());
  
  for(i = 0; i < nboxes; ++i){
    for(j = 0; j < demo_classes; ++j){
      if (dets[i].prob[j] > demo_thresh){
	num_labels_detected += 1;
      }
    }
  }
  
  if (num_labels_detected > 0)
    {
      
      regions_array_json = cJSON_AddArrayToObject(regions_json, "regions");
      
      for(i = 0; i < nboxes; ++i){
	for(j = 0; j < demo_classes; ++j){
	  if (dets[i].prob[j] > demo_thresh){
	    
	    region_json = cJSON_CreateObject();
	    
	    cJSON_AddStringToObject(region_json, "category", demo_names[j]);
	    
	    b = dets[i].bbox;
	    
	    // results_info->confidence[k] = dets[i].prob[j]*100;
	    
	    left  = (b.x-b.w/2.)*det_img->width;
	    right = (b.x+b.w/2.)*det_img->width;
	    top   = (b.y-b.h/2.)*det_img->height;
	    bot   = (b.y+b.h/2.)*det_img->height;
	    
	    if(left < 0) left = 0;
	    if(right > det_img->width-1) right = det_img->width-1;
	    if(top < 0) top = 0;
	    if(bot > det_img->height-1) bot = det_img->height-1;
	    
	    cJSON_AddNumberToObject(region_json, "height", (int)(b.h * det_img->height));
	    cJSON_AddNumberToObject(region_json, "width", (int)(b.w * det_img->width));
	    cJSON_AddNumberToObject(region_json, "x", (int)(left));
	    cJSON_AddNumberToObject(region_json, "y", (int)(top));
	    
	    cJSON_AddItemToArray(regions_array_json, region_json);
	    
	  }
	}
      }
    }

  return;
  
}

void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
    int frame_skip, char *prefix, char *out_filename, int mjpeg_port, int json_port, int dont_show, int ext_output, const char *json_filename)
{
    in_img = det_img = show_img = NULL;
    //skip = frame_skip;
    image **alphabet = load_alphabet();
    int delay = frame_skip;
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_ext_output = ext_output;
    demo_json_port = json_port;
    printf("Demo\n");
    net = parse_network_cfg_custom(cfgfile, 1, 0);    // set batch=1
    if(weightfile){
        load_weights(&net, weightfile);
    }
    //set_batch_network(&net, 1);
    fuse_conv_batchnorm(net);
    calculate_binary_weights(net);
    srand(2222222);

    if(filename){
        printf("video file: %s\n", filename);
        cpp_video_capture = 1;
        cap = get_capture_video_stream(filename);
    }else{
        printf("Webcam index: %d\n", cam_index);
        cpp_video_capture = 1;
        cap = get_capture_webcam(cam_index);
    }

    if (!cap) {
#ifdef WIN32
        printf("Check that you have copied file opencv_ffmpeg340_64.dll to the same directory where is darknet.exe \n");
#endif
        error("Couldn't connect to webcam.\n");
    }

    layer l = net.layers[net.n-1];
    int j;

    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) images[j] = make_image(1,1,3);

    if (l.classes != demo_classes) {
        printf("Parameters don't match: in cfg-file classes=%d, in data-file classes=%d \n", l.classes, demo_classes);
        getchar();
        exit(0);
    }

    if (json_filename)
      per_frame_json = cJSON_CreateObject();

    flag_exit = 0;

    pthread_t fetch_thread;
    pthread_t detect_thread;

    fetch_in_thread(0);
    det_img = in_img;
    det_s = in_s;

    fetch_in_thread(0);
    detect_in_thread(0);
    det_img = in_img;
    det_s = in_s;

    for(j = 0; j < FRAMES/2; ++j){
        fetch_in_thread(0);
        detect_in_thread(0);
        det_img = in_img;
        det_s = in_s;
    }

    if(!prefix && !dont_show){
        cvNamedWindow("Demo", CV_WINDOW_NORMAL);
        cvMoveWindow("Demo", 0, 0);
        cvResizeWindow("Demo", 1352, 1013);
    }

    CvVideoWriter* output_video_writer = NULL;    // cv::VideoWriter output_video;
    if (out_filename && !flag_exit)
    {
        CvSize size;
        size.width = det_img->width, size.height = det_img->height;
        int src_frame_count = -1;
	src_fps = 25;
        src_fps = get_stream_fps(cap, cpp_video_capture);
	src_frame_count = get_stream_frame_count(cap, cpp_video_capture);
	
        //const char* output_name = "test_dnn_out.avi";
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('H', '2', '6', '4'), src_fps, size, 1);
        output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('D', 'I', 'V', 'X'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'J', 'P', 'G'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'P', '4', 'V'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'P', '4', '2'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('X', 'V', 'I', 'D'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('W', 'M', 'V', '2'), src_fps, size, 1);
    }

    double before = get_wall_time();

    while(1){
        ++local_frame_count;
        {
            if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
            if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");

            float nms = .45;    // 0.4F
            int local_nboxes = nboxes;
            detection *local_dets = dets;

            //if (nms) do_nms_obj(local_dets, local_nboxes, l.classes, nms);    // bad results
            if (nms) do_nms_sort(local_dets, local_nboxes, l.classes, nms);

#ifndef CLASSIFYAPP
            printf("\033[2J");
            printf("\033[1;1H");
            printf("\nFPS:%.1f\n", fps);
            printf("Objects:\n\n");
#else
	    printf("frame_count: %d letterbox: %d w:%d h:%d\n", local_frame_count, letter_box, det_s.w, det_s.h);
#endif

	    
            ++frame_id;
            /* if (demo_json_port > 0) {
                int timeout = 200;
                send_json(local_dets, local_nboxes, l.classes, demo_names, frame_id, demo_json_port, timeout);
		} */

            draw_detections_cv_v3(show_img, local_dets, local_nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);

	    if ((show_img != NULL) && (per_frame_json != NULL))
	      {
		add_info_to_per_frame_json_old((char *)filename);
	      }
		
            free_detections(local_dets, local_nboxes);

            if(!prefix){
                if (!dont_show) {
                    show_image_cv_ipl(show_img, "Demo");
                    int c = cvWaitKey(1);
                    if (c == 10) {
                        if (frame_skip == 0) frame_skip = 60;
                        else if (frame_skip == 4) frame_skip = 0;
                        else if (frame_skip == 60) frame_skip = 4;
                        else frame_skip = 0;
                    }
                    else if (c == 27 || c == 1048603) // ESC - exit (OpenCV 2.x / 3.x)
                    {
                        flag_exit = 1;
                    }
                }
            }else{
                char buff[256];
                sprintf(buff, "%s_%08d.jpg", prefix, local_frame_count);
                // if(show_img) save_cv_jpg(show_img, buff);
            }

            // if you run it with param -mjpeg_port 8090  then open URL in your web-browser: http://localhost:8090
            if (mjpeg_port > 0 && show_img) {
                int port = mjpeg_port;
                int timeout = 200;
                int jpeg_quality = 30;    // 1 - 100
                send_mjpeg(show_img, port, timeout, jpeg_quality);
            }

            // save video file
            if (output_video_writer && show_img) {
                cvWriteFrame(output_video_writer, show_img);
                printf("\n cvWriteFrame \n");
            }

            cvReleaseImage(&show_img);

            pthread_join(fetch_thread, 0);
            pthread_join(detect_thread, 0);

            if (flag_exit == 1) break;

            if(delay == 0){
                show_img = det_img;
            }
            det_img = in_img;
            det_s = in_s;
        }
        --delay;
        if(delay < 0){
            delay = frame_skip;

            //double after = get_wall_time();
            //float curr = 1./(after - before);
            double after = get_time_point();    // more accurate time measurements
            float curr = 1000000. / (after - before);
            fps = curr;
            before = after;
        }
    }
    printf("input video stream closed. \n");
    if (output_video_writer) {
        cvReleaseVideoWriter(&output_video_writer);
        printf("output_video_writer closed. \n");
    }

    if (json_filename)
      {
	char *full_json_string = cJSON_Print(per_frame_json);
	if (full_json_string == NULL) {
	  fprintf(stderr, "Failed to print per_frame_json.\n");
	} else {
	  FILE *json_ofp = fopen(json_filename, "w");
	  fwrite(full_json_string, sizeof(char), strlen(full_json_string), json_ofp);
	  fclose(json_ofp);
	}
	cJSON_Delete(per_frame_json);
      }

    // free memory
    cvReleaseImage(&show_img);
    cvReleaseImage(&in_img);
    free_image(in_s);

    free(avg);
    for (j = 0; j < FRAMES; ++j) free(predictions[j]);
    for (j = 0; j < FRAMES; ++j) free_image(images[j]);

    // free_ptrs(names, net.layers[net.n - 1].classes);
    local_frame_count = -1;
    
    int i;
    const int nsize = 8;
    for (j = 0; j < nsize; ++j) {
        for (i = 32; i < 127; ++i) {
            free_image(alphabet[j][i]);
        }
        free(alphabet[j]);
    }
    free(alphabet);
    free_network(net);
    //cudaProfilerStop();
}

#ifdef CLASSIFYAPP
void process_video(struct prep_network_info *prep_netinfo,
		   char *orig_filename,
		   char *filename,
		   float thresh,
		   float hier_thresh,
		   char *output_img_prefix,
		   char *out_filename,
		   int dont_show,
		   char *json_filename,
		   int http_stream_port,
		   int frame_skip,
		   struct detection_results *results_info)
{

    pthread_t detect_thread;
    pthread_t fetch_thread;
    int j;
    char name[256];
    layer l;
    CvVideoWriter* output_video_writer = NULL;    // cv::VideoWriter output_video;
    double before = 0.0;
    int delay = frame_skip, src_frame_count = -1;
    bool first_json_entry = true;
    
    in_img = det_img = show_img = NULL;
    
    demo_alphabet = prep_netinfo->alphabet;
    demo_names = prep_netinfo->names;
    demo_classes = prep_netinfo->classes;
    demo_thresh = thresh;
    // demo_hier = hier_thresh;
    net = prep_netinfo->net;
    demo_ext_output = 0;
    demo_json_port = -1;

    srand(2222222);
    
    if(filename)
      {
	syslog(LOG_INFO, "= Processing input video file: %s, %s:%d", filename, __FILE__, __LINE__);
        cpp_video_capture = 1;
        cap = get_capture_video_stream(filename);
      }
    else
      {
	syslog(LOG_ERR, "= No input video file specified, %s:%d", __FILE__, __LINE__);
	error("No filename specified\n");
	return;
      }

    if (!cap)
      {
	syslog(LOG_ERR, "= Could not open input video file: %s, %s:%d", filename, __FILE__, __LINE__);
	error("Couldn't open video file\n");
	return;
      }

    l = net.layers[net.n-1];

    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) images[j] = make_image(1,1,3);

    if (l.classes != demo_classes) {
      syslog(LOG_ERR, "Parameters don't match: in cfg-file classes=%d, in data-file classes=%d, %s:%d", l.classes, demo_classes, __FILE__, __LINE__);
      return;
    }

    if (json_filename)
      per_frame_json = cJSON_CreateObject();

    flag_exit = 0;

    // Fetch image/frame 0
    fetch_in_thread(0);
    det_img = in_img; // initialized
    det_s = in_s; // initialized

    // Detect in image/frame 1, dets and nboxes initialized
    /* detect_in_thread(0);
    do_nms_sort(dets, nboxes, l.classes, 0.45);
    draw_detections_cv_v3(show_img, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);
    if ((show_img != NULL) && (per_frame_json != NULL))
      add_info_to_per_frame_json((char *)filename);
    free_detections(dets, nboxes);
    */
    syslog(LOG_DEBUG, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d dets: %p show_img: %p det_img: %p in_img: %p",
	   local_frame_count, letter_box, det_s.w, det_s.h, nboxes, dets, show_img, det_img, in_img);	
    local_frame_count += 1;

    // Fetch image/frame 1
    fetch_in_thread(0); 

    // Fetch image/frame 1, dets and nboxes initialized - we miss frame 0
    detect_in_thread(0);
    det_img = in_img; // updated
    det_s = in_s; // updated
    free_detections(dets, nboxes); // for valgrind
    
    /* do_nms_sort(dets, nboxes, l.classes, 0.45);
    draw_detections_cv_v3(show_img, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);
    if ((show_img != NULL) && (per_frame_json != NULL))
      add_info_to_per_frame_json((char *)filename);
    free_detections(dets, nboxes);
    if (output_video_writer && show_img) {
      cvWriteFrame(output_video_writer, show_img);
      // printf("\n cvWriteFrame \n");
    }
    */
    syslog(LOG_DEBUG, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d dets: %p show_img: %p det_img: %p in_img: %p",
	   local_frame_count, letter_box, det_s.w, det_s.h, nboxes, dets, show_img, det_img, in_img);	
    local_frame_count += 1;

    for(j = 0; j < FRAMES/2; ++j){
        fetch_in_thread(0); // frame 2
        detect_in_thread(0); // dets and nboxes updated - keep this as this is what will get worked upon first
        det_img = in_img;
        det_s = in_s;
	/* 
	do_nms_sort(dets, nboxes, l.classes, 0.45);
	draw_detections_cv_v3(show_img, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);
	if ((show_img != NULL) && (per_frame_json != NULL))
	  add_info_to_per_frame_json((char *)filename);
	free_detections(dets, nboxes);
	if (output_video_writer && show_img) {
	  cvWriteFrame(output_video_writer, show_img);
	  // printf("\n cvWriteFrame \n");
	}
	*/
	syslog(LOG_DEBUG, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d dets: %p show_img: %p det_img: %p in_img: %p",
	       local_frame_count, letter_box, det_s.w, det_s.h, nboxes, dets, show_img, det_img, in_img);
	local_frame_count += 1;	

    }

    src_fps = get_stream_fps(cap, cpp_video_capture);
    src_frame_count = get_stream_frame_count(cap, cpp_video_capture);

    if (out_filename && !flag_exit)
      {
        CvSize size;
        size.width = det_img->width, size.height = det_img->height;
	
        //const char* output_name = "test_dnn_out.avi";
        output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('H', '2', '6', '4'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('D', 'I', 'V', 'X'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'J', 'P', 'G'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'P', '4', 'V'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'P', '4', '2'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('X', 'V', 'I', 'D'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('W', 'M', 'V', '2'), src_fps, size, 1);
      }


    before = get_wall_time();
    while(1){
      {
	//if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
	//if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");	
	
	float nms = .45;    // 0.4F
	int local_nboxes = nboxes;
	detection *local_dets = dets;
	
	//if (nms) do_nms_obj(local_dets, local_nboxes, l.classes, nms);    // bad results
	if (nms) do_nms_sort(local_dets, local_nboxes, l.classes, nms); // work on dets, nboxes for frame 2 to begin with
	
	syslog(LOG_DEBUG, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d dets: %p show_img: %p det_img: %p in_img: %p",
	       local_frame_count, letter_box, det_s.w, det_s.h, local_nboxes, local_dets,
	       show_img, det_img, in_img);	
	
	++frame_id;
	/* if (demo_json_port > 0) {
	   int timeout = 200;
	   send_json(local_dets, local_nboxes, l.classes, demo_names, frame_id, demo_json_port, timeout);
	   } */
	
	draw_detections_cv_v3(show_img, local_dets, local_nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);
	
	if ((show_img != NULL) && (per_frame_json != NULL))
	  {
	    add_info_to_per_frame_json((char *)orig_filename, first_json_entry);
	    if (first_json_entry)
	      first_json_entry = false;
	  }
	  	
	free_detections(local_dets, local_nboxes); // dets, boxes freed
	local_dets = NULL;
	local_nboxes = 0;
	dets = local_dets;
	nboxes = local_nboxes;
	
	if(!output_img_prefix){
	  if (!dont_show) {
	    show_image_cv_ipl(show_img, "Demo");
	    int c = cvWaitKey(1);
	    if (c == 10) {
	      if (frame_skip == 0) frame_skip = 60;
	      else if (frame_skip == 4) frame_skip = 0;
	      else if (frame_skip == 60) frame_skip = 4;
	      else frame_skip = 0;
	    }
	    else if (c == 27 || c == 1048603) // ESC - exit (OpenCV 2.x / 3.x)
	      {
		flag_exit = 1;
	      }
	  }
	} else {
	  sprintf(name, "%s_%08d.jpg", output_img_prefix, local_frame_count);
	  if(show_img) save_cv_jpg(show_img, name);
	}
	
	// if you run it with param -mjpeg_port 8090  then open URL in your web-browser: http://localhost:8090
	/* if (mjpeg_port > 0 && show_img) {
	  int port = mjpeg_port;
	  int timeout = 200;
	  int jpeg_quality = 30;    // 1 - 100
	  send_mjpeg(show_img, port, timeout, jpeg_quality);
	  } */
	
	// save video file
	if (output_video_writer && show_img) {
	  cvWriteFrame(output_video_writer, show_img);
	  // printf("\n cvWriteFrame \n");
	}
	
	cvReleaseImage(&show_img);

	if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
	if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");	

	pthread_join(fetch_thread, 0);
	pthread_join(detect_thread, 0); // dets and nboxes updated for frame 3
		
	if (flag_exit == 1) break;
	
	if(delay == 0){
	  show_img = det_img;
	}
	det_img = in_img;
	det_s = in_s;
	++local_frame_count;
	results_info->percentage_completed = (local_frame_count * 100.0) / src_frame_count;
      }
      --delay;
      if(delay < 0){
	delay = frame_skip;
	
	//double after = get_wall_time();
	//float curr = 1./(after - before);
	double after = get_time_point();    // more accurate time measurements
	float curr = 1000000. / (after - before);
	fps = curr;
	before = after;
      }

      if ((local_frame_count % 100) == 0)
	syslog(LOG_INFO, "= For %s, FPS: %0.1f, framecount: %d/%d, percentage_completed = %0.1f, %s:%d",
	       filename, fps, local_frame_count, src_frame_count, results_info->percentage_completed, __FILE__, __LINE__);
      
    }

    syslog(LOG_INFO, "= DONE | For %s, FPS: %0.1f, framecount: %d/%d, percentage_completed = %0.1f, %s:%d",
	   filename, fps, local_frame_count, src_frame_count, results_info->percentage_completed, __FILE__, __LINE__);

    syslog(LOG_INFO, "= Input video stream closed. %s:%d", __FILE__, __LINE__);
    if (output_video_writer) {
        cvReleaseVideoWriter(&output_video_writer);
	syslog(LOG_INFO, "= Output_video_writer closed. %s:%d", __FILE__, __LINE__);
    }

    if (json_filename)
      {
	char *full_json_string = cJSON_Print(per_frame_json);
	if (full_json_string == NULL) {
	  syslog(LOG_ERR, "= Failed to print per_frame_json to file");
	} else {
	  FILE *json_ofp = fopen(json_filename, "w");
	  fwrite(full_json_string, sizeof(char), strlen(full_json_string), json_ofp);
	  fclose(json_ofp);
	  free(full_json_string);
	  full_json_string = NULL;
	  syslog(LOG_INFO, "= Done writing output JSON file");
	}
	cJSON_Delete(per_frame_json);
      }

    // free memory
    cvReleaseImage(&show_img);
    cvReleaseImage(&in_img);
    free_image(in_s);
    local_frame_count = -1;
    src_frame_count = -1;
    close_stream(cap, cpp_video_capture);
    cap = NULL;

    free(avg);
    for (j = 0; j < FRAMES; ++j) free(predictions[j]);
    for (j = 0; j < FRAMES; ++j) free_image(images[j]);
    
    return;
    
}

void process_video_old(struct prep_network_info *prep_netinfo,
		       char *filename,
		       float thresh,
		       float hier_thresh,
		       char *output_img_prefix,
		       char *out_filename,
		       int dont_show,
		       char *json_filename,
		       int http_stream_port,
		       int frame_skip)
{

    pthread_t detect_thread;
    pthread_t fetch_thread;
    int j;
    char name[256];
    layer l;
    CvVideoWriter* output_video_writer = NULL;    // cv::VideoWriter output_video;
    double before = 0.0;
    int delay = frame_skip;

    in_img = det_img = show_img = NULL;
    
    demo_alphabet = prep_netinfo->alphabet;
    demo_names = prep_netinfo->names;
    demo_classes = prep_netinfo->classes;
    demo_thresh = thresh;
    // demo_hier = hier_thresh;
    net = prep_netinfo->net;
    demo_ext_output = 0;
    demo_json_port = -1;

    srand(2222222);
    
    if(filename)
      {
	syslog(LOG_INFO, "= Processing input video file: %s, %s:%d", filename, __FILE__, __LINE__);
        cpp_video_capture = 1;
        cap = get_capture_video_stream(filename);
      }
    else
      {
	syslog(LOG_ERR, "= No input video file specified, %s:%d", __FILE__, __LINE__);
	error("No filename specified\n");
	return;
      }

    if (!cap)
      {
	syslog(LOG_ERR, "= Could not open input video file: %s, %s:%d", filename, __FILE__, __LINE__);
	error("Couldn't open video file\n");
	return;
      }

    l = net.layers[net.n-1];

    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) images[j] = make_image(1,1,3);

    if (l.classes != demo_classes) {
      syslog(LOG_ERR, "Parameters don't match: in cfg-file classes=%d, in data-file classes=%d, %s:%d", l.classes, demo_classes, __FILE__, __LINE__);
      return;
    }

    if (json_filename)
      per_frame_json = cJSON_CreateObject();

    flag_exit = 0;

    // Fetch image/frame 1
    fetch_in_thread(0);
    det_img = in_img;
    det_s = in_s;
    
    // Fetch image/frame 2, dets and nboxes changes
    fetch_in_thread(0); 
    // Fetch image/frame 2
    detect_in_thread(0);
    det_img = in_img;
    det_s = in_s;
    syslog(LOG_INFO, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d",
	   local_frame_count, letter_box, det_s.w, det_s.h, nboxes);	

    for(j = 0; j < FRAMES/2; ++j){
        fetch_in_thread(0);
        detect_in_thread(0);
        det_img = in_img;
        det_s = in_s;
	syslog(LOG_INFO, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d",
	       local_frame_count, letter_box, det_s.w, det_s.h, nboxes);
    }

    if (out_filename && !flag_exit)
    {
        CvSize size;
        size.width = det_img->width, size.height = det_img->height;
        src_fps = get_stream_fps(cap, cpp_video_capture);

        //const char* output_name = "test_dnn_out.avi";
        output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('H', '2', '6', '4'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('D', 'I', 'V', 'X'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'J', 'P', 'G'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'P', '4', 'V'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('M', 'P', '4', '2'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('X', 'V', 'I', 'D'), src_fps, size, 1);
        //output_video_writer = cvCreateVideoWriter(out_filename, CV_FOURCC('W', 'M', 'V', '2'), src_fps, size, 1);
    }
    
    before = get_wall_time();

    while(1){
      ++local_frame_count;
      {
	//if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
	//if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");	
	float nms = .45;    // 0.4F
	int local_nboxes = nboxes;
	detection *local_dets = dets;
	
	//if (nms) do_nms_obj(local_dets, local_nboxes, l.classes, nms);    // bad results
	if (nms) do_nms_sort(local_dets, local_nboxes, l.classes, nms);
	
	syslog(LOG_INFO, "= frame_count: %d letterbox: %d w:%d h:%d nboxes: %d",
	       local_frame_count, letter_box, det_s.w, det_s.h, local_nboxes);	
	
	++frame_id;
	/* if (demo_json_port > 0) {
	   int timeout = 200;
	   send_json(local_dets, local_nboxes, l.classes, demo_names, frame_id, demo_json_port, timeout);
	   } */
	
	draw_detections_cv_v3(show_img, local_dets, local_nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);
	
	if ((show_img != NULL) && (per_frame_json != NULL))
	  add_info_to_per_frame_json_old((char *)filename);
	  	
	free_detections(local_dets, local_nboxes);

	if(!output_img_prefix){
	  if (!dont_show) {
	    show_image_cv_ipl(show_img, "Demo");
	    int c = cvWaitKey(1);
	    if (c == 10) {
	      if (frame_skip == 0) frame_skip = 60;
	      else if (frame_skip == 4) frame_skip = 0;
	      else if (frame_skip == 60) frame_skip = 4;
	      else frame_skip = 0;
	    }
	    else if (c == 27 || c == 1048603) // ESC - exit (OpenCV 2.x / 3.x)
	      {
		flag_exit = 1;
	      }
	  }
	} else {
	  sprintf(name, "%s_%08d.jpg", output_img_prefix, local_frame_count);
	  if(show_img) save_cv_jpg(show_img, name);
	}
	
	// if you run it with param -mjpeg_port 8090  then open URL in your web-browser: http://localhost:8090
	/* if (mjpeg_port > 0 && show_img) {
	  int port = mjpeg_port;
	  int timeout = 200;
	  int jpeg_quality = 30;    // 1 - 100
	  send_mjpeg(show_img, port, timeout, jpeg_quality);
	  } */
	
	// save video file
	if (output_video_writer && show_img) {
	  cvWriteFrame(output_video_writer, show_img);
	  // printf("\n cvWriteFrame \n");
	}
	
	cvReleaseImage(&show_img);

	if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
	if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");	

	pthread_join(fetch_thread, 0);
	pthread_join(detect_thread, 0);
	
	if (flag_exit == 1) break;
	
	if(delay == 0){
	  show_img = det_img;
	}
	det_img = in_img;
	det_s = in_s;
      }
      --delay;
      if(delay < 0){
	delay = frame_skip;
	
	//double after = get_wall_time();
	//float curr = 1./(after - before);
	double after = get_time_point();    // more accurate time measurements
	float curr = 1000000. / (after - before);
	fps = curr;
	before = after;
      }

      //if ((local_frame_count % 100) == 0)
      //syslog(LOG_INFO, "= For %s, FPS: %0.1f, framecount: %d, %s:%d", filename, fps, local_frame_count+1, __FILE__, __LINE__);
      
    }
    
    syslog(LOG_INFO, "= Input video stream closed. %s:%d", __FILE__, __LINE__);
    if (output_video_writer) {
        cvReleaseVideoWriter(&output_video_writer);
	syslog(LOG_INFO, "= Output_video_writer closed. %s:%d", __FILE__, __LINE__);
    }

    if (json_filename)
      {
	char *full_json_string = cJSON_Print(per_frame_json);
	if (full_json_string == NULL) {
	  fprintf(stderr, "Failed to print per_frame_json.\n");
	} else {
	  FILE *json_ofp = fopen(json_filename, "w");
	  fwrite(full_json_string, sizeof(char), strlen(full_json_string), json_ofp);
	  fclose(json_ofp);
	}
	cJSON_Delete(per_frame_json);
      }

    // free memory
    cvReleaseImage(&show_img);
    cvReleaseImage(&in_img);
    free_image(in_s);
    local_frame_count = 0;
    // close_stream(cap, cpp_video_capture);
    // cap = NULL;

    free(avg);
    for (j = 0; j < FRAMES; ++j) free(predictions[j]);
    for (j = 0; j < FRAMES; ++j) free_image(images[j]);
    
    return;
    
}
#endif

#else
void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
    int frame_skip, char *prefix, char *out_filename, int mjpeg_port, int json_port, int dont_show, int ext_output)
{
    fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif

