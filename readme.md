# 简介
利用公网IPv6组建你的虚拟IPv4局域网，基于TAP设备，支持数据加密
## 为什么做这个
1、公网IPv6地址目前普及广泛，手机移动数据热点自带IPv6地址

2、玩局域网游戏，大部分局域网游戏只支持IPv4地址连接，利用这个软件将IPv4映射到IPv6来实现异地局域网游戏联机

# 编译
    # 安装编译三件套gcc,make和cmake(windows使用msys2环境)，还有openssl库
    cmake .
    make

# 运行
## 创建TAP设备
windows安装WinTapDrive文件夹下的驱动(OpenVPN的开源驱动)，linux跳过这一步

## 运行程序
    # 必须以管理员或者root用户运行程序
### windows
    # 修改runTapLan.bat文件内容，如何修改在文件内有说明
    # 将编译的可执行程序tapLan.exe与runTapLan.bat移动到放置驱动文件的文件夹WinTapDrive下，在WinTapDrive文件夹下以管理员身份运行runTapLan.bat文件即可

### linux
    # 作为服务端，所有流量经该设备转发
        tapLan -s 192.168.246.0/24 -p 3460
    # 作为客户端，所有流量发给服务端
        tapLan -c <路由器的公网IP地址> -p <与上面的端口号一致>

### 加密传输
    # 如果需要加密传输，软件支持AES-128对称加密
    # 服务端和客户端在启动程序时加上参数[-k <password>]
    # password的内容服务端与客户端需要一致，否则服务端与客户端无法建立连接
    # 使用加密传输会损失一些传输性能，使用iperf3测试带宽大概减少了10%左右