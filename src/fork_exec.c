
/* Functions to handle forking and exec-ing fileapp */

#include <common.h>

/* For clean cmdline args parsing */
#include <config.h>

/* JSON parser */
// #include <jansson.h>

/* For archiving output files to be pushed to DB */
// #include <zip_archival.h>

/* For ffprobe json parsing */
// #include <json.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define PROC_PRIORITY_INCREMENT 5
#define MAX_EXEC_RETRIES 2
#define MAX_NUM_KILL_SIGNALS_THRESHOLD 3



static char **argv_from_string(char *cmd, char *args) {
  int i, spaces = 0, argc = 0, len = strlen(args);
  char **argv;
  
  for ( i = 0; i < len; i++ )
    if ( isspace(args[i]) ) spaces++;
  
  // add 1 for cmd, 1 for NULL and 1 as spaces will be one short
  argv = (char **) malloc ( (spaces + 3) * sizeof(char *) );
  argv[argc++] = cmd;
  argv[argc++] = args;
  
  for ( i = 0; i < len; i++ ) {
    if ( isspace(args[i]) ) {
      args[i] = '\0';
      if ( i + 1 < len )
	argv[argc++] = args + i + 1;
    }
  }
  
  argv[argc] = (char*)NULL;
  return argv;
}

/*
static void 
populate_stream_info(struct per_job_info *cur_job_info, char *line, struct stream_info *stream) 
{

  int i = 0, dup_strlen = strlen(line) + 1; 
  char *token, *expected_token = ":";
  char *dup_str = NULL, *dup_str_copy = NULL, *pos = NULL;
  
  if (dup_strlen > 1) {
    
    dup_strlen += (ZR_WORDSIZE_IN_BYTES - dup_strlen % ZR_WORDSIZE_IN_BYTES);
    dup_str = (char *)malloc(dup_strlen);
    if (dup_str == NULL) {
      
      syslog(LOG_ERR, "Could not allocate memory for per-line parsing during discover of %s, %d\n", 
	     cur_job_info->localfilepath, cur_job_info->chunk_num);
      return;

    }
    
    memset((void *)dup_str, 0, dup_strlen);
    memcpy(dup_str, line, dup_strlen);
    if ((pos=strchr(dup_str, '\n')) != NULL)
      *pos = '\0';

    dup_str_copy = dup_str;

    // syslog(LOG_INFO, "Processing: %s\n", dup_str);

    if (starts_with("width", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->width = atoi(token);
	}
	i++;
      }
    } else if (starts_with("height", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->height = atoi(token);
	}
	i++;
      }
    } else if (starts_with("aspect_ratio", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->aspect_ratio = atoi(token);
	}
	i++;
      }
    } else if (starts_with("estimated_bitrate", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->bitrate = atof(token);
	}
	i++;
      }
    } else if (starts_with("framerate", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->framerate = atof(token);
	}
	i++;
      }
    } else if (starts_with("total frames", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) 
	{
	  syslog(LOG_DEBUG, "Token: %s, i = %d\n", token, i);
	  if (i == 1) 
	    {
	      char *subtoken, *expected_subtoken = " ", *dup_token1 = strdup(token), *dup_token2 = NULL;
	      int j = 0;
	 
	      if (dup_token1 == NULL)
		{
		  syslog(LOG_ERR, "= Could not duplicate str @ %s, line %d", __FILE__, __LINE__);
		  break;
		}
	      
	      dup_token2 = dup_token1;
	      syslog(LOG_DEBUG, "dup_token: %s, j = %d\n", dup_token1, j);

	      while ((subtoken = strsep((char **)&dup_token1, expected_subtoken)) != NULL) 
		{
		  syslog(LOG_DEBUG, "Subtoken: %s, j = %d\n", subtoken, j);
		  if (j == 1)
		    {
		      stream->num_PES_frames = atoi(subtoken);
		      break;
		    }
		  j++;
		}

	      free_mem(dup_token2);
	    }

	  i++;
	  
	}
    } else if (starts_with("sample_rate", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->sample_rate = atoi(token);
	}
	i++;
      }
    } else if (starts_with("channels", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->channels = atoi(token);
	}
	i++;
      }
    } else if (starts_with("nominal_frame_size", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->framesize = atoi(token);
	}
	i++;
      }
    } else if (starts_with("codec_type", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  stream->codec_type = atoi(token);
	}
	i++;
      }
    } else if (starts_with("filesize", dup_str) == true) {
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) {
	if (i == 1) {
	  if (!strcmp(cur_job_info->type, "tcode")) {
	    cur_job_info->segment_size = atol(token);
	    if (cur_job_info->enhanced_mode == false)
	      cur_job_info->input_media_size = cur_job_info->segment_size;
	  } else
	    cur_job_info->segment_size = cur_job_info->input_media_size;
	}
	i++;
      }
    } else if (starts_with("Estimated video duration", dup_str) == true) {
      
      while ((token = strsep((char **)&dup_str, expected_token)) != NULL) 
	{
	  if (i == 1) {
	    if (strcmp(token, "nan"))
	      cur_job_info->estimated_duration = atof(token);
	    else
	      cur_job_info->estimated_duration = -1.0;
	  }
	  i++;
	}
    }
    
    free(dup_str_copy);
    dup_str_copy = NULL;
    dup_str = NULL;
    
  }

  return;
}
*/

/*
static void
parse_mediainfo_output(struct per_job_info *cur_job_info, 
		       const char *xml_filename,
		       struct stream_info *video0, 
		       struct stream_info **audio)
{
  int i = 0;
  struct stream_info *cur_audio;
  for (i = 0; i < MAX_EXPECTED_AUDIO_STREAMS; i++)
    {
      cur_audio = *(audio + i);
      cur_audio->track_id = -1;
    }

  parse_mediainfo_xml(xml_filename, cur_job_info, video0, audio);

  if (video0->track_id > -1)
    video0->parsed = true;

  for (i = 0; i < MAX_EXPECTED_AUDIO_STREAMS; i++)
    {
      cur_audio = *(audio + i);
      if (cur_audio->track_id > -1)
	cur_audio->parsed = true;
    }

  return;

}
*/

