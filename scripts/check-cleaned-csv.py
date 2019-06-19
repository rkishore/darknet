import os
import sys
import subprocess
import argparse
import shutil
import json
import string
import logging
from PIL import Image


categories_for_img = ['cargo-truck',
                      'dump-truck',
                      'bongo-truck',
                      'water-tanker-truck',
                      'septic-tanker-truck',
                      'standard-bus',
                      'standard-compactor',
                      'stakebed-truck',
                      'flatbed-truck',
                      'lowboy-semi-truck']

num_cat_imgs = {}

if __name__ == "__main__":

    cleaned_csv_path = "/mnt/bigdrive1/tracking/army_fouo/outputs/combined_training_data/cleaned.csv"
    base_input_csv_path = cleaned_csv_path.rsplit("/", 1)[0]

    for cat in categories_for_img:
        num_cat_imgs[cat] = 0
    
    with open(cleaned_csv_path, "r") as ifp:
        for line in ifp:
            if line.startswith("name"):
                print line.strip()
                continue

            split_line = line.split(",")
            full_img_path = base_input_csv_path + "/" + split_line[0].strip()
            image_obj = Image.open(full_img_path)
            img_size_info = image_obj.size
            #if img_size_info[0] < 100 and img_size_info[1] < 100:
            #    continue
            #print "%s,%s,%dx%d" % (full_img_path,split_line[1].strip(), img_size_info[0], img_size_info[1],)
            cur_category = split_line[1].strip()
            #if cur_category.startswith("water-tanker") or cur_category.startswith("septic-tanker"):
            if cur_category.startswith("tanker"):
                if "water-tanker" in split_line[0].strip():
                    num_cat_imgs["water-tanker-truck"] += 1
                else:
                    num_cat_imgs["septic-tanker-truck"] += 1
            #    print "%s,%s" % (split_line[0].strip(),"tanker-truck",)
            else:
                num_cat_imgs[cur_category] += 1
            #    print "%s,%s" % (split_line[0].strip(),cur_category,)


for cat in categories_for_img:
    print cat, num_cat_imgs[cat]
