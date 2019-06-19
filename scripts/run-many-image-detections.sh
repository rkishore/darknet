#!/bin/bash

for (( i=20; i<21; i++ )); do
	echo "/mnt/bigdrive1/cnn/outputs/test-images/image1000000_${i}.jpg";
	curl -v --header "Content-Type: application/json" --request POST --data "{\"input\": \"/mnt/bigdrive1/cnn/outputs/test-images/image1000000_${i}.jpg\", \"type\": \"file\", \"output_dir\": \"/mnt/bigdrive1/cnn\", \"config\": {\"detection_threshold\": 0.5}, \"mode\": \"image\"}" http://10.1.10.110:10004/api/v0/classify/ ;
done