/*
static int 
parse_inputfile_stats_and_report_to_DB(struct per_job_info *cur_job_info, 
				       char *cmd_to_exec, int *opcode, long double *cpu_util, unsigned int *expected_filename_len) 
{
  char file_name[*expected_filename_len];
  FILE* file; 
  char *tcode_json = NULL;
  struct stream_info *video_stream = NULL, *audio_streams[MAX_EXPECTED_AUDIO_STREAMS], *audio0 = NULL, *audio1 = NULL;
  bool seen_audio_hdrs[MAX_EXPECTED_AUDIO_STREAMS], audio_stream_mem_error = false;
  double bytes_done = 0, cur_speed = 0;
  unsigned int stream_info_size = sizeof(struct stream_info), i = 0;

  video_stream = (struct stream_info *)malloc(stream_info_size);
  memset(audio_streams, 0, sizeof(struct stream_info *) * MAX_EXPECTED_AUDIO_STREAMS);

  for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
    seen_audio_hdrs[i] = false;

  for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
    {
      audio_streams[i] = (struct stream_info *)malloc(stream_info_size);
      if (audio_streams[i] == NULL)
	{
	  audio_stream_mem_error = true;
	  break;
	}
    }

  if ((video_stream == NULL) || (audio_stream_mem_error == true)) {
    
    syslog(LOG_ERR, "Could not allocate memory to parse input file %s", cur_job_info->localfilepath);
    
    if (video_stream != NULL) {
      free(video_stream);
      video_stream = NULL;
    }

    for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
      free_mem(audio_streams[i]);

    return -1;

  } else {

    memset(video_stream, 0, sizeof(struct stream_info));
    for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
      memset(audio_streams[i], 0, sizeof(struct stream_info));

    memset(file_name, 0, *expected_filename_len);

    if (*opcode == DISCOVER_CMD)
      sprintf(file_name, "%s.json", cur_job_info->input_filename_prefix);
    else if (*opcode == DISCOVER_OUTPUT_CMD)
      sprintf(file_name, "%s-%s-output.json", cur_job_info->input_filename_prefix, cur_job_info->id);

    if (parse_ffprobe_output(cur_job_info, (const char *)file_name, video_stream, &audio_streams[0], opcode) < 0)
      {

	syslog(LOG_ERR, "= Job_ID: %s | Could not parse ffprobe output!", cur_job_info->id);
	if (*opcode == DISCOVER_CMD)
	  report_tcode_error_to_DB(cur_job_info, cmd_to_exec, -1, opcode, "failed to parse ffprobe's output.");
	else if (*opcode == DISCOVER_OUTPUT_CMD) 
	  {
	    int cur_opcode = TCODE_CMD;
	    report_tcode_error_to_DB(cur_job_info, cmd_to_exec, -1, &cur_opcode, "invalid output file - check transcode configuration");
	  }

	free_mem(video_stream->name);
	free_mem(video_stream);
	for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
	  {
	    free_mem(audio_streams[i]->name);
	    free_mem(audio_streams[i]);
	  }

	return -1;
      }
    

    if (video_stream->parsed == true) {
      
      syslog(LOG_INFO, "= Job_ID: %s | Video characteristics: %ux%u, %0.2f, %0.2f, %u, %u, %s, size: %lu, duration: %0.2f\n", 
	     cur_job_info->id,
	     video_stream->width, 
	     video_stream->height, 
	     video_stream->bitrate, 
	     video_stream->framerate, 
	     video_stream->num_PES_frames, 
	     video_stream->codec_type, video_stream->name,
	     cur_job_info->segment_size,
	     cur_job_info->estimated_duration);

      if ( (get_config()->sched_version == SCHED_VERSION_HOSTNAME_BASED) && (video_stream->codec_type == FILEAPP_HEVC_CODEC_TYPE) && (get_config()->hevc_mode == 0) )
	{
	  cur_job_info->unsupported_input_codec = true;
	  report_tcode_error_to_DB(cur_job_info, NULL, -1, opcode, "Codec not supported");
	  free_mem(video_stream->name);
	  free_mem(video_stream);
	  for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
	    {
	      free_mem(audio_streams[i]->name);
	      free_mem(audio_streams[i]);
	    }
	  return -1;
	}


      
    }

    cur_job_info->num_audio_streams = 0;
    for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
      {    
	if (audio_streams[i]->parsed == true) 
	  {

	    syslog(LOG_DEBUG, "=== Audio[%d]: %0.2f, %u, %u, %u, %s\n", i,
		   audio_streams[i]->bitrate, audio_streams[i]->sample_rate, audio_streams[i]->num_PES_frames, audio_streams[i]->codec_type, audio_streams[i]->name);
	    
	    cur_job_info->num_audio_streams += 1;

	  }
      }

    if (*opcode == DISCOVER_CMD)
      {
    
	if (video_stream->parsed == false)
	  {
	    report_tcode_error_to_DB(cur_job_info, cmd_to_exec, -1, opcode, "Sync byte not found.");
	
	    free_mem(video_stream->name);
	    free_mem(video_stream);
	    for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
	      {
		free_mem(audio_streams[i]->name);
		free_mem(audio_streams[i]);
	      }

	    return -1;
	  }
	else
	  {
	    if (((cur_job_info->enhanced_mode) && (cur_job_info->chunk_num == 0)) ||
		(cur_job_info->enhanced_mode == false)) 
	      {
      
		cur_job_info->num_video_streams = 1;	   

		if (cur_job_info->url_flag) 
		  cur_job_info->bytes_discovered = cur_job_info->bytes_pulled;
		else
		  {
		    if (cur_job_info->enhanced_mode == false)
		      cur_job_info->bytes_discovered = cur_job_info->input_media_size;
		    else
		      cur_job_info->bytes_discovered = cur_job_info->segment_size;
		  }
		
		bytes_done = cur_job_info->input_media_size * 0.02; // report that 1% is done
		if ((tcode_json = get_json_for_DB_reporting((!strcmp(cur_job_info->type, "discover")) ? "discover" : "transcode",
							    "start",
							    get_config()->unique_signature,
							    &bytes_done,
							    &cur_speed,
							    (double *)cpu_util,
							    NULL,
							    cur_job_info,
							    video_stream,
							    &audio_streams[0],
							    NULL)) == NULL)
		  {
		    syslog(LOG_ERR, "= Job_ID: %s | Could not get tcode_json for reporting transcode start\n", cur_job_info->id);
		  }
		else
		  {
		    if (!get_config()->cmdline_mode)
		      {
			syslog(LOG_INFO, "= Job_ID: %s | Conveying %s to DB\n", cur_job_info->id, tcode_json);	
			//convey_json_to_db(&cur_job_info->curl_ctrl, get_config()->db_server, cur_job_info->id, tcode_json, true);    
			update_db_with_job_state_with_retries(cur_job_info, tcode_json);
		      }
		    else
		      syslog(LOG_INFO, "= Job_ID: %s | Discover complete: %s", cur_job_info->id, tcode_json);
		  }
	      }
	  }

	free_mem(tcode_json);
      }
    
    if (*opcode == DISCOVER_CMD)    
      free_mem(video_stream->name);
    free_mem(video_stream);
    for (i = 0; i< MAX_EXPECTED_AUDIO_STREAMS; i++)
      {
	if (*opcode == DISCOVER_CMD)
	  free_mem(audio_streams[i]->name);
	free_mem(audio_streams[i]);
      }


  }
  
  return 0;
}
*/


