import os
import sys
import subprocess
import argparse
import shutil
import json
import string
import logging
from PIL import Image
import pprint

logging.basicConfig(filename="/mnt/bigdrive1/cnn/project1-training-renaming-2.log", format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO)

parser = argparse.ArgumentParser(description="Rename training data provided to check quality of data marked")
parser.add_argument('--inputfilepath', type=str, help='Path to input file with list of source JSON files to be processed', required=True)
parser.add_argument('--outputfilepath', type=str, help='Path to where output cropped JPG files generated should be moved to', required=True)

categories = ["Cargo/Box Truck",
              "Dump Truck",
              "Bongo Truck",
              "Water Tanker Truck",
              "Septic Tanker Truck",
              "Standard Bus",
              "Standard Compactor",
              "Standard Front Loader",
              "Stakebed Truck",
              "Flatbed Truck",
              "Container Semi Truck",
              "Container Tanker Truck",
              "Lowboy Semi Truck",
              "Pick Up Truck",
              "Mini Van",
              "Standard SUV",
              "Cargo Semi Truck",
              "2 wheel Motorcycle",
              "Other"]

categories_for_img = ["cargo-truck",
                      "dump-truck",
                      "bongo-truck",
                      "water-tanker-truck",
                      "septic-tanker-truck",
                      "standard-bus",
                      "standard-compactor",
                      "standard-front-loader",
                      "stakebed-truck",
                      "flatbed-truck",
                      "container-semi-truck",
                      "container-tanker-truck",
                      "lowboy-semi-truck",
                      "pick-up-truck",
                      "mini-van",
                      "standard-suv",
                      "cargo-semi-truck",
                      "motorcyle",
                      "other"]

total_categories = len(categories)

def crop(image_path, coords, saved_location):
    """
    @param image_path: The path to the image to edit
    @param coords: A tuple of x/y coordinates (x1, y1, x2, y2)
    @param saved_location: Path to save the cropped image
    """
    image_obj = Image.open(image_path)
    img_size_info = image_obj.size
    logging.info(" About to crop %s with resolution: %0.1f x %0.1f" % (image_path, img_size_info[0], img_size_info[1]))
    cropped_image = image_obj.crop(coords)
    cropped_image.save(saved_location)
    # cropped_image.show()


