#!/bin/bash

for (( i=50; i<51; i++ )); do
	echo "http://10.1.10.110:8080/openoutputs/test-images/image1000000_${i}.jpg";
	curl -v --header "Content-Type: application/json" --request POST --data "{\"input\": \"http://10.1.10.110:8080/openoutputs/test-images/image1000000_${i}.jpg\", \"type\": \"stream\", \"output_dir\": \"/mnt/bigdrive1/cnn\", \"config\": {\"detection_threshold\": 0.5}, \"mode\": \"image\"}" http://10.1.10.110:10004/api/v0/classify/ ;
	curl -v http://10.1.10.110:10004/api/v0/classify/1/ ; 
done