static bool child_process_active(pid_t pid, int status, char *job_id, int *echild_done) 
{
  bool retval = true;
  
  if (pid == 0)
    {

      /* if WNOHANG was specified and one or more child(ren) specified by pid exist, 
	 but have not yet changed state, then 0 is returned by waitpid.

	 DO NOTHING
      */
      ;	 

    }
  else if (pid > 0)
    {
      /* This is the common case */
      if (WIFEXITED(status)) 
	{
	  syslog(LOG_INFO, "= Job_ID: %s | pid: %d | WIFEXIT: %d/%d \n", job_id, pid, 
		 WIFEXITED(status), WEXITSTATUS(status));
	  retval = false;
	} 
      else if (WIFSIGNALED(status)) 
	{
	  syslog(LOG_ERR, "= Job_ID: %s | pid: %d | WIFSIGNALED: %d/%d\n", job_id, pid,
		 WIFSIGNALED(status), WTERMSIG(status));
	  retval = false;
	} 
      else if (WIFSTOPPED(status)) 
	{
	  syslog(LOG_ERR, "= Job_ID: %s | Stopped by signal %d\n", job_id, WSTOPSIG(status));
	  
	  /* Send the child process a SIGCONT signal to wake it up. */
	  if (kill(pid, SIGCONT) < 0) 
	    syslog(LOG_ERR, "= Job_ID: %s | Could not send continue signal to child_pid: %d, %s", job_id, pid, strerror(errno));
	
	} 
    }
  else if (pid < 0) 
    {      
      
      if (errno == ECHILD)
	{
      	  retval = false;
	  *echild_done = 1;
	  syslog(LOG_INFO, "= Job_ID: %s | pid: %d | WIFEXIT: %d/%d SIGNAL: %d/%d  | ECHILD: %s, %s:%d", 
		 job_id, pid, 
		 WIFEXITED(status), WEXITSTATUS(status),
		 WIFSIGNALED(status), WTERMSIG(status), strerror(errno), __FILE__, __LINE__);
	}
      else if (errno == EINVAL) 
	{
	  syslog(LOG_ERR, "= Job_ID: %s | Invalid options to waitpid: %s\n", job_id, strerror(errno));
	}
      else
	{
	  /* Other weird errors.  */
	  syslog(LOG_ERR, "= Job_ID: %s | Weird error in waitpid: %s\n", job_id, strerror(errno));
	  retval = false;
	}
      
    }
  
  return retval;

}

