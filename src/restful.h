#if !defined(_IGOLGI_RESTFUL_H_)
#define _IGOLGI_RESTFUL_H_

#include "common.h"

#define START_CLASSIFY_ID 0

#define MAX_DETECTIONS_PER_IMAGE 16
#define MAX_LABEL_STRING_SIZE 128

struct detection_results {

  int num_labels_detected;
  
  char labels[MAX_DETECTIONS_PER_IMAGE][MAX_LABEL_STRING_SIZE];

  float confidence[MAX_DETECTIONS_PER_IMAGE];

  float processing_time_in_seconds;

  int top_left_x[MAX_DETECTIONS_PER_IMAGE];
  int top_left_y[MAX_DETECTIONS_PER_IMAGE];
  int top_right_x[MAX_DETECTIONS_PER_IMAGE];
  int top_right_y[MAX_DETECTIONS_PER_IMAGE];
  int bottom_left_x[MAX_DETECTIONS_PER_IMAGE];
  int bottom_left_y[MAX_DETECTIONS_PER_IMAGE];
  int bottom_right_x[MAX_DETECTIONS_PER_IMAGE];
  int bottom_right_y[MAX_DETECTIONS_PER_IMAGE];

  int left[MAX_DETECTIONS_PER_IMAGE];
  int right[MAX_DETECTIONS_PER_IMAGE];
  int top[MAX_DETECTIONS_PER_IMAGE];
  int bottom[MAX_DETECTIONS_PER_IMAGE];
  
};

typedef struct _classify_job_info_ {

  int                classify_id;

  int                classify_status;
  pthread_mutex_t    job_status_lock;

  float              percentage_finished;
  pthread_mutex_t    job_percent_complete_lock;

  struct timespec    start_timestamp, end_timestamp;

  struct detection_results results_info;
  
} classify_job_info;

typedef struct _restful_comm_struct_ {

    pthread_t          restful_server_thread_id;
    int                restful_thread_handle;
    volatile int       is_restful_thread_active;
    int                restful_server_port;
    void               *dispatch_queue;

    pthread_mutex_t    *classifyapp_data_lock;
    classifyapp_struct    *classifyapp_data;

    /*
    bool               classifyapp_init;
    pthread_mutex_t    classifyapp_init_lock;
    */
  
    classify_job_info cur_classify_info;
    pthread_mutex_t    classify_info_lock;

    int                classify_thread_status;
    pthread_mutex_t    thread_status_lock;

    pthread_t          classify_thread_id;
    volatile int       is_classify_thread_active;

} restful_comm_struct;

enum {
  HTTP_200, // OK
  HTTP_201, // Created
  HTTP_204, // No Content (used for successful DELETE response)
  HTTP_400, // Bad Request
  HTTP_404, // Not Found
  HTTP_405, // Method Not Allowed
  HTTP_501, // Not Implemented
  HTTP_503  // Service Unavailable
};

enum {
  CLASSIFY_THREAD_STATUS_IDLE,
  CLASSIFY_THREAD_STATUS_BUSY
};

enum {
  CLASSIFY_STATUS_IDLE,
  CLASSIFY_STATUS_RUNNING,
  CLASSIFY_STATUS_STOPPED,
  CLASSIFY_STATUS_COMPLETED,
  CLASSIFY_STATUS_ERROR,
  CLASSIFY_STATUS_INPUT_ERROR,
  CLASSIFY_STATUS_OUTPUT_ERROR,
};

#if defined(__cplusplus)
extern "C" {
#endif

  restful_comm_struct *restful_comm_create(classifyapp_struct *classifyapp, int *server_port);
  int restful_comm_destroy(restful_comm_struct *restful);
  int restful_comm_start(restful_comm_struct *restful, void *dispatch_queue);
  int restful_comm_stop(restful_comm_struct *restful);
  int restful_comm_thread_start(classifyapp_struct *classifyapp_data, restful_comm_struct **restful, void **dispatch_queue, int *server_port);
  char *file_extension(char *filename);
  int restful_classify_thread_start(restful_comm_struct *restful);
  int restful_classify_thread_stop(restful_comm_struct *restful);

#if defined(__cplusplus)
}
#endif


#endif // _IGOLGI_RESTFUL_H_
