#!flask/bin/python
#
# Dummy REST server that mimics REST API of Classifier application
#
# Copyright igolgi Inc. 2018-2019
#
from flask import Flask, jsonify, make_response, request, abort
import time
from threading import Thread

app = Flask(__name__)

busy_status = False
completed = 0.0

samples = [
]

@app.errorhandler(404)
def not_found(error):
    return make_response(jsonify({'error': 'Not found'}), 404)

@app.errorhandler(503)
def service_unavailable(error):
    return make_response(jsonify({'error': 'Service Unavailable'}), 503)

@app.route('/api/v0/classify/1', methods=['GET'])
def get_tasks():
    global busy_status, completed, samples
    
    if not samples:
        return jsonify({"id": 1, "status": "idle"})
    
    samples[0]["percentage_completed"] = completed
    if completed == 100.0:
        samples[0]["status"] = "finished"
    return jsonify(samples[0])
    
@app.route('/api/v0/classify', methods=['POST'])
def create_task():
    global busy_status, completed, samples
    
    print request
    #print request.json
    if not request.json or not 'input' in request.json:
        if not request.json:
            print "No JSON request"
        if not 'input' in request.json:
            print "No input in request.json"

        abort(400)

    if busy_status == True:
        return service_unavailable(503)
    else:
        completed = 0.0
        busy_status = True
        samples = []
        sample = {
            'id': 1,
            'input': request.json['input'],
            'mode': request.json['mode'],
            'type': request.json['type'],
            'output_dir': request.json['output_dir'],
            'output_filepath': request.json['output_filepath'],
            'output_json_filepath': request.json['output_json_filepath'],
            'config': request.json['config'],
            'percentage_completed': completed,
            'status': 'running',
            'time_started': "2019/02/15 20:19:12",
            'error_msg': None
        }
        
    samples.append(sample)
    return jsonify(samples[0]), 201

def myfunc(i):

    global busy_status, completed, samples

    while(True):
        if busy_status == True:
            while (completed < 100.0):
                samples[0]["percentage_completed"] = completed
                completed += 10.0
                time.sleep(1)
                print "Percentage completed: ", completed
                
            print "Done processing. Setting busy_status to False. Ready for next HTTP POST request"
            busy_status = False
            samples[0]["percentage_completed"] = completed
        else:
            time.sleep(1)
    
if __name__ == '__main__':
    i = 0
    t = Thread(target=myfunc, args=(i,))
    t.daemon = True
    t.start()
    app.run(debug=True, host='0.0.0.0', port=10001)