/*
static int handle_successful_execution(char *cmd_to_exec_args, 
				       int *child_exit_status);
{
  int retval = 0;
  
  if (*opcode == DISCOVER_CMD)
    {
	  
      unsigned int filename_len = 0; 
      filename_len = snprintf(NULL, 0, "%s.json", cur_job_info->input_filename_prefix) + 1;
      if (parse_inputfile_stats_and_report_to_DB(cur_job_info, cmd_to_exec_args, opcode, cpu_util, &filename_len) < 0)
	retval = -1;

    }
  else if ( (*opcode == TMUX_CMD) || (*opcode == DEMUX_VIDEO_CMD) || (*opcode == DEMUX_AUDIO_CMD) || 
	    (*opcode == DEMUX_OUTPUT_VIDEO_CMD) || (*opcode == DEMUX_OUTPUT_AUDIO_CMD) || 
	    (*opcode == GEN_VIX_OUTPUTFILE_CMD) || (*opcode == FFMPEG_TMUX_CMD) || (*opcode == DEMUX_OUTPUT_PS_CMD) ||
	    (*opcode == TMUX_OUTPUT_MP4_CMD) || (*opcode == TMUX_OUTPUT_MOV_CMD) || 
	    (*opcode == TMUX_OUTPUT_3GP_CMD) || (*opcode == TMUX_OUTPUT_FLV_CMD) || (*opcode == GENTRACKS_OUTPUT_CMD) || (*opcode == M3U8_OUTPUT_CMD))
    {

      u64 output_filesize = 0;
      char *expected_output_filename = NULL;

      if (*opcode == DEMUX_VIDEO_CMD)
	expected_output_filename = cur_job_info->input_video_stream;
      else if (*opcode == DEMUX_AUDIO_CMD)
	expected_output_filename = cur_job_info->input_audio_stream;
      else if (*opcode == DEMUX_OUTPUT_VIDEO_CMD)
	expected_output_filename = cur_job_info->output_video_stream;
      else if (*opcode == DEMUX_OUTPUT_AUDIO_CMD)
	expected_output_filename = cur_job_info->output_audio_stream;      
      else if ( (*opcode == TMUX_CMD) || (*opcode == FFMPEG_TMUX_CMD) )
	expected_output_filename = cur_job_info->transmuxed_filepath;
      else if (*opcode == GEN_VIX_OUTPUTFILE_CMD)
	expected_output_filename = cur_job_info->output_vix_file;
      else if ( (*opcode == DEMUX_OUTPUT_PS_CMD) || (*opcode == TMUX_OUTPUT_MP4_CMD) || (*opcode == TMUX_OUTPUT_MOV_CMD) || 
		(*opcode == TMUX_OUTPUT_3GP_CMD) || (*opcode == TMUX_OUTPUT_FLV_CMD) )
	expected_output_filename = cur_job_info->output_program_stream;
      else if (*opcode == GENTRACKS_OUTPUT_CMD) 
	expected_output_filename = cur_job_info->output_tracks_json;      
      else if (*opcode == M3U8_OUTPUT_CMD) 
	expected_output_filename = cur_job_info->m3u8_output_filename;      

      retval = fsize(expected_output_filename, &output_filesize);
      if (retval < 0)
	syslog(LOG_ERR, "= Job_ID: %s | Could not get output filesize for %s", cur_job_info->id, expected_output_filename);
      else
	{
	  if ( (*opcode == TMUX_CMD) || (*opcode == FFMPEG_TMUX_CMD) || (*opcode == TMUX_OUTPUT_MP4_CMD) || (*opcode == TMUX_OUTPUT_MOV_CMD) || 
	       (*opcode == TMUX_OUTPUT_3GP_CMD) || (*opcode == TMUX_OUTPUT_FLV_CMD) )
	    syslog(LOG_INFO, "= Job_ID: %s | Transmux completed and output filesize for %s is: %lu", cur_job_info->id, expected_output_filename, output_filesize);
	  else if (*opcode == GEN_VIX_OUTPUTFILE_CMD)
	    syslog(LOG_INFO, "= Job_ID: %s | VIX output generated and output filesize for %s is: %lu", cur_job_info->id, expected_output_filename, output_filesize);
	  else if (*opcode == GENTRACKS_OUTPUT_CMD)
	    syslog(LOG_INFO, "= Job_ID: %s | Output tracks file generated and output filesize for %s is: %lu", cur_job_info->id, expected_output_filename, 
		   output_filesize);
	  else if (*opcode == M3U8_OUTPUT_CMD)
	    syslog(LOG_INFO, "= Job_ID: %s | m3u8 file generated and output filesize for %s is: %lu", cur_job_info->id, expected_output_filename, output_filesize);
	  else
	    syslog(LOG_INFO, "= Job_ID: %s | Demux completed and output filesize for %s is: %lu", cur_job_info->id, expected_output_filename, output_filesize);
	}
    }    
  
  return retval;
}

      
static void update_filesize_state(struct per_job_info *cur_job_info, 
				  u64 *wait_cycles, 
				  i64 *prevsize, 
				  i64 *cursize,
				  long double *cpu_util)
{
  if (*cursize >= 0)
    {

      *prevsize = *cursize;

      if ((fsize(cur_job_info->tmp_output_filename, (u64 *)cursize)) == 0)
	{
	  syslog(LOG_INFO, "= Job_ID: %s | Tracking progress: chunk_num=%d | size @ start (%lu): prev=%ld, cur=%ld (diff=%ld) | cpu_util=%0.4LF\n", 
		 cur_job_info->id, cur_job_info->chunk_num,
		 *wait_cycles, *prevsize, *cursize, *cursize - *prevsize, *cpu_util);
	}
	  
    }
  else
    {
      
      // Initialization
      if ((fsize(cur_job_info->tmp_output_filename, (u64 *)prevsize)) == 0)
	{
	  syslog(LOG_INFO, "= Job_ID: %s | Tracking progress: chunk_num=%d | size @ start (%lu): prev=%ld, cur=%ld | cpu_util=%0.4LF\n", 
		 cur_job_info->id, cur_job_info->chunk_num,
		 *wait_cycles, *prevsize, *cursize, *cpu_util);
	}
      
    }

  return;
}

static bool 
check_and_act_on_fileapp_hang_or_abort(struct per_job_info *cur_job_info, 
				       double *total_wait_cycles, 
				       u64 *wait_cycles, 
				       int *childpid, 
				       i64 *prevsize, 
				       i64 *cursize,
				       long double *cpu_util)
{
  
  //Returns FALSE if SIGKILL is sent to child 
  
  
  bool retval = true;
  u64 cur_quit_check_duration = 0;

  *wait_cycles = *wait_cycles + 1;
  
  // to deal with fileapp hangs
  if ((*total_wait_cycles <= HANG_THRESHOLD_OBSERVED) && (!strcmp(cur_job_info->video_codec, "hevc")))
    cur_quit_check_duration = QUIT_CHECK_DURATION_HANG;
  else
    cur_quit_check_duration = QUIT_CHECK_DURATION_DISKFULL;
  
  if (!strcmp(cur_job_info->operating_mode, "activitydetection"))
    cur_quit_check_duration *= QUIT_CHECK_DURATION_ACTIVITY_DETECTION_MULTIPLIER;

  *wait_cycles = *wait_cycles % cur_quit_check_duration;
 
  //Record filesize, QUIT_CHECK_DURATION_START seconds into the 
  //current monitoring period.
  if (*wait_cycles == QUIT_CHECK_DURATION_START)
    {
      update_filesize_state(cur_job_info, wait_cycles, prevsize, cursize, cpu_util);
    }
  
    // Sample size of output file and send SIGKILL if needed 
  if (*wait_cycles == (cur_quit_check_duration-1))
    {

      if (*cursize >= 0)
	*prevsize = *cursize;

      if ((fsize(cur_job_info->tmp_output_filename, (u64 *)cursize)) == 0)
	{
	  syslog(LOG_INFO, "= Job_ID: %s | Tracking progress: chunk_num=%d | size @ check (%lu): prev=%lu, cur=%lu (diff=%lu) | cpu_util=%0.4Lf\n", 
		 cur_job_info->id, cur_job_info->chunk_num,
		 *wait_cycles, *prevsize, *cursize, *cursize - *prevsize, *cpu_util);
	}
      
      if (cur_quit_check_duration == QUIT_CHECK_DURATION_DISKFULL)
	{
	  if (cur_job_info->prev_cpu_util == -1)
	    {
	      cur_job_info->prev_cpu_util = *cpu_util;
	      cur_job_info->num_consec_low_cpu_util_intervals = 0;
	    }
	  else
	    {
	      
	      cur_job_info->cur_cpu_util = *cpu_util;
	      syslog(LOG_INFO, "= Job_ID: %s | Tracking progress: chunk_num=%d | size @ check (%lu): prev=%lu, cur=%lu (diff=%lu) | cpu_util=%0.4Lf (%0.4Lf, %0.4Lf, %d)", 
		     cur_job_info->id, cur_job_info->chunk_num,
		     *wait_cycles, *prevsize, *cursize, *cursize - *prevsize, 
		     *cpu_util, cur_job_info->prev_cpu_util, cur_job_info->cur_cpu_util,
		     cur_job_info->num_consec_low_cpu_util_intervals);

	      if ((cur_job_info->cur_cpu_util <= 0.01) && (cur_job_info->prev_cpu_util <= 0.01))
		cur_job_info->num_consec_low_cpu_util_intervals += 1;
	      else
		cur_job_info->num_consec_low_cpu_util_intervals = 0;

	      if ((*cursize - *prevsize) <= LOW_OUTPUT_THRESHOLD_IN_BYTES_PER_MIN)
		cur_job_info->num_consec_low_output_intervals += 1;
	      else
		cur_job_info->num_consec_low_output_intervals = 0;

	      cur_job_info->prev_cpu_util = cur_job_info->cur_cpu_util;

	    }
	}
      

      if ( (*cursize >= 0) && (*prevsize >= 0) && (strcmp(cur_job_info->video_codec, "hevc")) && (*cursize == *prevsize) && 
	   (cur_job_info->num_consec_low_cpu_util_intervals >= LOW_CPU_UTIL_THRESHOLD) )
	{
	  syslog(LOG_ERR, 
		 "= Job_ID: %s | Tracking progress: chunk_num=%d | Output file %s size constant for %d sec OR low CPU utilization for >= 5 min OR low output size signalling slower-than-expected progress; SIGKILL child pid %d | cur=%lu, prev=%lu (diff=%lu) | cpu_util=%0.4Lf | low_output_intervals: %d | low_cpu_util_ivals: %d",
		 cur_job_info->id, cur_job_info->chunk_num,
		 cur_job_info->tmp_output_filename, 
		 (int)(cur_quit_check_duration-QUIT_CHECK_DURATION_START), 
		 *childpid,
		 *cursize, *prevsize, *cursize - *prevsize, *cpu_util,
		 cur_job_info->num_consec_low_output_intervals,
		 cur_job_info->num_consec_low_cpu_util_intervals);
		      
	  if (kill(*childpid, SIGKILL) < 0) 
	    syslog(LOG_ERR, "Could not send SIGKILL signal to child_pid:%d, %s", *childpid, strerror(errno));

	  retval = false;
	    
	}

    }

  if (check_if_job_was_aborted(cur_job_info) == 0)
    {
      
      syslog(LOG_ERR, "= Job_ID: %s | Found abort file and killing active child process with pid: %d", cur_job_info->id, *childpid);

      if (kill(*childpid, SIGKILL) < 0) 
	syslog(LOG_ERR, "Could not send SIGKILL signal to child_pid:%d, %s", *childpid, strerror(errno));

      retval = false;

    }

  return retval;
}

void
report_tcode_update_to_DB(struct per_job_info *cur_job_info,
			  double *bytes,
			  double *speed,
			  double *cpu_util)
{
  char *tcode_json = NULL;

  if ((tcode_json = get_json_for_DB_reporting("transcode",
					      "update",
					      get_config()->unique_signature,
					      bytes,
					      speed,
					      cpu_util,
					      NULL,
					      cur_job_info,
					      NULL,
					      NULL,
					      NULL)) == NULL)
    {
      syslog(LOG_ERR, "Could not get tcode_json for reporting tcode update.\n");
    }
  else
    {	      
      syslog(LOG_DEBUG, "= Conveying %s to DB", tcode_json);
      convey_json_to_db(&cur_job_info->curl_ctrl, get_config()->db_server, cur_job_info->id, tcode_json, true);
      //update_db_with_job_state_with_retries(cur_job_info, tcode_json);
    }
      
  // Cleanup
  free_mem(tcode_json);

  return;

}

static void
calc_bytes_done(struct per_job_info *cur_job_info, double *wait_cyles, double *duration, int *total_frames, double *assumed_frame_proc_rate, double *bytes_done)
{

  if (cur_job_info->enhanced_mode) 
    {
      *bytes_done = ( (*assumed_frame_proc_rate * *wait_cyles) / *total_frames ) * (double)cur_job_info->segment_size;
      if (*bytes_done > cur_job_info->segment_size)
	*bytes_done = ((*duration-1) / *duration) * (double)cur_job_info->segment_size;
    }
  else
    {
      *bytes_done = ( (*assumed_frame_proc_rate * *wait_cyles) / *total_frames ) * (double)cur_job_info->input_media_size;
      if (*bytes_done > cur_job_info->input_media_size)
	*bytes_done = ((*duration-1) / *duration) * (double)cur_job_info->input_media_size;
    } 

  return;

}

static void 
check_progress_and_report_if_needed(struct per_job_info *cur_job_info, double *total_wait_cycles, long double *cpu_util)
{
  double cur_speed = 0, bytes_done = 0;
  double adjusted_duration = cur_job_info->estimated_duration * CONSERVATIVE_SPEED_ESTIMATE;
  int total_video_frames = adjusted_duration * cur_job_info->input_video_framerate;

  *total_wait_cycles += 1;
  if (((u64)*total_wait_cycles % QUIT_CHECK_DURATION_HANG) == 0)
    {
      if (!strcmp(cur_job_info->operating_mode, "activitydetection"))
	{
	  double assumed_proc_speed = 12.0; // fps
	  calc_bytes_done(cur_job_info, total_wait_cycles, &adjusted_duration, &total_video_frames, &assumed_proc_speed, &bytes_done);	  
	}
      else
	{
	  double assumed_proc_speed = cur_job_info->input_video_framerate; // fps
	  if (!strcmp(cur_job_info->video_codec, "hevc"))
	    assumed_proc_speed /= 3.0;
	  calc_bytes_done(cur_job_info, total_wait_cycles, &adjusted_duration, &total_video_frames, &assumed_proc_speed, &bytes_done);	  
	}

      if (bytes_done == 0)
	bytes_done = cur_job_info->input_media_size * 0.02; // report that 1% is done
      else
	{
	  if (bytes_done < (cur_job_info->input_media_size * 0.02))
	    bytes_done = cur_job_info->input_media_size * 0.02;
	}

      syslog(LOG_INFO, "= Job_ID: %s | reporting progress | chunk_num=%d | total_wait_cycles: %0.2fs | adjusted_duration: %0.2fs/%0.2fs | bytes_done: %0.2f | cpu_util: %0.4Lf\n",
	     cur_job_info->id, 
	     cur_job_info->chunk_num,
	     *total_wait_cycles, 
	     adjusted_duration, 
	     cur_job_info->estimated_duration,
	     bytes_done,
	     *cpu_util);
	     
      // Report transcode update to DB 
      report_tcode_update_to_DB(cur_job_info, &bytes_done, &cur_speed, (double *)cpu_util);

    }		

}
*/

