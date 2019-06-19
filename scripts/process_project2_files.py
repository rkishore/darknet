import sys
import os
import requests
import json
import time

input_file_list = []
project2_mp4_input_folder = "/mnt/bigdrive1/video_analytics_assessment/fouo/inputs/"
project2_ts_input_folder = "/mnt/bigdrive1/video_analytics_assessment/fouo/"
output_folder = "/mnt/bigdrive1/video_analytics_assessment/fouo/outputs/custom3/"
post_dict = {
    "input": "/mnt/bigdrive1/cnn/inputs/co_60sec_clip1.mp4",
    "type": "file",
    "output_dir": "/tmp",
    "output_filepath": output_folder + "co_60sec_clip1.mp4",
    "mode": "video",
    "output_json_filepath": output_folder + "co_60sec_clip1.json",
    "config": {"detection_threshold": 0.5}
}

post_url = "http://10.1.10.110:10001/api/v0/classify"
get_url = "http://10.1.10.110:10001/api/v0/classify/1"

process_mp4_files = False
process_ts_files = True

def get_or_change_state_using_requests_lib(url_name, payload, function_name):
    """ Helper function to POST to 'url_name' with 'payload' """
    response = None
    retval = 0
    
    connection_timeout_in_sec = 2
    try:
    
        if function_name == "POST":
            response = requests.post(url_name, json=payload, timeout=connection_timeout_in_sec)
            
        if function_name == "GET":
            response = requests.get(url_name, timeout=connection_timeout_in_sec)
                
    except requests.exceptions.ConnectionError:
        print("HTTP to %s failed with ConnectionError." % (url_name,))
        retval = -1
    except requests.exceptions.Timeout:
        print("HTTP to %s timed out after %d seconds" % (url_name, connection_timeout_in_sec))
        retval = -1
    except requests.URLRequired:
        print("Invalid URL: " + str(url_name))
        retval = -1
    except requests.TooManyRedirects:
        print("Too many redirects for: " + str(url_name))
        retval = -1
    except requests.exceptions.RequestException:
        print("Ambiguous error for URL: " + str(url_name))
        retval = -1
    else:
        
        if not (response.status_code == requests.codes.created) and \
           not (response.status_code == requests.codes.ok) and \
           not (response.status_code == requests.codes.accepted) and \
           not (response.status_code == requests.codes.no_content):
            
            print("%s to %s failed with code: %d" % (function_name, url_name, response.status_code))
            #print("Response: %s" % (response.text,))
            retval = -1
        
        else:
            print("%s to %s succeeded: %s" % (function_name, url_name, str(response.status_code)))

        return response, retval

def populate_input_mp4_filelist():
    global input_file_list 
    for root, dirs, files in os.walk(project2_mp4_input_folder):
        for filename in files:
            #print(filename)
            if filename.endswith(".mp4"):
                input_file_list.append(filename)
    print "= %d input files to process" % (len(input_file_list))

def populate_input_ts_filelist():
    global input_file_list 
    for root, dirs, files in os.walk(project2_ts_input_folder):
        for filename in files:
            # print "= root=", root, " | filename: ",  filename
            if filename.endswith(".ts"):
                input_file_list.append(root + "/" + filename)
    print "= %d input files to process" % (len(input_file_list))

def main():

    if process_mp4_files:
        populate_input_mp4_filelist()
    elif process_ts_files:
        populate_input_ts_filelist()

    for inputfile in input_file_list:
        if (process_mp4_files):
            post_dict["input"] = project2_input_folder + inputfile
            post_dict["output_filepath"] = output_folder + inputfile
            post_dict["output_json_filepath"] = output_folder + inputfile.rsplit("mp4", 1)[0] + "json"
        else:
            post_dict["input"] = inputfile
            inputfileonly = inputfile.rsplit("/", 1)[1]
            post_dict["output_filepath"] = output_folder + inputfileonly.rsplit("ts", 1)[0] + "mp4"
            post_dict["output_json_filepath"] = output_folder + inputfileonly.rsplit("ts", 1)[0] + "json"
            
        print post_dict

        response, retval = get_or_change_state_using_requests_lib(post_url, post_dict, "POST")
        try:
            post_resp_info = json.loads(response.content)
        except (AttributeError, ValueError) as e:
            print "%s" % (str(e),)
        else:
            print "= POST response: ", post_resp_info
            
        if not (retval < 0):
            time.sleep(5)
            while (True):
                response,retval = get_or_change_state_using_requests_lib(get_url, {}, "GET")
                try:
                    get_resp_info = json.loads(response.content)
                except (AttributeError, ValueError) as e:
                    print "%s" % (str(e),)
                else:
                    print "= GET response: ", get_resp_info
                    if get_resp_info["status"] == "finished":
                        print "= Done processing %s. Exiting." % (post_dict["input"])
                        break
                time.sleep(5)
                
        else:
            print "Proceeding further as POST failed..."
    
if __name__ == "__main__":
    sys.exit(main())
        
