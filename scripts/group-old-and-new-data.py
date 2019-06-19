import os
import sys
import subprocess
import argparse
import json
import logging
import glob
import shutil

# /mnt/bigdrive1/tracking/army_fouo/outputs/training_data/

logging.basicConfig(filename="/mnt/bigdrive1/cnn/project1-training-collation.log", format='%(asctime)s %(levelname)s:%(message)s', level=logging.INFO)

parser = argparse.ArgumentParser(description="Rename training data provided to check quality of data marked")
parser.add_argument('--inputfolderpath1', type=str, help='Path to input folder with old files', required=True)
parser.add_argument('--inputfolderpath2', type=str, help='Path to input folder with new files', required=True)
parser.add_argument('--outputfolderpath', type=str, help='Path to where output JPG files should be moved to', required=True)

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

categories_for_img_ignore = ["pick-up-truck",
                             "mini-van",
                             "standard-suv",
                             "motorcyle"]

total_categories = len(categories)

if __name__ == '__main__':

    args = vars(parser.parse_args())
    
    logging.info("= Old path: %s" % (args["inputfolderpath1"],))
    logging.info("= New path: %s" % (args["inputfolderpath2"],))
    logging.info("= Output path: %s" % (args["outputfolderpath"]))
    
    for category in categories_for_img:

        if category in categories_for_img_ignore:
            logging.info(" = Ignoring %s and moving on" % (category,))
            continue
        
        if args["inputfolderpath1"].endswith("/"):
            old_cat_path = "%s%s*.jpg" % (args["inputfolderpath1"], category)
        else:
            old_cat_path = "%s/%s*.jpg" % (args["inputfolderpath1"], category)

        if args["inputfolderpath2"].endswith("/"):
            new_cat_path = "%s%s*.jpg" % (args["inputfolderpath2"], category)
        else:
            new_cat_path = "%s/%s*.jpg" % (args["inputfolderpath2"], category)

        old_cat_jpg_files = glob.glob(old_cat_path)
        num_old_cat_jpg_files = len(old_cat_jpg_files)
        
        logging.info("= Old cat path: %s with numfiles: %d" % (old_cat_path, num_old_cat_jpg_files))
        
        cat_idx = 0
        for catfile in old_cat_jpg_files:
            if args["outputfolderpath"].endswith("/"):
                final_catfile_path = "%s%s-%d.jpg" % (args["outputfolderpath"], category, cat_idx,)
            else:
                final_catfile_path = "%s/%s-%d.jpg" % (args["outputfolderpath"], category, cat_idx,)

            logging.info(" Copying %s to %s" % (catfile, final_catfile_path,))
            cat_idx += 1
            #shutil.copy(catfile, input_png_imgfile_path)

        new_cat_jpg_files = glob.glob(new_cat_path)
        num_new_cat_jpg_files = len(new_cat_jpg_files)

        logging.info("= New cat path: %s with numfiles: %d" % (new_cat_path, num_new_cat_jpg_files))            
        for catfile in new_cat_jpg_files:
            if args["outputfolderpath"].endswith("/"):
                final_catfile_path = "%s%s-%d.jpg" % (args["outputfolderpath"], category, cat_idx,)
            else:
                final_catfile_path = "%s/%s-%d.jpg" % (args["outputfolderpath"], category, cat_idx,)

            logging.info(" Copying %s to %s" % (catfile, final_catfile_path,))
            cat_idx += 1

        logging.info("= CATEGORY: %s | numfiles: %d" % (category, cat_idx,))