int
get_free_space_available_in_bytes(char *target_path,
				  long unsigned *free_space,
				  char **json_err)
{

  struct statvfs sfs;
  int retval = 0;

  /* http://stackoverflow.com/questions/4965355/converting-statvfs-to-percentage-free-correctly/4965511#4965511 */
  if ((retval = statvfs(target_path, &sfs)) != -1)
    {

      // total = sfs.f_blocks * sfs.f_frsize / (1024 * 1024);
      // free = sfs.f_bfree * sfs.f_frsize / (1024 * 1024);
      //printf("Space available at %s: %lu, %lu, %lu\n", target_path, total, available, free);
      *free_space = sfs.f_bavail * sfs.f_frsize;
      syslog(LOG_DEBUG, "= Free space available at %s: %lu\n", target_path, *free_space);

    }

  return retval;

}

static bool disk_space_full(char *localpath)
{
  long unsigned free_space_available = 0;
  char *error_str = NULL;
  bool retval = false;
  
  if ((get_free_space_available_in_bytes(localpath, &free_space_available, &error_str)) != 0) 
    {		      
      syslog(LOG_ERR, "%s.\n", error_str);
    } 
  else 
    {
      
      syslog(LOG_DEBUG, "= Free space available: %lu\n", free_space_available);
      
      if (free_space_available == 0)
	retval = true;

    }

  return retval;

}

