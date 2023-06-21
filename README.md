# BUAA_OS_简介

本仓库包含了本人于2022年春季操作系统课程设计中的全部代码。下面对该课设项目简要介绍一下。

* CPU：32位MIPS CPU R3000
* 运行环境：GXemul仿真环境

## 内核启动-Lab1

不同于使用BootLoader的真实操作系统，本OS运行在GXemul仿真环境，操作系统内核的加载过程被大大简化。Gxemul启动后，PC会跳转到位于`boot/start.S`中的内核入口_start函数，而后通过`jal`汇编命令跳转到`init/main.c`下的main函数中。MIPS CPU中，程序地址空间分为4个区域，如下图：

<img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306202239886.png" alt="image-20230620223954792" style="zoom:50%;" />

内核位于kseg0位置，CPU发出的逻辑地址到物理地址的映射不经过MMU，而是直接将地址高位清零，通过cache对该区域访问。

## 内存管理-Lab2

R3000发出虚拟地址，虚拟地址映射到物理地址的规则是：

* 虚拟地址位于kseg0，则高位清0通过cache访存，该区域存放内核代码和数据结构
* 虚拟地址位于kseg1，则高3位清0不通过cache访存，用于映射外设
* 虚拟地址位于kuseg，则通过TLB获取物理地址后通过cache访存





