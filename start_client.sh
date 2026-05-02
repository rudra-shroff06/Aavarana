#!/usr/bin/bash
sudo ip netns exec ns_client$1 ./aavarana_client 192.168.$1.1 $2 $3