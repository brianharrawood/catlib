#!/usr/bin/bash

##test_dir="/NAS/user_data/user/hariwood/CTkit/catlib/test/DRM/run/"
test_dir="/home/brian/src/CTkit/catlib/test/DRM/run"

cd $test_dir; ./DRMtest -f params.txt &
cd $test_dir; ./DRMtest -f params.txt &

echo "This may take some time, depending on parameters chosen." 
