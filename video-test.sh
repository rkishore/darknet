#/bin/bash

PORT=$1
OUTPUTSUBDIR=$2
	
echo "= EXECUTING HTTP POST TO http://10.1.10.110:${PORT}/api/v0/classify/"
curl --header "Content-Type: application/json"   --request POST   --data "{\"input\": \"/mnt/bigdrive1/cnn/inputs/co_60sec_clip1.mp4\", \"type\": \"file\", \"output_dir\": \"/tmp\", \"output_filepath\": \"/mnt/bigdrive1/video_analytics_assessment/fouo/outputs/${OUTPUTSUBDIR}/co_60sec_clip1.mp4\", \"mode\": \"video\", \"output_json_filepath\": \"/mnt/bigdrive1/video_analytics_assessment/fouo/outputs/${OUTPUTSUBDIR}/co_60sec_clip1.json\", \"config\": {\"detection_threshold\": 0.5}}" http://10.1.10.110:${PORT}/api/v0/classify

for (( ; ; ))
do
	echo "= EXECUTING HTTP GET TO http://10.1.10.110:${PORT}/api/v0/classify/1/"
	curl http://10.1.10.110:${PORT}/api/v0/classify/1/
	sleep 5
done
curl --header "Content-Type: application/json"   --request POST   --data '{"input": "/mnt/bigdrive1/cnn/inputs/co_60sec_clip1.mp4", "type": "file", "output_dir": "/tmp", "output_filepath": "", "mode": "video", "config": {"detection_threshold": 0.5}}' http://10.1.10.110:10001/api/v0/classify
