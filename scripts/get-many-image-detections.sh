#!/bin/bash

for (( i=0; i<15; i++ )); do
	curl -v http://10.1.10.110:10002/api/v0/classify/${i}/ ; 	
	sleep 1 ;
done
