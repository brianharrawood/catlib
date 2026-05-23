#!/usr/bin/bash

# this handles debug output when compiled with DEBUG on

host=$(hostname -s)
for i in {1..20}; do
  ./DRMtest -f params.txt >& "run_log/log$(printf '%02d' "$i")_$host" &
done


echo "This may take some time, depending on parameters chosen." 
