#!/usr/bin/bash

#hosts="altair vega rachael sparta"
hosts="altair vega zebra tara aqua rachael"
test_dir="/NAS/user_data/user/hariwood/CTkit/catlib/test/DRM/run/"

for host in $hosts; do
    ssh $host "cd $test_dir; ./DRMtest -f params.txt &" &
    ssh $host "cd $test_dir; ./DRMtest -f params.txt &" &
done

wait

