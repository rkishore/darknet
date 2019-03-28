import os
import sys
import subprocess
import argparse
import shutil
import json
import string
import logging
from PIL import Image

logging.basicConfig(filename="/mnt/bigdrive1/cnn/project1-training-prep.log", format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO)

parser = argparse.ArgumentParser(description="Convert training data provided to that required by the Darknet framework")
parser.add_argument('--inputfilepath', type=str, help='Path to input file with list of source JSON files to be processed', required=True)
parser.add_argument('--outputfilepath', type=str, help='Path to where output JPG and TXT files generated should be moved to', required=True)
parser.add_argument('--trainfilepath', type=str, help='Path to output train.txt file', required=True)
parser.add_argument('--validfilepath', type=str, help='Path to output valid.txt file', required=True)
parser.add_argument('--ftestfilepath', type=str, help='Path to output ftest.txt file', required=True)

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
              "Other"]

total_categories = len(categories)

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
    
    '''
    json_filenames_to_process = ["20180927T044812_1_1.ts_frames.json",\
                                "20180927T044823_1_2.ts_frames.json",\
                                "20180927T053504_3_1.ts_frames.json",\
                                "20180918T083740_3_2.ts_frames.json",\
                                "20180913T160155_26_4.ts_frames.json",]
    '''
    global_img_idx = 0
    ofp_train = open(args["trainfilepath"], "w")
    ofp_valid = open(args["validfilepath"], "w")
    ofp_ftest = open(args["ftestfilepath"], "w")
    json_files_found = []
    num_train_imgs = 0
    num_valid_imgs = 0
    num_ftest_imgs = 0
    num_train_objects = 0
    num_valid_objects = 0
    num_ftest_objects = 0
    num_train_cat_objects = [0] * total_categories
    num_valid_cat_objects = [0] * total_categories
    num_ftest_cat_objects = [0] * total_categories

    for filepath in inputfiles_to_process:
        json_filename_only = filepath.rsplit("/", 1)[1]        
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
                logging.info(" Working on %s" % (img_name,))
                # print "= Original img name: %s" % (img_name,)
                img_name_components = img_name.rsplit("/", 1)
                # print img_name_components
                if img_dir_path.endswith("/"):
                    input_imgfile_path = img_dir_path + img_name_components[1]
                    # final_imgfile_path = args["outputfilepath"] + img_name_components[1]
                    final_imgfile_path = args["outputfilepath"] + "%07d.jpg" % (global_img_idx,)
                else:
                    input_imgfile_path = img_dir_path + "/" + img_name_components[1]
                    # final_imgfile_path = args["outputfilepath"] + "/" + img_name_components[1]
                    final_imgfile_path = args["outputfilepath"] + "/" + "%07d.jpg" % (global_img_idx,)
                    
                # print img_name, input_imgfile_path
                if (os.path.exists("%s" % (input_imgfile_path,))):

                    input_txtfile_path = input_imgfile_path.rsplit(".jpg", 1)[0] + ".txt"
                    input_txtfile_name_only = input_txtfile_path.rsplit("/", 1)[1]
                    if (args["outputfilepath"].endswith("/")):
                        # final_txtfile_path = args["outputfilepath"] + input_txtfile_name_only
                        final_txtfile_path = args["outputfilepath"] + "%07d.txt" % (global_img_idx,)
                    else:
                        # final_txtfile_path = args["outputfilepath"] + "/" + input_txtfile_name_only
                        final_txtfile_path = args["outputfilepath"] + "/" + "%07d.txt" % (global_img_idx,)
                            
                    logging.debug(" Input_txt_filepath: %s, final_txt_filepath: %s" % (input_txtfile_path, final_txtfile_path,))

                    # Copy image file to final destination
                    logging.info(" Copying %s to %s" % (input_imgfile_path, final_imgfile_path,))
                    shutil.copyfile(input_imgfile_path, final_imgfile_path)

                    num_objects_in_training_img = len(ifp_data["per_frame_info"][img_name]["regions"])
                    if num_objects_in_training_img > 0:

                        # Get resolution of input image
                        img_info = Image.open(input_imgfile_path)
                        img_size_info = img_info.size
                        
                        # Creating txt file with labeled info about image
                        logging.info(" Writing to %s" % (input_txtfile_path,))
                    
                        with open(input_txtfile_path, "w") as ofp_txt:
                            for objinfo in ifp_data["per_frame_info"][img_name]["regions"]:

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
                                    
                                #else objinfo["category"] == "vehicle":
                                #    img_category_label = 1

                                logging.info(" Category: %s" % (cur_category))
                                logging.info(" %d %0.6f(%0.6f, %0.6f) %0.6f(%0.6f, %0.6f) %0.6f(%0.6f) %0.6f(%0.6f) %0.1fx%0.1f" % \
                                             (img_category_label, x_center_normalized, x_center, x_topleft, y_center_normalized, y_center, y_topleft, \
                                              img_width_normalized, img_width, img_height_normalized, img_height, img_size_info[0], img_size_info[1]))
                                ofp_txt.write("%d %0.6f %0.6f %0.6f %0.6f\n" % (img_category_label, x_center_normalized, y_center_normalized, \
                                                                                img_width_normalized, img_height_normalized))

                                if json_filename_only in test_filenames:
                                    num_ftest_objects += 1
                                    num_ftest_cat_objects[img_category_label] += 1
                                elif json_filename_only in validation_filenames:
                                    num_valid_objects += 1
                                    num_valid_cat_objects[img_category_label] += 1
                                else:
                                    num_train_objects += 1
                                    num_train_cat_objects[img_category_label] += 1
                                                
                        if json_filename_only in test_filenames:
                            logging.info(" Writing ftest imgpath %s of category %d from %s" % (final_imgfile_path, img_category_label, json_filename_only,))
                            ofp_ftest.write("%s\n" % (final_imgfile_path,))
                            ofp_ftest.flush()                            
                            num_test_imgs += 1
                            
                        elif json_filename_only in validation_filenames:
                            logging.info(" Writing validation imgpath %s of category %d from %s" % (final_imgfile_path, img_category_label, json_filename_only,))
                            ofp_valid.write("%s\n" % (final_imgfile_path,))
                            ofp_valid.flush()
                            num_valid_imgs += 1
                        else:
                            logging.info(" Writing training imgpath %s of category %d from %s" % (final_imgfile_path, img_category_label, json_filename_only,))
                            ofp_train.write("%s\n" % (final_imgfile_path,))
                            ofp_train.flush()
                            num_train_imgs += 1
                        
                    else:

                        logging.info(" Creating empty txt file %s" % (input_txtfile_path,))
                        ofp_txt = open(input_txtfile_path, "w")
                        ofp_txt.close()

                    global_img_idx += 1
                    logging.info(" Moving %s to %s" % (input_txtfile_path, final_txtfile_path,))
                    shutil.move(input_txtfile_path, final_txtfile_path)
                        
                else:
                    logging.error(" %s does not exist!" % (input_imgfile_path,))

    ofp_train.close()
    ofp_valid.close()
    ofp_ftest.close()

    total_imgs = num_train_imgs + num_valid_imgs + num_ftest_imgs
    logging.info(" total | #(train imgs): %d/%d (%0.2f%%) | #(valid imgs): %d/%d (%0.2f%%) | #(ftest imgs): %d/%d (%0.2f%%)" % \
                 (num_train_imgs, total_imgs, (num_train_imgs * 100.0)/total_imgs,\
                  num_valid_imgs, total_imgs, (num_valid_imgs * 100.0)/total_imgs,\
                  num_ftest_imgs, total_imgs, (num_ftest_imgs * 100.0)/total_imgs))
    print " total | #(train imgs): %d/%d (%0.2f%%) | #(valid imgs): %d/%d (%0.2f%%) | #(ftest imgs): %d/%d (%0.2f%%)" % \
        (num_train_imgs, total_imgs, (num_train_imgs * 100.0)/total_imgs,\
         num_valid_imgs, total_imgs, (num_valid_imgs * 100.0)/total_imgs,\
         num_ftest_imgs, total_imgs, (num_ftest_imgs * 100.0)/total_imgs)

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
                      num_valid_cat_objects[idx], num_valid_objects, (num_valid_cat_objects[idx] * 100.0)/num_valid_objects,\
                      num_ftest_cat_objects[idx], num_ftest_objects, ((num_ftest_cat_objects[idx] * 100.0)/num_ftest_objects if num_ftest_objects > 0 else 0),\
                      (val + num_valid_cat_objects[idx] + num_ftest_cat_objects[idx]), num_train_objects))
        print " cat%d (%s) | #(train objs): %d/%d (%0.2f%%) | #(valid objs): %d/%d (%0.2f%%) | #(ftest objs): %d/%d (%0.2f%%) | total: %d/%d" % \
            (idx, categories[idx], val, num_train_objects, (val * 100.0)/num_train_objects,\
             num_valid_cat_objects[idx], num_valid_objects, (num_valid_cat_objects[idx] * 100.0)/num_valid_objects,\
             num_ftest_cat_objects[idx], num_ftest_objects, ((num_ftest_cat_objects[idx] * 100.0)/num_ftest_objects if num_ftest_objects > 0 else 0),\
             (val + num_valid_cat_objects[idx] + num_ftest_cat_objects[idx]), num_train_objects)

