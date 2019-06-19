import os
import shutil

num_persons_found = 0
num_cars_found = 0
num_motorbikes_found = 0
num_buses_found = 0
num_trucks_found = 0
num_vehicles_found = 0
num_service_vehicles_found = 0

for value in ["train", "val"]:

    num_labels_processed = 0

    with open("./coco/coco-%s-labels.txt" % (value,), "r") as top_ifp:

        for top_line in top_ifp:

            stripped_top_line = top_line.strip()
            split_top_line = stripped_top_line.rsplit("/", 1)

            # New label file output path
            new_label_path = "/mnt/bigdrive1/cnn/rkishore/darknet/scripts/coco-custom-project1/labels/%s2014/" % (value,) + split_top_line[1]
            print "= New label path: %s" % (new_label_path)

            ofp = open(new_label_path, "w")

            new_image_path = "/mnt/bigdrive1/cnn/rkishore/darknet/scripts/coco-custom-project1/images/%s2014/" % (value,) + split_top_line[1]
            new_image_path = new_image_path.replace(".txt", ".jpg")
            old_image_path = new_image_path.replace("coco-custom-project1", "coco")
            print "= New image path: %s | old image path: %s" % (new_image_path, old_image_path)

            shutil.copyfile(old_image_path, new_image_path)
            with open(stripped_top_line, "r") as ifp:
                for line in ifp:

                    # Create new label file but with only the labels that start with 0, i.e. person
                    # Copy over corresponding image to new image path
                    stripped_line = line.strip()
                    # 0 - person -> 0 (person)
                    # 2 - car -> 1 (car)
                    # 5 - bus -> 2 (service-vehicle)
                    # 7 - truck -> 2 (service-vehicle)

                    if stripped_line.startswith("0 ") or stripped_line.startswith("2 ") or \
                       stripped_line.startswith("5 ") or stripped_line.startswith("7 "):

                        if stripped_line.startswith("0 "):
                            writeout_line = line
                            num_persons_found += 1
                        elif stripped_line.startswith("2 "):
                            writeout_line = line.replace("2 ", "1 ", 1)
                            num_cars_found += 1
                            num_vehicles_found += 1
                        elif stripped_line.startswith("5 "):
                            writeout_line = line.replace("5 ", "2 ", 1)
                            num_buses_found += 1
                            num_vehicles_found += 1
                            num_service_vehicles_found += 1
                        elif stripped_line.startswith("7 "):
                            writeout_line = line.replace("7 ", "2 ", 1)
                            num_trucks_found += 1
                            num_vehicles_found += 1
                            num_service_vehicles_found += 1

                        ofp.write(writeout_line)

                        #if not stripped_line.startswith("0 "):
                        #    print "= Check this file %s" % (new_label_path,)

            ofp.close()

            num_labels_processed += 1

            '''
            if num_labels_processed >= 5000:
                break
            '''
            
    print "= STATS | %s | #persons: %d | #cars: %d | #motorbikes: %d | #buses: %d | #trucks: %d | #vehicles: %d (expected %d) | | #svc-vehicles %d (expected %d)" % \
        (value, num_persons_found, num_cars_found, num_motorbikes_found, num_buses_found, num_trucks_found, num_vehicles_found,\
         num_cars_found + num_motorbikes_found + num_buses_found + num_trucks_found,\
         num_service_vehicles_found, num_buses_found + num_trucks_found,)
            
        
        
