import requests
import json
import time

filenames = ["image5000000_24",
             "image5000000_25",
             "image5000000_26",
             "image5000000_27",
             "image5000000_28",
             "image2000000_9",
             "image5000000_29",
             "image2000000_10",
             "image5000000_30",
             "image2000000_11",
             "image5000000_31",
             "image2000000_12",
             "image2000000_13",
             "image5000000_32",
             "image2000000_14",
             "image5000000_33",
             "image2000000_15",
             "image5000000_34",
             "image2000000_16",
             "image1000000_53",
             "image5000000_35",
             "image1000000_54"]

url = "http://10.1.10.110:10002/api/v0/classify/"
for filename in filenames[0:1]:

    input_filename = "http://10.1.10.94/%s.jpg" % (filename,)
    print("Processing %s" % (input_filename,))
    json_to_send = json.dumps({"input": input_filename,
                               "type": "stream",
                               "output_dir": "/mnt/bigdrive1/cnn/",
                               "config": {"detection_threshold": 0.5}})

    post_resp = requests.post(url, data = json_to_send, timeout=1.0)
    post_resp_json = post_resp.json()
    post_resp_id = post_resp_json["id"]
    print("ID: %s" % (str(post_resp_id),))
    time.sleep(0.05)
    get_url = url + str(post_resp_id) + "/"
    get_resp = requests.get(get_url, timeout=1.0)
    print("GET response: %s" % (str(get_resp.json()),))
             
    
    