def convert_to_jpgfile_if_needed(img_dir_path, cutout_image_filepath):
    
    input_imgfile_path = cutout_image_filepath
    input_png_imgfile_path = cutout_image_filepath.replace(".jpg", ".png")
    convert_log_filepath = cutout_image_filepath.replace(".jpg", ".log")
    img_width = -1.0
    img_height = -1.0
    logging.info(" Working on %s (inside convert)" % (input_imgfile_path,))

    
    if (os.path.exists("%s" % (input_imgfile_path,))):

        # Copy image file to final destination
        if (not os.path.exists("%s" % (input_png_imgfile_path,))):
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

        else:
            logging.info("%s already exists. Conversion finished for %s" % (input_png_imgfile_path, input_imgfile_path,))

        image_obj = Image.open(input_imgfile_path)
        img_size_info = image_obj.size
        img_width = img_size_info[0]
        img_height = img_size_info[1]
        logging.info(" About to return info on %s with resolution: %0.1f x %0.1f" % (input_imgfile_path, img_size_info[0], img_size_info[1]))

    else:
        logging.error(" %s does not exist!" % (input_imgfile_path,))

    return input_imgfile_path, img_width, img_height

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
   
    logging.info(" Number of categories: %d" % (total_categories,)) 
    logging.info(" Number of files in validation set: %d | test set: %d" % (len(validation_filenames), len(test_filenames)))

    global_img_idx = 0
    num_train_cat_objects = [0] * total_categories
    num_valid_cat_objects = [0] * total_categories
    num_ftest_cat_objects = [0] * total_categories
    num_train_objects = 0
    num_valid_objects = 0
    num_ftest_objects = 0
    
    for filepath in inputfiles_to_process:

        json_filename_only = filepath.rsplit("/", 1)[1]        

        #if (json_filename_only in validation_filenames):
        #    continue
        
        logging.info(" Processing %s" % (filepath,))
        print " Processing %s" % (filepath,)
        
        # Get the path for the images corresponding to this JSON file
        # This info. is used to construct the actual path of the images
        # as the path specified in the JSON file is incorrect
        # img_dir_path = filepath.replace(".json", "")
        img_dir_path = filepath.rsplit("/", 1)[0]
        logging.info(" img_dir_path: %s" % (img_dir_path,))

        with open(filepath, "r") as ifp:
            ifp_data = json.load(ifp)
            frame_num_list = ifp_data["per_frame_info"]

            # print len(frame_num_list)
            #print frame_num_list
            
            logging.info(" Number of images marked for training in %s: %d" % (filepath, len(frame_num_list)))
            logging.info(" Number of expected training object images (objects_count) in %s: %d" % (filepath, ifp_data["objects_count"]))

            #pprint.pprint(frame_num_list)
            img_name_empty = False
            for img_name,val in frame_num_list.items():

                img_name_empty = False

                if len(img_name) > 0:

                    #print img_name
                    logging.info(" Working on frame %s" % (img_name,))
                    
                    num_objects_in_training_img = len(ifp_data["per_frame_info"][img_name]["regions"])

                    if num_objects_in_training_img > 0:

                        # Get resolution of input image
                        #img_info = Image.open(input_imgfile_path)
                        #img_size_info = img_info.size

                        local_obj_idx = 0
                        for objinfo in ifp_data["per_frame_info"][img_name]["regions"]:

                            if img_dir_path.endswith("/"):
                                input_imgfile_path = img_dir_path + objinfo["image_path"]
                            else:
                                input_imgfile_path = img_dir_path + "/" + objinfo["image_path"]

                            logging.info(" input_imgfile_path (base): %s" % (input_imgfile_path,))
                            
                            if (os.path.exists("%s" % (input_imgfile_path,))):

                                input_imgfile_path, measured_img_width, measured_img_height = convert_to_jpgfile_if_needed(img_dir_path, input_imgfile_path)
                                
                                x_topleft = 0
                                y_topleft = 0
                                img_width = float(objinfo["width"])
                                img_height = float(objinfo["height"])
                                x_bottomright = x_topleft + img_width
                                y_bottomright = y_topleft + img_height
                                x_center = x_topleft + float(img_width/2)
                                y_center = y_topleft + float(img_height/2)
                                
                                if (measured_img_width != img_width) or (measured_img_height != img_height):
                                    logging.info(" MISMATCH in resolution | measured: %0.1f x %0.1f | reported: %0.1f x %0.1f" % (measured_img_width, measured_img_height, img_width, img_height,))
                                else:
                                    logging.info(" MATCH in resolution | measured: %0.1f x %0.1f | reported: %0.1f x %0.1f" % (measured_img_width, measured_img_height, img_width, img_height,))


                                img_width_normalized = img_width / measured_img_width
                                img_height_normalized = img_height / measured_img_height
                                x_center_normalized = x_center / measured_img_width
                                y_center_normalized = y_center / measured_img_height

                                cur_category = objinfo["subcategory"].strip() + " " + objinfo["category"].strip()
                                if cur_category.startswith("Cargo/Box"):
                                    cur_category = "Cargo/Box Truck"
                                if "Trucks" in cur_category:
                                    cur_category = cur_category.replace("Trucks", "Truck")
                                if "Van" in cur_category:
                                    cur_category = cur_category.rsplit(" Van", 1)[0]

                                if cur_category in categories:
                                    img_category_label = categories.index(cur_category)
                                else:
                                    print "= Category other than expected: %s" % (cur_category,)
                                    cur_category = "Other"
                                    img_category_label = categories.index(cur_category)

                                if img_dir_path.endswith("/"):
                                    final_imgfile_path = args["outputfilepath"] + "%s-%07d-%d.jpg" % (categories_for_img[img_category_label], global_img_idx, local_obj_idx)
                                else:
                                    final_imgfile_path = args["outputfilepath"] + "/" + "%s-%07d-%d.jpg" % (categories_for_img[img_category_label], global_img_idx, local_obj_idx)

                                logging.info(" Copying %s to %s" % (input_imgfile_path, final_imgfile_path,))
                                shutil.copyfile(input_imgfile_path, final_imgfile_path)

                                local_obj_idx += 1
                                num_train_cat_objects[img_category_label] += 1
                                num_train_objects += 1
                                
                            else:
                                
                                logging.error(" %s does not exist!" % (input_imgfile_path,))
                    
                else:
                    logging.error("Empty key. Special processing")

                    img_name_empty = True

                global_img_idx += 1

        total_objects = num_train_objects + num_valid_objects + num_ftest_objects
        logging.info(" total | #(train objs): %d/%d (%0.2f%%) | #(valid objs): %d/%d (%0.2f%%) | #(ftest objs): %d/%d (%0.2f%%)" % \
                     (num_train_objects, total_objects, (num_train_objects * 100.0)/total_objects,\
                      num_valid_objects, total_objects, (num_valid_objects * 100.0)/total_objects,\
                      num_ftest_objects, total_objects, (num_ftest_objects * 100.0)/total_objects))
        print " total | #(train objs): %d/%d (%0.2f%%) | #(valid objs): %d/%d (%0.2f%%) | #(ftest objs): %d/%d (%0.2f%%)" % \
            (num_train_objects, total_objects, (num_train_objects * 100.0)/total_objects,\
             num_valid_objects, total_objects, (num_valid_objects * 100.0)/total_objects,\
             num_ftest_objects, total_objects, (num_ftest_objects * 100.0)/total_objects)
            
        for idx,val in enumerate(num_train_cat_objects):
            logging.info(" cat%d (%s) | #(train objs): %d/%d (%0.2f%%) | #(valid objs): %d/%d (%0.2f%%) | #(ftest objs): %d/%d (%0.2f%%) | total: %d/%d" % \
                         (idx, categories[idx], val, num_train_objects, (val * 100.0)/num_train_objects,\
                          num_valid_cat_objects[idx], num_valid_objects, ((num_valid_cat_objects[idx] * 100.0)/num_valid_objects if num_valid_objects > 0 else 0),\
                          num_ftest_cat_objects[idx], num_ftest_objects, ((num_ftest_cat_objects[idx] * 100.0)/num_ftest_objects if num_ftest_objects > 0 else 0),\
                          (val + num_valid_cat_objects[idx] + num_ftest_cat_objects[idx]), num_train_objects))
            print " cat%d (%s) | #(train objs): %d/%d (%0.2f%%) | #(valid objs): %d/%d (%0.2f%%) | #(ftest objs): %d/%d (%0.2f%%) | total: %d/%d" % \
                (idx, categories[idx], val, num_train_objects, (val * 100.0)/num_train_objects,\
                 num_valid_cat_objects[idx], num_valid_objects, ((num_valid_cat_objects[idx] * 100.0)/num_valid_objects if num_valid_objects > 0 else 0),\
                 num_ftest_cat_objects[idx], num_ftest_objects, ((num_ftest_cat_objects[idx] * 100.0)/num_ftest_objects if num_ftest_objects > 0 else 0),\
                 (val + num_valid_cat_objects[idx] + num_ftest_cat_objects[idx]), num_train_objects)

                    