int 
fork_and_execve(char *cmd_to_exec_args, char *output_file_expected, char *localpath, int *echild_done) 
{

  int retval = 0, status = -1, child_pid = -1, child_exit_status = -1, wpid = -1, killed_due_to_hang = 0;
  u64 num_wait_cycles = 0;
  char *stderr_filename = NULL, *stdin_filename = NULL, *cur_operation = "Transmux";
  int shell_terminal = STDIN_FILENO;
  int shell_is_interactive = isatty (shell_terminal);
  unsigned int exec_success = 0, local_exec_retries = 0;
  struct timespec ts;

  ts.tv_sec = 0;
  ts.tv_nsec = 1000000000L; // 10^9 ns or 1s

  /* We fork a child process to execute the fileapp and retry the operation if unsuccessful for atleast MAX_EXEC_RETRIES */
  do {

    if (local_exec_retries > 0)
      nanosleep(&ts, NULL); // Sleep for 1s before trying again 

    /* child process to run fileapp */
    if ((child_pid = fork()) < 0) {
      syslog(LOG_ERR, "= Forking fileapp via system failed: %s", strerror(errno));
      return -1;
    }
    
    if (child_pid == 0) 
      { 
	/* child */
	
	char **cmd_args = NULL;
	char *newenviron[] = { NULL };
	int stderr_fd = -1, stdin_fd = -1, cur_prio = -1, new_prio = -1, max_retries = 5, num_retries = 0; 
	
	syslog(LOG_DEBUG, "= Child 1: pgrp=%d pid=%d child_pid:%d interactive_shell=%d", 
	       getpgrp(), getpid(), child_pid, shell_is_interactive);
	
	// Make yourself process group leader (fileapp behaves better with this, i.e. produces output files as it should
	if (setpgid(0,0) < 0)
	  { 
	    syslog(LOG_ERR, "= Could not make myself process group leader: %s\n", strerror(errno));
	  }
	
	if (shell_is_interactive)
	  { 
	    
	    // put child in foreground
	    if (tcsetpgrp(shell_terminal, getpgrp()) < 0)
	      { 
		syslog(LOG_ERR, "= Could not put child in foreground (tcsetpgrp): %s\n", strerror(errno));
	      }
	    
	  }
	
	/* Set the handling for job control signals back to the default.  */
	signal (SIGINT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);
	signal (SIGTSTP, SIG_DFL);
	signal (SIGTTIN, SIG_DFL);
	signal (SIGTTOU, SIG_DFL);
	signal (SIGCHLD, SIG_DFL);
	
	int a_retval = asprintf(&stderr_filename, "%s/ffmpeg.out", localpath);
	if (a_retval < 0)
	  {
	    syslog(LOG_ERR, "= Could not asprintf stderr_filename");
	    return -1;
	  }
	
	while ((stderr_fd = TEMP_FAILURE_RETRY (open(stderr_filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR))) < 0) {
	  //while ((stderr_fd = TEMP_FAILURE_RETRY (open(stderr_filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR))) < 0) {
	  
	  syslog(LOG_ERR, "Could not open output file %s for %s. Retrying in 1 second.\n", stderr_filename, cur_operation);
	  
	  num_retries += 1;
	  
	  if (num_retries < max_retries)
	    sleep(1);
	  else
	    {
	      syslog(LOG_CRIT, "Failed to open stderr file %s for %s after %d retries. Exiting.\n", 
		     stderr_filename,
		     cur_operation,
		     max_retries);
	      free_mem(stderr_filename);
	      exit(-1);
	    }
	}
	
	dup2(stderr_fd, 1); // make stdout go to file and close stdout
	dup2(stderr_fd, 2); // make stderr go to file and close stderr
	close(stderr_fd); // stderr_fd no longer needed - the dup'ed handles are sufficient
	stderr_fd = -1;
	free_mem(stderr_filename);
	
	num_retries = 0;
	if (asprintf(&stdin_filename, "%s.stdin", localpath) < 0)
	  {
	    syslog(LOG_ERR, "= Could not asprintf stdin_filename");
	    return -1;
	  }

	while ((stdin_fd = TEMP_FAILURE_RETRY (open(stdin_filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR))) < 0) {
	  
	  syslog(LOG_ERR, "Could not open stdin file %s for %s. Retrying in 1 second.\n", stdin_filename, cur_operation);
	  
	  num_retries += 1;
	  
	  if (num_retries < max_retries)
	    sleep(1);
	  else
	    {
	      syslog(LOG_CRIT, "Failed to open stdin file %s for %s after %d retries. Exiting.\n", 
		     stdin_filename,
		     cur_operation,
		     max_retries);
	      exit(-1);
	    }

	}
	
	dup2(stdin_fd, 0); // make stdin come from a file and close stdin
	close(stdin_fd);
	stdin_fd = -1;
	free_mem(stdin_filename);
	closelog();
	
	// cmd_args = argv_from_string("/usr/bin/ffmpeg", cmd_to_exec_args);
	cmd_args = argv_from_string("/bin/bash", cmd_to_exec_args);

	/* Lower current process priority */
	errno = 0;
	cur_prio = getpriority(PRIO_PROCESS, 0);
	if ((errno == ESRCH) || (errno == EINVAL))
	  {
	    syslog(LOG_ERR, "Could not get process priority so cannot lower it.");
	  }
	else
	  {
	    
	    syslog(LOG_DEBUG, "Current child process priority: %d\n", cur_prio);
	    
	    /* 
	       From http://linux.die.net/man/3/setpriority
	       
	       setpriority() shall set the nice values of all of the specified processes to value+ {NZERO}.
	       
	       The default nice value is {NZERO}; lower nice values shall cause more favorable scheduling. While 
	       the range of valid nice values is [0,{NZERO}*2-1], implementations may enforce more restrictive limits. 
	       
	       If value+ {NZERO} is less than the system's lowest supported nice value, setpriority() shall set the 
	       nice value to the lowest supported value; if value+ {NZERO} is greater than the system's highest 
	       supported nice value, setpriority() shall set the nice value to the highest supported value.
	       
	    */
	    new_prio = cur_prio + PROC_PRIORITY_INCREMENT;
	    syslog(LOG_DEBUG, "Lowering process priority from %d to %d\n", cur_prio, new_prio);
	    
	    if ((setpriority(PRIO_PROCESS, 0, new_prio)) < 0)
	      {
		syslog(LOG_ERR, "setpriority from %d to %d failed\n", cur_prio, new_prio);
	      }
	  }

	//retval = execve("/usr/bin/ffmpeg", cmd_args, newenviron);
	retval = execve("/bin/bash", cmd_args, newenviron);
	
	// Should not be here
	// report_tcode_error_to_DB(cur_job_info, cmd_to_exec_args, -1, &op_code, "execve failed!");   
	syslog(LOG_CRIT, "= execve failed, %s, %d", __FILE__, __LINE__);
	exit(retval);
	
      }
    else  
      {
	
	i64 prev_size = -1, cur_size = -1;
	double total_num_wait_cycles = 0;
	int num_times_killed = 0;
	
	/* parent */
	syslog(LOG_INFO, "= Parent 1: pgrp=%d pid=%d child_pid:%d term=%d", 
	       getpgrp(), getpid(), child_pid, tcgetpgrp(STDIN_FILENO));
	
	// Make child its own process group leader - to avoid race conditions
	if (setpgid(child_pid, child_pid) < 0)
	  {
	    if( errno != EACCES )
	      {
		syslog(LOG_ERR, "Could not make child its own process group leader: %s", strerror(errno));
	      }
	  }
      
	if (shell_is_interactive)
	  { 
	    
	    /* Put the job in foreground */
	    if (tcsetpgrp(shell_terminal, getpgid(child_pid)) < 0)
	    {
	      // we also do this in child                                                                                                                                
	      syslog(LOG_ERR, "Could not put child in foreground (tcsetgrp): %s", strerror(errno));
	    }
	    
	  }
		
	/* Send the process group a SIGCONT signal to wake it up. */
	if (kill(child_pid, SIGCONT) < 0)
	  syslog(LOG_ERR, "Could not send continue signal to child_pid %d, %s", child_pid, strerror(errno));
		  
	// syslog(LOG_INFO, "About to wait\n");
	do {
	  
	  sleep(1); // let child process run

	  // Check status
	  wpid = TEMP_FAILURE_RETRY(waitpid(child_pid, &status, WUNTRACED|WNOHANG));

	  
	} while(child_process_active(wpid, status, "1", echild_done));
	
	
	if ((WIFEXITED(status)) || (errno == ECHILD) || (*echild_done == 1))
	  { 
	    if (WIFEXITED(status))
	      child_exit_status = WEXITSTATUS(status);
	    else 
	      child_exit_status = 0;
	    
	    if ((child_exit_status == 0) && (access(output_file_expected, F_OK) != -1)) 
	      {	       

		double bytes_done = 0, cur_speed = 0;

		// int post_exec_retval = handle_successful_execution(cmd_to_exec_args, &child_exit_status);
		syslog(LOG_INFO, "= Transmux completed, %s:%d", __FILE__, __LINE__);
				
		/* Mark the flag for success of execution */
		exec_success = 1;

	      }
	    else 
	      {
		
		/* Check if disk ran out of space */
		/* if (disk_space_full(localpath) == true)
		  {
		    syslog(LOG_ERR, "disk out of space!, %s:%d", __FILE__, __LINE__);
		    break; // don't retry
		  }
		*/
		
		/* Dont report to DB unless MAX retry attempts have been reached, however do print it for debugging */
		if (local_exec_retries < (MAX_EXEC_RETRIES - 1)) 
		  {
		    
		    retval = 0;

		    syslog(LOG_ERR, "= %s for %s failed with exit_status: %d! Likely SIGKILL-ed\n", 
			   cur_operation, localpath, child_exit_status);
		    
		  }
		else 
		  {
		    retval = -1;
		    
		    syslog(LOG_ERR, "= %s for %s failed with exit_status: %d! Final attempt Done!\n", 
			   cur_operation, localpath, child_exit_status);
		    
		    /* if (disk_space_full(localpath) == true)
		      {
			// report_tcode_error_and_convey_abort_to_db(cur_job_info, &op_code, cmd_to_exec_args, &child_exit_status, "disk out of space!", true);
			syslog(LOG_ERR, "disk out of space!, %s:%d", __FILE__, __LINE__);
			break; // don't retry
			} */	
		  }
		
	      }
	    
	  }
	else 
	  {
	    
	    if (!(num_times_killed >= MAX_NUM_KILL_SIGNALS_THRESHOLD))
	      syslog(LOG_INFO, "= WSIGNALED | Outfile: %s | errno: %s", output_file_expected, strerror(errno));

	    /*
	    if (disk_space_full(localpath) == true)
	      {
		int exit_status = WTERMSIG(status);
		syslog(LOG_ERR, "disk out of space!, %s:%d", __FILE__, __LINE__);
		break; // don't retry
	      }
	    */
	    
	    /* Dont report to DB unless MAX retry attempts have been reached, however do print it for debugging */
	    if (local_exec_retries < (MAX_EXEC_RETRIES - 1)) 
	      {
		
		retval = 0;
		
		syslog(LOG_ERR, "= %s for %s failed with exit_status: %d! Likely SIGKILL-ed\n", 
		       cur_operation, localpath, child_exit_status);
		
	      }
	    else 
	      {
		retval = -1;
		
		syslog(LOG_ERR, "= %s for %s failed with exit_status: %d! Final attempt Done!\n", 
			   cur_operation, localpath, child_exit_status);

		/*
		if (disk_space_full(localpath) == true)
		  {
		    // report_tcode_error_and_convey_abort_to_db(cur_job_info, &op_code, cmd_to_exec_args, &child_exit_status, "disk out of space!", true);
		    syslog(LOG_ERR, "disk out of space!, %s:%d", __FILE__, __LINE__);
		    break; // don't retry
		  }
		*/
		
	      }
	    
	  }
      }
    
  } while ( ((++local_exec_retries) < MAX_EXEC_RETRIES ) && (!exec_success) );  

  // free_mem(cur_operation);

  if (shell_is_interactive)
    { 
      
      /* Put the shell back in the foreground.  */
      if (tcsetpgrp (shell_terminal, getpgrp()) < 0)
	{ 
	  syslog(LOG_ERR, "Could not put parent shell back in foreground: %s\n", strerror(errno));
	}
      
      
    }
  
  syslog(LOG_DEBUG, "= Parent 3: pgrp=%d pid=%d child_pid:%d term=%d\n", 
	 getpgrp(), getpid(), child_pid, tcgetpgrp(STDIN_FILENO));

  syslog(LOG_INFO, "= killed_due_to_hang: %s | retval: %d | local_exec_retries: %d | exec_success: %d", 
	 killed_due_to_hang == -1 ? "true" : "false", retval, local_exec_retries, exec_success);

  return retval;

}

