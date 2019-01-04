import os
import subprocess
import argparse
import shutil
import json
import string
import logging
from PIL import Image

logging.basicConfig(format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO)

parser = argparse.ArgumentParser(description="Convert training data provided to that required by the Darknet framework")
parser.add_argument('--inputfilepath', type=str, help='Path to input file with list of source JSON files to be processed', required=True)
parser.add_argument('--tsfilepath', type=str, help='Path to source TS files', required=True)
parser.add_argument('--outputfilepath', type=str, help='Path to where output JPG and TXT files generated should be moved to', required=True)

if __name__ == "__main__":
    
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

    logging.debug("= Command-line args: %s" % (str(args)))
    logging.info(" Number of files to process: %d" % (len(inputfiles_to_process)))
    
    json_filenames_to_process = ["Axis_P1425-LE_Vis_Evening_ScenarioE1-1.ts_frames.json",\
                                 "Day_Vig4_Open1_HDEO_HD380C.ts_frames.json",\
                                 "Night_Vig4_Open1_HDIR_HD380.ts_frames.json",\
                                 "Pelco_Vis_Day_Scenario1-1.ts_frames.json",\
                                 "SSIII_IR_Day_Scenario1-1.ts_frames.json",\
                                 "SSIII_IR_Evening_ScenarioE1-1.ts_frames.json",\
                                 "SSIII_IR_Night_ScenarioN1-1.ts_frames.json",\
                                 "Scout_Recon_IR_Day_Scenario1-1.ts_frames.json",\
                                 "Scout_Recon_IR_Evening_ScenarioE1-1.ts_frames.json",\
                                 "Scout_Recon_IR_Night_ScenarioN1-1.ts_frames.json",\
                                 "Scout_Sony_Vis_Day_Scenario1-1.ts_frames.json",\
                                 "Scout_Sony_Vis_Evening_ScenarioE1-1.ts_frames.json",\
                                 "SSIII_IR_Evening_ScenarioE1-3.ts_frames.json",\
                                 "SSIII_IR_Evening_ScenarioE1-2.ts_frames.json",\
                                 "Axis_P1425-LE_Vis_Day_Scenario1-1.ts_frames.json",]
    
    '''
    json_filenames_to_process = ["20180927T044812_1_1.ts_frames.json",\
                                "20180927T044823_1_2.ts_frames.json",\
                                "20180927T053504_3_1.ts_frames.json",\
                                "20180918T083740_3_2.ts_frames.json",\
                                "20180913T160155_26_4.ts_frames.json",]
    '''   
    for filepath in inputfiles_to_process:
        json_filename_only = filepath.rsplit("/", 1)[1]        
        if json_filename_only in json_filenames_to_process[0:1]:
            logging.info(" Processing %s" % (filepath,))

            # Get the path for the images corresponding to this JSON file
            # This info. is used to construct the actual path of the images
            # as the path specified in the JSON file is incorrect
            img_dir_path = filepath.replace(".json", "")
            logging.debug(" img_dir_path: %s" % (img_dir_path,))

            with open(filepath, "r") as ifp:
                ifp_data = json.load(ifp)
                img_name_list = ifp_data.keys()
                
                logging.info(" Number of training images in %s: %d" % (filepath, len(ifp_data.keys())))

                for img_name in img_name_list[0:2]:
                    # print "= Original img name: %s" % (img_name,)
                    img_name_components = img_name.rsplit("/", 1)
                    # print img_name_components
                    if img_dir_path.endswith("/"):
                        input_imgfile_path = img_dir_path + img_name_components[1]
                        final_imgfile_path = args["outputfilepath"] + img_name_components[1]
                    else:
                        input_imgfile_path = img_dir_path + "/" + img_name_components[1]
                        final_imgfile_path = args["outputfilepath"] + "/" + img_name_components[1]
                        
                    # print img_name, input_imgfile_path
                    if (os.path.exists("%s" % (input_imgfile_path,))):

                        input_txtfile_path = input_imgfile_path.rsplit(".jpg", 1)[0] + ".txt"
                        input_txtfile_name_only = input_txtfile_path.rsplit("/", 1)[1]
                        if (args["outputfilepath"].endswith("/")):
                            final_txtfile_path = args["outputfilepath"] + input_txtfile_name_only
                        else:
                            final_txtfile_path = args["outputfilepath"] + "/" + input_txtfile_name_only
                        logging.debug(" Input_txt_filepath: %s, final_txt_filepath: %s" % (input_txtfile_path, final_txtfile_path,))

                        num_objects_in_training_img = len(ifp_data[img_name]["regions"])
                        if num_objects_in_training_img > 0:

                            # Get resolution of input image
                            img_info = Image.open(input_imgfile_path)
                            img_size_info = img_info.size

                            # Copy image file to final destination
                            logging.info(" Copying %s to %s" % (input_imgfile_path, final_imgfile_path,))
                            shutil.copyfile(input_imgfile_path, final_imgfile_path)

                            # Creating txt file with labeled info about image
                            logging.info(" Writing to %s" % (input_imgfile_path,))
                            with open(input_txtfile_path, "w") as ofp_txt:
                                for objinfo in ifp_data[img_name]["regions"]:

                                    x_topleft = float(objinfo["x"])
                                    y_topleft = float(objinfo["y"])
                                    img_width = float(objinfo["width"])
                                    img_height = float(objinfo["height"])
                                    x_center = x_topleft + float(img_width/2)
                                    y_center = y_topleft + float(img_height/2)

                                    img_width_normalized = img_width / img_size_info[0]
                                    img_height_normalized = img_height / img_size_info[1]
                                    x_center_normalized = x_center / img_size_info[0]
                                    y_center_normalized = y_center / img_size_info[1]

                                    if objinfo["category"] == "person":
                                        img_category_label = 0
                                    elif objinfo["category"] == "vehicle":
                                        img_category_label = 1

                                    logging.info("%d %0.6f(%0.6f, %0.6f) %0.6f(%0.6f, %0.6f) %0.6f(%0.6f) %0.6f(%0.6f) %0.1fx%0.1f" % \
                                                 (img_category_label, x_center_normalized, x_center, x_topleft, y_center_normalized, y_center, y_topleft, \
                                                  img_width_normalized, img_width, img_height_normalized, img_height, img_size_info[0], img_size_info[1]))
                                    ofp_txt.write("%d %0.6f %0.6f %0.6f %0.6f\n" % (img_category_label, x_center_normalized, y_center_normalized, \
                                                                                    img_width_normalized, img_height_normalized))

                            logging.info(" Moving %s to %s" % (input_imgfile_path, final_imgfile_path,))
                            shutil.move(input_txtfile_path, final_txtfile_path)

                    else:
                        logging.error(" %s does not exist!" % (input_imgfile_path,))
                        
                    #subprocess.call(darknet_cmd, stdout=ofp_log, stderr=ofp_log, shell=True)
