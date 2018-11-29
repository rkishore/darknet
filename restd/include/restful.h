#if !defined(_ZIPREEL_RESTFUL_H_)
#define _ZIPREEL_RESTFUL_H_

#include "common.h"

#define START_TRANSCRIBE_ID 0

typedef struct _transcribe_job_info_ {

  int                transcribe_id;

  int                transcribe_status;
  pthread_mutex_t    job_status_lock;

  float              percentage_finished;
  pthread_mutex_t    job_percent_complete_lock;

  struct timespec    start_timestamp, end_timestamp;

} transcribe_job_info;

typedef struct _restful_comm_struct_ {

    pthread_t          restful_server_thread_id;
    int                restful_thread_handle;
    volatile int       is_restful_thread_active;
    int                restful_server_port;
    void               *dispatch_queue;

    pthread_mutex_t    *audioapp_data_lock;
    audioapp_struct    *audioapp_data;
    
    bool               audioapp_init;
    pthread_mutex_t    audioapp_init_lock;

    transcribe_job_info cur_transcribe_info;
    pthread_mutex_t    transcribe_info_lock;

    int                transcribe_thread_status;
    pthread_mutex_t    thread_status_lock;

    pthread_t          transcribe_thread_id;
    volatile int       is_transcribe_thread_active;

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
  TRANSCRIBE_THREAD_STATUS_IDLE,
  TRANSCRIBE_THREAD_STATUS_BUSY
};

enum {
  TRANSCRIBE_STATUS_IDLE,
  TRANSCRIBE_STATUS_RUNNING,
  TRANSCRIBE_STATUS_STOPPED,
  TRANSCRIBE_STATUS_COMPLETED,
  TRANSCRIBE_STATUS_ERROR
};

#if defined(__cplusplus)
extern "C" {
#endif

  restful_comm_struct *restful_comm_create(audioapp_struct *audioapp, int *server_port);
  int restful_comm_destroy(restful_comm_struct *restful);
  int restful_comm_start(restful_comm_struct *restful, void *dispatch_queue);
  int restful_comm_stop(restful_comm_struct *restful);
  int restful_comm_thread_start(audioapp_struct *audioapp_data, restful_comm_struct **restful, void **dispatch_queue, int *server_port);
  char *file_extension(char *filename);
  int restful_transcribe_thread_start(restful_comm_struct *restful);
  int restful_transcribe_thread_stop(restful_comm_struct *restful);

#if defined(__cplusplus)
}
#endif


#endif // _ZIPREEL_RESTFUL_H_