int
prepare_and_exec_cmd(int *opcode,
		     char *cmd_args,
		     char *expected_summary_file,
		     int *echild_done)
{
  int retval = 0;

  /*                                                                                                                                                                             
    RACE CONDITION AVOIDANCE                                                                                                                                                     
                                                                                                                                                                                 
    Close-on-exec stdin, stdout and stderr on exec so that child process can redirect stderr to file                                                                             
    and avoid race condition described below.

    Without the lines below, there was a race condition where the child will generate a lot of 
    output on stderr and fill the OS write buffer. Parent meanwhile will be waiting to read 
    from stdout. Child will not write to stdout till Parent reads stderr (and empties it). Parent will 
    not proceed till it reads from stdout
    These commands set the close-on-exec flag for stdin, stdout and stderr file descriptors,
    which causes them to be automatically (and atomically) closed when any of the exec-family functions 
    succeed.

  */
  fcntl(0, F_SETFD, FD_CLOEXEC);
  fcntl(1, F_SETFD, FD_CLOEXEC);
  fcntl(2, F_SETFD, FD_CLOEXEC);

  if ((retval = fork_and_execve(cmd_args, expected_summary_file, get_config()->tmp_output_path, echild_done)) < 0)
    syslog(LOG_ERR, "= Executing cmd failed with return val: %d\n", retval);

  return retval;

}

