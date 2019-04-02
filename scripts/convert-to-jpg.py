import os
import sys
import subprocess
import argparse
import shutil
import json
import string
import logging
from PIL import Image

logging.basicConfig(filename="/mnt/bigdrive1/cnn/project1-training-convert-to-jpg.log", format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO)

parser = argparse.ArgumentParser(description="Convert training data provided to that required by the Darknet framework")
parser.add_argument('--inputfilepath', type=str, help='Path to input file with list of source JSON files to be processed', required=True)

if __name__ == '__main__':

    args = vars(parser.parse_args())
    
    inputfiles_to_process = []
    if os.path.exists(args["inputfilepath"]):
        with open(args["inputfilepath"], "r") as ifp:
            for line in ifp:
                stripped_line = line.strip()
                if os.path.exists("%s" % stripped_line):
                    inputfiles_to_process.append(stripped_line)
                else:
                    print "%s does not exist" % (line.strip())
    else:
        print "%s does not exist" % (args["inputfilepath"])
        exit(-1)

    logging.debug(" Command-line args: %s" % (str(args)))
    logging.info(" Number of files to process: %d" % (len(inputfiles_to_process)))
    
    # Validation set
    validation_filenames = ["4-2end to 93623.json"]
    
    # Test set (we have detection results without training for these videos)
    # json_filenames_to_notprocess = []
    test_filenames = []

    valtest_filenames = validation_filenames + test_filenames
    if ( (len(validation_filenames) != len(set(validation_filenames))) or \
         (len(test_filenames) != len(set(test_filenames))) or \
         (len(valtest_filenames) != len(set(valtest_filenames))) ):
         
        if (len(validation_filenames) != len(set(validation_filenames))):
            print "Validation filename list does not have all unique filenames"
                
        if (len(test_filenames) != len(set(test_filenames))):
            print "test filename list does not have all unique filenames"

        if (len(valtest_filenames) != len(set(valtest_filenames))):
            print "validation and test filename lists do not have all unique filenames"
            
        sys.exit(0)
    
    # json_filenames_to_notprocess = []
   
    logging.info(" Number of files in validation set: %d | test set: %d" % (len(validation_filenames), len(test_filenames)))

    global_img_idx = 0

    for filepath in inputfiles_to_process:

        json_filename_only = filepath.rsplit("/", 1)[1]        

        #if (json_filename_only in validation_filenames):
        #    continue
        
        logging.info(" Processing %s" % (filepath,))

        # Get the path for the images corresponding to this JSON file
        # This info. is used to construct the actual path of the images
        # as the path specified in the JSON file is incorrect
        # img_dir_path = filepath.replace(".json", "")
        img_dir_path = filepath.rsplit("/", 1)[0]
        logging.info(" img_dir_path: %s" % (img_dir_path,))

        with open(filepath, "r") as ifp:
            ifp_data = json.load(ifp)
            img_name_list = ifp_data["per_frame_info"]

            #print img_name_list
            
            logging.info(" Number of potential training images in %s: %d" % (filepath, len(img_name_list)))
            logging.info(" Number of expected training object images in %s: %d" % (filepath, ifp_data["objects_count"]))
            
            for img_name in img_name_list:
                logging.debug(" Working on %s" % (img_name,))
                # print "= Original img name: %s" % (img_name,)
                img_name_components = img_name.rsplit("/", 1)
                # print img_name_components
                if img_dir_path.endswith("/"):
                    input_imgfile_path = img_dir_path + img_name_components[1]
                    input_png_imgfile_path = img_dir_path + img_name_components[1].replace(".jpg", ".png")
                    convert_log_filepath = img_dir_path + img_name_components[1].replace(".jpg", ".log")
                else:
                    input_imgfile_path = img_dir_path + "/" + img_name_components[1]
                    input_png_imgfile_path = img_dir_path + "/" + img_name_components[1].replace(".jpg", ".png")
                    convert_log_filepath = img_dir_path + "/" + img_name_components[1].replace(".jpg", ".log")
                    
                logging.info(" Working on %s" % (input_imgfile_path,))
                
                if (os.path.exists("%s" % (input_imgfile_path,))):

                    # Copy image file to final destination
                    logging.info(" Moving %s to %s (logfile: %s)" % (input_imgfile_path, input_png_imgfile_path, convert_log_filepath,))
                    shutil.move(input_imgfile_path, input_png_imgfile_path)
                    
                    with open(convert_log_filepath, "w") as ofp_log:
                        logging.info(" Converting %s to %s" % (input_png_imgfile_path, input_imgfile_path,))
                        convert_cmd_to_exec = "/usr/bin/convert %s %s" % (input_png_imgfile_path, input_imgfile_path,)
                        ofp_log.write("%s\n" % (convert_cmd_to_exec))
                        ofp_log.flush()
                        subprocess.call(convert_cmd_to_exec, stdout=ofp_log, stderr=ofp_log, shell=True)
                        ofp_log.flush()
                        ffmpeg_info_cmd = "/usr/bin/ffmpeg -hide_banner -i \"%s\"" % (input_imgfile_path,)
                        ofp_log.write("%s\n" % (ffmpeg_info_cmd))
                        ofp_log.flush()
                        subprocess.call(ffmpeg_info_cmd, stdout=ofp_log, stderr=ofp_log, shell=True)

                    # shutil.copyfile(input_imgfile_path, final_imgfile_path)
                    
                            
                else:
                    logging.error(" %s does not exist!" % (input_imgfile_path,))


