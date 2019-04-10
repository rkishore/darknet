import os
import sys
import subprocess
import argparse
import shutil
import json
import string
import logging
from PIL import Image

logging.basicConfig(filename="/mnt/bigdrive1/cnn/project1-training-check.log", format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO)

parser = argparse.ArgumentParser(description="Convert training data provided to that required by the Darknet framework")
parser.add_argument('--inputfilepath', type=str, help='Path to input file with list of source JSON files to be processed', required=True)
parser.add_argument('--outputfilepath', type=str, help='Path to where output cropped JPG files generated should be moved to', required=True)

categories = ["Cargo/Box Truck",
              "Dump Truck",
              "Bongo Truck",
              "Water Tanker Truck",
              "Standard Bus",
              "Standard Compactor",
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
                      "standard-bus",
                      "standard-compactor",
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
                    final_imgfile_path = args["outputfilepath"] + "%07d.jpg" % (global_img_idx,)
                else:
                    input_imgfile_path = img_dir_path + "/" + img_name_components[1]
                    final_imgfile_path = args["outputfilepath"] + "/" + "%07d.jpg" % (global_img_idx,)

                logging.info(" Working on %s" % (input_imgfile_path,))
                
                if (os.path.exists("%s" % (input_imgfile_path,))):

                    # Copy image file to final destination
                    # logging.info(" Copying %s to %s" % (input_imgfile_path, final_imgfile_path,))
                    # shutil.copyfile(input_imgfile_path, final_imgfile_path)

                    num_objects_in_training_img = len(ifp_data["per_frame_info"][img_name]["regions"])
                    if num_objects_in_training_img > 0:

                        # Get resolution of input image
                        #img_info = Image.open(input_imgfile_path)
                        #img_size_info = img_info.size

                        local_obj_idx = 0
                        for objinfo in ifp_data["per_frame_info"][img_name]["regions"]:
                                
                            x_topleft = float(objinfo["x"])
                            y_topleft = float(objinfo["y"])
                            img_width = float(objinfo["width"])
                            img_height = float(objinfo["height"])
                            x_bottomright = x_topleft + img_width
                            y_bottomright = y_topleft + img_height

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
                                
                            logging.info(" Generating cropped img from %s to %s | coordinates: (%0.1f, %0.1f, %0.1f, %0.1f)" % (input_imgfile_path, final_imgfile_path, \
                                                                                                                                x_topleft, y_topleft, x_bottomright, y_bottomright))
                            
                            crop(input_imgfile_path, (x_topleft, y_topleft, x_bottomright, y_bottomright), final_imgfile_path)
                            local_obj_idx += 1

                    global_img_idx += 1
                            
                else:
                    logging.error(" %s does not exist!" % (input_imgfile_path,))


