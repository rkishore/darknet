from urllib2 import Request, urlopen, HTTPError, URLError
import json
import argparse
import time
import os
import glob
import threading
import logging

logging.basicConfig(level=logging.DEBUG,
                    format='[%(levelname)s] (%(threadName)-10s) %(message)s',)

parser = argparse.ArgumentParser(description="Test classifyapp API")
parser.add_argument('--addr', help="Address of classifyapp service (e.g. localhost:55555)", default="localhost:10001", type=str, required=True)
parser.add_argument('--image_name', help="Key of image in script (e.g. 'vehicle1')", default="vehicle1", type=str)
parser.add_argument('--testpost', help="Set this to 1 to test HTTP POST (default=0)", default=0, type=int)
parser.add_argument('--testgets', help="Use this to test HTTP GET (single). Provide job_id as argument. (default=-1)", default=-1, type=int)
parser.add_argument('--testdelete', help="Use this to test HTTP DELETE. Provide job_id as argument. (default=-1)", default=-1, type=int)

#parser.add_argument('--testall', help="Set this to 1 to test all API methods (default=0)", default=0, type=int)
#parser.add_argument('--testgeta', help="Set this to 1 to test HTTP GET (all) (default=0)", default=0, type=int)
#parser.add_argument('--testdir', help="Use this to test transcribing for multiple files. Provide directory where files reside as argument. (default=NULL)", default=None, type=str)
#parser.add_argument('--testerror', help="Set this to 1 to test response to erroneous JSON inputs (default=0)", default=0, type=int)

values_json_dict = { 
    "vehicle1": {
        "input": "http://li1249-5.members.linode.com:8080/output/Car1.png",
        "type": "stream",
        "output_dir": "/tmp",
        "output_fileprefix": "TEST1",
        "config": {
            "acoustic_model": "/usr/local/share/pocketsphinx/acoustic/en-us",
            "language_model": "/usr/local/share/pocketsphinx/language/en-us.lm.dmp",
            "dictionary": "/usr/local/share/pocketsphinx/dictionary/cmu07a.dic",
            "writeout_duration": 120.0,
        },
    },
}

#values = json.dumps(values_json)

headers = {
  'Content-Type': 'application/json'
}

service_addr_list = []
MAX_POST_RETRIES = 3
'''
def test_erroneous_input(service_addr):

    # Bad input file location
    err_input_json = {
        "input": "/spurious/IQMediaPAL.mp4",
        "output": "/tmp/IQMediaPAL-output.mp4",
        #"config": {
        #    "output_format": "360x240",
        #    "bitrate_kbps": 300,
        #    "quality": 1
        #}
    }

    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)

    # Bad output file location
    err_input_json["input"] = "/mnt/bigdrive2/IQMediaPAL_B_20141218_1440.mp4"
    err_input_json["output"] = "/kishore/IQMediaPAL-output.mp4"
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)
    
    # Absent config
    err_input_json["output"] = "/tmp/IQMediaPAL-output.mp4"
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)
    
    # Absent output_format, bit-rate or quality
    err_input_json["config"] = {
        "dummy": "360x240",
    }
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)

    # Absent bit-rate or quality
    err_input_json["config"] = {
        "output_format": "360x240",
    }
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)

    # Absent bit-rate or quality
    err_input_json["config"] = {
        "output_format": "360x240",
        "bitrate_kbps": 300,
    }
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)
    
    # Bad output_format
    err_input_json["config"] = {
        "output_format": "ASADSAD",
        "bitrate_kbps": 300,
        "quality": 1
    }
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)

    # Bad bitrate
    err_input_json["config"]["output_format"] = "360x240"
    err_input_json["config"]["bitrate_kbps"] = "asdsa"
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)

    # Bad quality value
    err_input_json["config"]["bitrate_kbps"] = 300
    err_input_json["config"]["quality"] = -1
    local_values = json.dumps(err_input_json)
    post_retval = test_post(service_addr, local_values)

'''

