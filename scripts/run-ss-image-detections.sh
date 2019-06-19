#!/bin/bash

#for i in "image5000000_24" "image5000000_25" "image5000000_26" "image5000000_27" "image5000000_28" "image2000000_9.jpg" "image5000000_29" "image2000000_10"  "image5000000_30" "image2000000_11" "image5000000_31" "image2000000_12" "image2000000_13" "image5000000_32" "image2000000_14" "image5000000_33" "image2000000_15" "image5000000_34" "image2000000_16" "image1000000_53" "image5000000_35" "image1000000_54"
for (( i=0; i<1; i++ )); do
	#echo "/mnt/bigdrive1/cnn/outputs/test-images/image5000000_35.jpg";
	#curl -v --header "Content-Type: application/json" --request POST --data "{\"input\": \"/mnt/bigdrive1/cnn/outputs/test-images/image5000000_35.jpg\", \"type\": \"file\", \"output_dir\": \"/mnt/bigdrive1/cnn\", \"config\": {\"detection_threshold\": 0.5}, \"mode\": \"image\"}" http://10.1.10.110:10004/api/v0/classify/ ;
	echo "http://10.1.10.172/image2000000_${i}.jpg";
	#curl -v --header "Content-Type: application/json" --request POST --data "{\"input\": \"http://10.1.10.94/image5000000_35.jpg\", \"type\": \"stream\", \"output_dir\": \"/mnt/bigdrive1/cnn\", \"config\": {\"detection_threshold\": 0.5}, \"mode\": \"image\"}" http://10.1.10.110:10004/api/v0/classify/ ;
	curl -v --header "Content-Type: application/json" --request POST --data "{\"input\": \"http://10.1.10.172/image2000000_${i}.jpg\", \"type\": \"stream\", \"output_dir\": \"/mnt/bigdrive1/cnn/\", \"output_filepath\": \"/mnt/bigdrive1/cnn/outputs/image2000000_${i}\", \"config\": {\"detection_threshold\": 0.5}, \"mode\": \"image\"}" http://10.1.10.110:10001/api/v0/classify/ ;
	sleep 2;
done
