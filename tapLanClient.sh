ip tuntap add dev tapLan mode tap
ip link set dev tapLan up
ip addr add 172.16.100.2/24 dev tapLan
ip link set dev tapLan mtu 1426
systemctl stop firewalld