def test_post(service_addr, image_name):
    ''''if not data_tosend:
        request = Request('http://%s/api/v0/classify' % (service_addr,), data=values, headers=headers)
    else:
        request = Request('http://%s/api/v0/classify' % (service_addr,), data=data_tosend, headers=headers)
        '''
    
    values = json.dumps(values_json_dict[image_name])
    #print values
    request = Request('http://%s/api/v0/classify' % (service_addr,), data=values, headers=headers)
    retval = 0
    try:
        response_body = urlopen(request).read()
    except HTTPError as e:
        print '= The server couldn\'t fulfill the POST request.'
        print '= Error code: ', e.code
	print '= Error reason: ', e.reason
        retval = -1
    except URLError as e:
        print '= Failed to reach a server. POST request.'
        print '= Reason: ', e.reason
        retval = -1
    else:
        print "= START HTTP POST response = "
        print response_body
        print "= END HTTP POST response = "
        retval = 0
        
    return retval

def test_get_all(service_addr, debug=False):
    response_body = None
    request = Request('http://%s/api/v0/classify' % (service_addr,))
    try:
        response_body = urlopen(request).read()
    except HTTPError as e:
        print '= The server couldn\'t fulfill the GET (all) request.'
        print '= Error code: ', e.code, e.reason
    except URLError as e:
        print '= Failed to reach a server. GET (all) request.'
        print '= Reason: ', e.reason
    else:
        if (debug):
            print "= START HTTP GET (all) response = "
            print response_body
            print "= END HTTP GET (all) response = "
        else:
            pass

    return response_body;

def test_get_single(service_addr, jobid_list, debug=False):
    response_body = None
    for job_id in jobid_list:
        request = Request('http://%s/api/v0/classify/%d/' % (service_addr, int(job_id)))
        try:
            response_body = urlopen(request).read()
        except HTTPError as e:
            print '= The server couldn\'t fulfill the GET request for ID: %d' % (int(job_id))
            print '= Error code: ', e.code, e.reason
        except URLError as e:
            print '= Failed to reach a server. GET request for ID: %d' % (int(job_id))
            print '= Reason: ', e.reason
        else:
            if (debug):
                print "= START HTTP GET (single) response for classify_id: %s = " % (str(job_id),)
                print response_body
                print "= END HTTP GET (single) response for classify_id: %s = " % (str(job_id),)
            else:
                pass

    return response_body

def test_delete_single(service_addr, jobid_list):
    for job_id in jobid_list:
        cur_url = 'http://%s/api/v0/classify/%d/' % (service_addr, int(job_id))
        #print "CALLING HTTP DELETE with: ", cur_url
        request = Request(cur_url)
        request.get_method = lambda: 'DELETE'
        try:
            response_body = urlopen(request).read()
        except HTTPError as e:
            print '= The server couldn\'t fulfill the DELETE request for ID: %d' % (int(job_id))
            print '= Error code: ', e.code, e.reason
        except URLError as e:
            print '= Failed to reach a server. DELETE request for ID: %d' % (int(job_id))
            print '= Reason: ', e.reason
        else:
            print "= START HTTP DELETE (single) response for classify_id: %s = " % (str(job_id),)
            print "= Successful (empty response)", response_body
            print "= END HTTP DELETE (single) response for classify_id: %s = " % (str(job_id),)

    return


