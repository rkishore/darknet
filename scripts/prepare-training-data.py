import os
import subprocess
import argparse
import shutil
import json
import string

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
        
    print "= Command-line args: %s" % (str(args))
    print "= Number of files to process: %d" % (len(inputfiles_to_process))
    
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
        if json_filename_only in json_filenames_to_process:
            print "= Processing %s" % (filepath,)
            img_dir_path = filepath.replace(".json", "")
            print "= img_dir_path: %s" % (img_dir_path,)
            with open(filepath, "r") as ifp:
                ifp_data = json.load(ifp)
                img_name_list = ifp_data.keys()
                print "= Number of images to use in %s: %d" % (filepath, len(ifp_data.keys()))
                for img_name in img_name_list[0:1]:
                    # print "= Original img name: %s" % (img_name,)
                    img_name_components = img_name.rsplit("/", 1)
                    # print img_name_components
                    '''
                    if (args["tsfilepath"].endswith("/")):
                        input_imgfile_path = args["tsfilepath"] + img_name_components[1]
                    else:
                        input_imgfile_path = args["tsfilepath"] + "/" + img_name_components[1]

                    dirnames_to_replace = ["/Twilight_Collection/Axis_P1425-LE/",]
                    actual_dirnames_to_use = ["/Twilight_Collection_Vignettes_1_2_3/Axis P1425-LE/",]
                    for idx, dirname_to_replace in enumerate(dirnames_to_replace):
                        if dirname_to_replace in input_imgfile_path:
                            print "= Found %s in %s" % (dirname_to_replace, input_imgfile_path,)
                            input_imgfile_path = input_imgfile_path.replace(dirname_to_replace, actual_dirnames_to_use[idx])
                            break
                    '''
                    if img_dir_path.endswith("/"):
                        input_imgfile_path = img_dir_path + img_name_components[1]
                    else:
                        input_imgfile_path = img_dir_path + "/" + img_name_components[1]
                        
                    #print img_name, input_imgfile_path
                    if (os.path.exists("%s" % (input_imgfile_path,))):
                        input_txt_filepath = input_imgfile_path.rsplit(".jpg", 1)[0] + ".txt"
                        input_txt_filename_only = input_txt_filepath.rsplit("/", 1)[1]
                        if (args["outputfilepath"].endswith("/")):
                            final_txtfile_path = args["outputfilepath"] + input_txt_filename_only
                        else:
                            final_txtfile_path = args["outputfilepath"] + "/" + input_txt_filename_only
                        print "= Input_txt_filepath: %s, final_txt_filepath: %s" % (input_txt_filepath, final_txtfile_path,)
                    else:
                        print "= %s does not exist!" % (input_imgfile_path,)
                        
                    #subprocess.call(darknet_cmd, stdout=ofp_log, stderr=ofp_log, shell=True)
