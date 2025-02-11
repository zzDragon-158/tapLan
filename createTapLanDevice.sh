ip tuntap add dev tapLan mode tap
ip link set dev tapLan up
# ip addr add 192.168.100.101/24 dev tapLan   # 地址可以改
# ip link set dev tapLan mtu 1426
# systemctl stop firewalld
