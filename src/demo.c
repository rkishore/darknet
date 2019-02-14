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

#ifdef CLASSIFYAPP
#include "darknet.h"
#endif

image get_image_from_stream(CvCapture *cap);

static char **demo_names;
static image **demo_alphabet;
static int demo_classes;

static float **probs;
static box *boxes;
static network net;
static image in_s ;
static image det_s;
static CvCapture * cap;
static int cpp_video_capture = 0;
static float fps = 0;
static float demo_thresh = 0;
static int demo_ext_output = 0;

static float *predictions[FRAMES];
static int demo_index = 0;
static image images[FRAMES];
static IplImage* ipl_images[FRAMES];
static float *avg;

void draw_detections_cv(IplImage* show_img, int num, float thresh, box *boxes, float **probs, char **names, image **alphabet, int classes);
void draw_detections_cv_v3(IplImage* show_img, detection *dets, int num, float thresh, char **names, image **alphabet, int classes, int ext_output);
void show_image_cv_ipl(IplImage *disp, const char *name);
image get_image_from_stream_resize(CvCapture *cap, int w, int h, int c, IplImage** in_img, int cpp_video_capture, int dont_close);
image get_image_from_stream_letterbox(CvCapture *cap, int w, int h, int c, IplImage** in_img, int cpp_video_capture, int dont_close);
int get_stream_fps(CvCapture *cap, int cpp_video_capture);
IplImage* in_img;
IplImage* det_img;
IplImage* show_img;

static int flag_exit;
static int letter_box = 0;
static int local_frame_count = 0;

cJSON *per_frame_json = NULL;

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
        printf("Stream closed.\n");
        flag_exit = 1;
        return EXIT_FAILURE;
    }
    //in_s = resize_image(in, net.w, net.h);

    return 0;
}

