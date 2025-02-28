## 简介
利用公网IPv6实现IPv4局域网，基于TAP设备
### 为什么做这个
1、公网IPv6地址人人都有

2、玩局域网游戏，但是家里网络没有公网IPv4地址且租不起服务器

3、study

## 编译
    # 安装好编译三件套gcc,make和cmake(windows使用msys2环境)
    mkdir -p build
    cd build
    cmake ..
    make

## 运行
### 创建TAP设备
windows安装WinTapDrive文件夹下的驱动(OpenVPN的开源驱动)，linux跳过这一步

### 运行程序
    # 必须以管理员或者root用户运行程序
    # 作为服务端，所有流量经该设备转发
        tapLan -s 192.168.246.1 -p 3460
    # 作为客户端，所有流量发给服务端
        tapLan -c <路由器的公网IP地址> -p <与上面的端口号一致>
