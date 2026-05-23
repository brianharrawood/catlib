#!/usr/bin/bash

#run at upcoming time

if [ "$#" -ne 1 ]; then
    time="now"
else
    time=$1
fi

echo "./run_host.sh" | at "$time"