void *detect_in_thread(void *ptr)
{
    float nms = .45;    // 0.4F
    
    layer l = net.layers[net.n-1];
    float *X = det_s.data;
    float *prediction = network_predict(net, X);

    cJSON *regions_json = NULL, *regions_array_json = NULL, *region_json = NULL;
    char tmp_char_buff[128];
    int i,j, num_labels_detected = 0;
    int left, right, top, bot;
    box b;

    memcpy(predictions[demo_index], prediction, l.outputs*sizeof(float));
    mean_arrays(predictions, FRAMES, l.outputs, avg);
    l.output = avg;

    free_image(det_s);

    int nboxes = 0;
    detection *dets = NULL;
    if (letter_box)
        dets = get_network_boxes(&net, in_img->width, in_img->height, demo_thresh, demo_thresh, 0, 1, &nboxes, 1); // letter box
    else
        dets = get_network_boxes(&net, det_s.w, det_s.h, demo_thresh, demo_thresh, 0, 1, &nboxes, 0); // resized
    //if (nms) do_nms_obj(dets, nboxes, l.classes, nms);    // bad results
    if (nms) do_nms_sort(dets, nboxes, l.classes, nms);


    printf("\033[2J");
    printf("\033[1;1H");
    printf("\nFPS:%.1f\n",fps);
    printf("frame_count: %d letterbox: %d w:%d h:%d\n", local_frame_count + 1, letter_box, det_s.w, det_s.h);
    printf("Objects:\n\n");
    
    ipl_images[demo_index] = det_img;
    det_img = ipl_images[(demo_index + FRAMES / 2 + 1) % FRAMES];
    demo_index = (demo_index + 1)%FRAMES;

    draw_detections_cv_v3(det_img, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes, demo_ext_output);

    if (det_img != NULL)
      {
	if (per_frame_json != NULL)
	  {
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
	  }
      }
    
    free_detections(dets, nboxes);

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

void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
	  int frame_skip, char *prefix, char *out_filename, int http_stream_port, int dont_show, int ext_output, const char *json_filename)
{
    //skip = frame_skip;
    image **alphabet = load_alphabet();
    int delay = frame_skip;
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_ext_output = ext_output;
    printf("Demo\n");
    net = parse_network_cfg_custom(cfgfile, 1);    // set batch=1
    if(weightfile){
        load_weights(&net, weightfile);
    }
    //set_batch_network(&net, 1);
    fuse_conv_batchnorm(net);
    calculate_binary_weights(net);
    srand(2222222);

    if(filename){
        printf("video file: %s\n", filename);
//#ifdef CV_VERSION_EPOCH    // OpenCV 2.x
//        cap = cvCaptureFromFile(filename);
//#else                    // OpenCV 3.x
        cpp_video_capture = 1;
        cap = get_capture_video_stream((char *)filename);
//#endif
    }else{
        printf("Webcam index: %d\n", cam_index);
//#ifdef CV_VERSION_EPOCH    // OpenCV 2.x
//        cap = cvCaptureFromCAM(cam_index);
//#else                    // OpenCV 3.x
        cpp_video_capture = 1;
        cap = get_capture_webcam(cam_index);
//#endif
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

    boxes = (box *)calloc(l.w*l.h*l.n, sizeof(box));
    probs = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
    for(j = 0; j < l.w*l.h*l.n; ++j) probs[j] = (float *)calloc(l.classes, sizeof(float *));

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

    if(filename)
      detect_in_thread((void *)filename);
    else
      detect_in_thread(0);
    
    det_img = in_img;
    det_s = in_s;

    for(j = 0; j < FRAMES/2; ++j){
        fetch_in_thread(0);
	if(filename)
	  detect_in_thread((void *)filename);
	else
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
        int src_fps = 25;
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

    double before = get_wall_time();

    while(1){
        ++local_frame_count;
        if(1){
            if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
	    if(filename)
	      {
		if(pthread_create(&detect_thread, 0, detect_in_thread, (void *)filename)) error("Thread creation failed");
	      }
	    else
	      {
		if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
	      }
	    
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
                cvSaveImage(buff, show_img, 0);
                //save_image(disp, buff);
            }

            // if you run it with param -http_port 8090  then open URL in your web-browser: http://localhost:8090
            if (http_stream_port > 0 && show_img) {
                //int port = 8090;
                int port = http_stream_port;
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
        }else {
            fetch_in_thread(0);
            det_img = in_img;
            det_s = in_s;
            detect_in_thread(0);

            show_img = det_img;
            if (!dont_show) {
                show_image_cv_ipl(show_img, "Demo");
                cvWaitKey(1);
            }
            cvReleaseImage(&show_img);
        }
        --delay;
        if(delay < 0){
            delay = frame_skip;

            double after = get_wall_time();
            float curr = 1./(after - before);
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

    for (j = 0; j < l.w*l.h*l.n; ++j) free(probs[j]);
    free(boxes);
    free(probs);

    free_ptrs((void **)names, net.layers[net.n - 1].classes);

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
}

#ifdef CLASSIFYAPP
void demo_custom(struct prep_network_info *prep_netinfo, char *filename, float thresh, float hier_thresh, char *output_img_prefix, char *out_filename, int dont_show, const char *json_filename, int http_stream_port, int frame_skip)
{

    pthread_t detect_thread;
    pthread_t fetch_thread;
    int j;
    char name[256];
    layer l;
    CvVideoWriter* output_video_writer = NULL;    // cv::VideoWriter output_video;
    double before = 0.0;
    int delay = frame_skip;
    
    demo_names = prep_netinfo->names;
    demo_alphabet = prep_netinfo->alphabet;
    demo_classes = prep_netinfo->classes;
    demo_thresh = thresh;
    // demo_hier = hier_thresh;
    printf("Demo\n");
    net = prep_netinfo->net;
    
    srand(2222222);
    
    if(filename){
        printf("video file: %s\n", filename);
        cpp_video_capture = 1;
        cap = get_capture_video_stream(filename);
    }else{
      error("No filename specified\n");
      return;
    }

    if (!cap) 
      error("Couldn't open video file\n");

    l = net.layers[net.n-1];

    avg = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) predictions[j] = (float *) calloc(l.outputs, sizeof(float));
    for(j = 0; j < FRAMES; ++j) images[j] = make_image(1,1,3);

    boxes = (box *)calloc(l.w*l.h*l.n, sizeof(box));
    probs = (float **)calloc(l.w*l.h*l.n, sizeof(float *));
    for(j = 0; j < l.w*l.h*l.n; ++j) probs[j] = (float *)calloc(l.classes, sizeof(float *));

    if (l.classes != demo_classes) {
      printf("Parameters don't match: in cfg-file classes=%d, in data-file classes=%d \n", l.classes, demo_classes);
      return;
    }

    if (json_filename)
      per_frame_json = cJSON_CreateObject();

    flag_exit = 0;

    fetch_in_thread(0);
    det_img = in_img;
    det_s = in_s;

    fetch_in_thread(0);

    detect_in_thread((void *)filename);

    det_img = in_img;
    det_s = in_s;

    for(j = 0; j < FRAMES/2; ++j){
        fetch_in_thread(0);
	detect_in_thread((void *)filename);
	det_img = in_img;
        det_s = in_s;
    }

    if (out_filename && !flag_exit)
    {
        CvSize size;
        int src_fps = 25;

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
    while(1) {
      
        ++local_frame_count;

	if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
	if(pthread_create(&detect_thread, 0, detect_in_thread, (void *)filename)) error("Thread creation failed");
	
	if(output_img_prefix){
	  sprintf(name, "%s_%08d.jpg", output_img_prefix, local_frame_count);
	  cvSaveImage(name, show_img, 0);
	  //save_image(disp, buff);
	}
	
	// if you run it with param -http_port 8090  then open URL in your web-browser: http://localhost:8090
	if (http_stream_port > 0 && show_img) {
	  //int port = 8090;
	  int port = http_stream_port;
	  int timeout = 200;
	  int jpeg_quality = 30;    // 1 - 100
	  send_mjpeg(show_img, port, timeout, jpeg_quality);
	}
	
	// save video file
	if (output_video_writer && show_img) {
	  cvWriteFrame(output_video_writer, show_img);
	  // printf("\n cvWriteFrame \n");
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
      
      double after = get_wall_time();
      float curr = 1./(after - before);
      fps = curr;
      before = after;
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

    for (j = 0; j < l.w*l.h*l.n; ++j) free(probs[j]);
    free(boxes);
    free(probs);
    
    return;
    
}
#endif

#else
void demo(char *cfgfile, char *weightfile, float thresh, float hier_thresh, int cam_index, const char *filename, char **names, int classes,
	  int frame_skip, char *prefix, char *out_filename, int http_stream_port, int dont_show, int ext_output, char *json_filename)
{
  fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}

#ifdef CLASSIFYAPP
void demo_custom(struct prep_network_info *prep_netinfo, char *filename, float thresh, float hier_thresh,
		 char *output_img_prefix, char *out_filename, int dont_show, const char *json_filename, int http_stream_port, int frame_skip)
{
  fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif
  
#endif

