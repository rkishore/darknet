import os
import subprocess
import argparse
import shutil

parser = argparse.ArgumentParser(description="Process dataset1 files")
parser.add_argument('--inputfilepath', type=str, help='Path to input file with list of source TS files to be processed', required=True)
parser.add_argument('--outputfilepath', type=str, help='Path to where MP4 files should be moved to', required=True)

inputfiles_to_process = []

def populate_input_ts_filelist(input_folder_path):
    global inputfiles_to_process
    for root, dirs, files in os.walk(input_folder_path):
        for filename in files:
            # print "= root=", root, " | filename: ",  filename
            if filename.endswith(".ts") or filename.endswith(".mpg"):
                # print "= root=", root, " | filename: ",  filename
                inputfiles_to_process.append(root + "/" + filename)
    print "= %d input files to process" % (len(inputfiles_to_process))

if __name__ == "__main__":
    
    args = vars(parser.parse_args())
    
    if os.path.exists(args["inputfilepath"]):

        populate_input_ts_filelist(args["inputfilepath"])

        if not (len(inputfiles_to_process) > 0):
            print "No TS files found. Quitting."
            exit(-1)
            
    else:
        print "%s does not exist" % (args["inputfilepath"])
        exit(-1)
        
    print "Number of files to process: %d" % (len(inputfiles_to_process))
    print args
    
    # Convert to MP4 
    for filepath in inputfiles_to_process:

        if (filepath.endswith(".ts") or filepath.endswith(".mpg")):
            
            if filepath.endswith(".ts"):
                mp4_filepath = filepath.rsplit(".ts", 1)[0] + ".mp4"
                ffmpeg_log_filepath = filepath.rsplit(".ts", 1)[0] + ".log"
            else:
                mp4_filepath = filepath.rsplit(".mpg", 1)[0] + ".mp4"
                ffmpeg_log_filepath = filepath.rsplit(".mpg", 1)[0] + ".log"
                
            mp4_filename_only = mp4_filepath.rsplit("/", 1)[1]
            ffmpeg_log_filename_only = ffmpeg_log_filepath.rsplit("/", 1)[1]
            if (args["outputfilepath"].endswith("/")):
                input_mp4file_path = args["outputfilepath"] + mp4_filename_only
                input_logfile_path = args["outputfilepath"] + ffmpeg_log_filename_only
            else:
                input_mp4file_path = args["outputfilepath"] + "/" + mp4_filename_only
                input_logfile_path = args["outputfilepath"] + "/" + ffmpeg_log_filename_only

            print filepath, mp4_filepath, input_mp4file_path, input_logfile_path
            
            with open(ffmpeg_log_filepath, "w") as ofp_log:
                print "Converting \"%s\" to \"%s\" (logfile: \"%s\")" % (filepath, mp4_filepath, ffmpeg_log_filepath)
                ffmpeg_cmd = "/usr/bin/ffmpeg -hide_banner -y -i \"%s\" -c:v copy -c:a copy -f mp4 \"%s\"" % (filepath, mp4_filepath)
                print "Using cmd: %s" % (ffmpeg_cmd)
                ofp_log.write("%s\n" % (ffmpeg_cmd))
                ofp_log.flush()
                subprocess.call(ffmpeg_cmd, stdout=ofp_log, stderr=ofp_log, shell=True)
                ofp_log.flush()
                ffmpeg_info_cmd = "/usr/bin/ffmpeg -hide_banner -i \"%s\"" % (mp4_filepath,)
                ofp_log.write("%s\n" % (ffmpeg_info_cmd))
                ofp_log.flush()
                subprocess.call(ffmpeg_info_cmd, stdout=ofp_log, stderr=ofp_log, shell=True)
                ofp_log.write("Moving %s to %s\n" % (mp4_filepath, input_mp4file_path))
                ofp_log.flush()
                shutil.move(mp4_filepath, input_mp4file_path)
                ofp_log.write("Moving %s to %s\n" % (ffmpeg_log_filepath, input_logfile_path))
                ofp_log.flush()
                shutil.move(ffmpeg_log_filepath, input_logfile_path)
            
