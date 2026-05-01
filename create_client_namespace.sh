#!/usr/bin/bash

sudo ip netns add ns_client$1

sudo ip link add veth_server$1 type veth peer name veth_cli$1

sudo ip link set veth_server$1 netns ns_server
sudo ip link set veth_cli$1 netns ns_client$1

sudo ip netns exec ns_server ip addr add 192.168.$1.1/24 dev veth_server$1
sudo ip netns exec ns_server ip link set veth_server$1 up

sudo ip netns exec ns_client$1 ip addr add 192.168.$1.2/24 dev veth_cli$1
sudo ip netns exec ns_client$1 ip link set veth_cli$1 up
sudo ip netns exec ns_client$1 ip link set lo up

sudo ip netns exec ns_client$1 ip route add 192.168.$1.1 dev veth_cli$1