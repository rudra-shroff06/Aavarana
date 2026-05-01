#!/usr/bin/bash
sudo ip netns add ns_server
sudo ip netns exec ns_server ip link set lo up