def process_split_filelist(input_mp4file_list, service_endpoint_id):

    local_values_json = {
        "input": "/mnt/bigdrive2/IQMediaPAL_B_20141218_1440.mp4",
        "output": "/tmp/IQMediaPAL_B_20141218_1440-output.mp4",
        "config": {
            "output_format": "360x240",
            "bitrate_kbps": 300,
            "quality": 1
        }
    }
    
    local_values = json.dumps(local_values_json)

    cur_service_endpoint = service_addr_list[service_endpoint_id]

    logging.debug(str(cur_service_endpoint) + " | idx = " + str(service_endpoint_id))

    #logging.debug(str(input_mp4file_list))

    files_processed = 0
    for inputfile in input_mp4file_list:

        local_values_json["input"] = inputfile
        local_values_json["output"] = inputfile.replace("input", "output").split(".mp4")[0] + "-output.mp4"
        local_values = json.dumps(local_values_json)
        logging.debug(str(cur_service_endpoint) + " | " + str(local_values_json))

        # Test HTTP POST 
        post_retval = test_post(cur_service_endpoint, local_values)
        if (post_retval < 0):
            for i in range(0, MAX_POST_RETRIES):
                time.sleep(5)
                post_retval = test_post(cur_service_endpoint, local_values)
                if post_retval == 0:
                    break
        
        # Test HTTP GET (all)
        job_ids = []
        response_body = test_get_all(cur_service_endpoint)
        if (response_body):
            decoded_json = json.loads(response_body)
            for job_info in decoded_json:
                if "id" in job_info and "status" in job_info and job_info["status"] == "running":
                    job_ids.append(job_info["id"])
                    break
                    
            if (len(job_ids)):
                still_running = True
                while(still_running):
                    response_body2 = test_get_single(cur_service_endpoint, job_ids)
                    decoded_json2 = json.loads(response_body2)
                    if "status" in decoded_json2 and decoded_json2["status"] != "running":
                        still_running = False
                    else:
                        logging.debug(str(decoded_json2["id"]) + " | " + str(decoded_json2["input"]) + " | " + str(decoded_json2["percentage_finished"]) +\
                                      " | " + str(cur_service_endpoint) + " | " + str(files_processed))
                        
                    if (still_running):
                        time.sleep(5)
                        
                files_processed += 1

        else:
            logging.debug("No response!")
    
    

'''
    elif (cmdline_args.testall):

        # Test HTTP POST 
        test_post(service_addr_list[0], cmdline_args.station)

        # Test HTTP GET (all)
        response_body = test_get_all(service_addr_list[0])

        # Test HTTP GET (single) and DELETE
        if (response_body):
            job_ids = []
            decoded_json = json.loads(response_body)
            for job_info in decoded_json:
                if "id" in job_info:
                    job_ids.append(job_info["id"])

            print "= Sleeping for some time (5s) before deleting the running job..."
            time.sleep(5) # let the job start and run for a bit
            test_get_single(service_addr_list[0], job_ids)
            test_delete_single(service_addr_list[0], job_ids)
            # Get job state after delete
            test_get_single(service_addr_list[0], job_ids)            

        else:
            print "= Could not execute HTTP GET (single) and HTTP DELETE (single) as HTTP GET (all) failed"            
'''

''' main() '''
if __name__ == "__main__":
    
    cmdline_args = parser.parse_args()

    print cmdline_args
    
    service_addr_list = cmdline_args.addr.split(",")

    if (cmdline_args.testpost):

        # Test HTTP POST 
        test_post(service_addr_list[0], cmdline_args.image_name)

    elif (cmdline_args.testgets >= 0):

        # Test HTTP GET (single)
        job_ids = []
        job_ids.append(cmdline_args.testgets)
        response_body = test_get_single(service_addr_list[0], job_ids, debug=True)
    
    elif (cmdline_args.testdelete >= 0):

        # Test HTTP GET (single)
        job_ids = []
        job_ids.append(cmdline_args.testdelete)
        response_body = test_delete_single(service_addr_list[0], job_ids)

        '''
        # Create a job with HTTP POST 
        # test_post(service_addr_list[0])

        # Get last two jobs with HTTP GET (all)
        response_body = test_get_all(service_addr_list[0])

        # Test HTTP DELETE (single)
        if (response_body):
            job_ids = []
            decoded_json = json.loads(response_body)
            for job_info in decoded_json:
                if "id" in job_info and "status" in job_info:
                    if job_info["status"] == "running":
                        job_ids.append(job_info["id"])

            #print "= Sleeping for some time (5s) before deleting the running job..."
            #time.sleep(5) # let the job start and run for a bit
            test_delete_single(service_addr_list[0], job_ids)        
            # Get deleted job state 
            test_get_single(service_addr_list[0], job_ids)            
            
        else:
            print "= Could not execute HTTP DELETE (single) as HTTP GET (all) failed"            
            '''            
    else:
        
        print "\n= Select at least one choice from --testgets, --testpost, --testdelete"
        print "= For --testgets and --testdelete, provide an integer argument >= 0"
        print "= For --testpost, provide a string argument for the station"
