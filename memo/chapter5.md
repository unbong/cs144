# 应用程序和NAT
## NATs-简介
* NAT是介于两个端点之间工作，用于转换目的地IP地址和端口地址的功能
* 能够隐藏发送端的ip信息。
* 过程 
    * A向B发送TCP的段，经过NAT
    * 在NAT中会把TCP段中A的IP地址和端口信息，转换成NAT的IP地址和NAT的端口号。
    * 在NAT中生成一个A的IP，端口号和NAT的IP 端口号的映射信息，并保持一段时间。
    * B返回TCP的段信息，经过NAT时，会同过映射信息，将TCP中的ip和端口进行转换

## NATs 类型
## NATs 含义
## NATs 操作

## 第五章 HTTP DNS DHCP 略