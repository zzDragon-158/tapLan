ip tuntap add dev tapLanClient mode tap
ip link set dev tapLanClient up
ip addr add 172.16.100.2/24 dev tapLanClient
ip link set dev tapLanClient mtu 1426
systemctl stop firewalld

