ip tuntap add dev tapLanServer mode tap
ip link set dev tapLanServer up
ip addr add 172.16.100.1/24 dev tapLanServer
ip link set dev tapLanServer mtu 1426
systemctl stop firewalld

