***本文档算作者的不断踩坑+努力填坑过程记录吧***
## 1.24
### macos安装bochs
1. 在公司的服务器(ubuntu)安装，结果发现没有ui(囧
2. macos
    1. brew安装，默认版本是2.7，运行时会出现
       ```
       bochs connected to screen "/dev/ttys001"
       ```
       总而言之，奇奇怪怪+不会

   2. brew安装指定版本bochs，参考https://stackoverflow.com/questions/39187812/homebrew-how-to-install-older-versions
   还是失败了，因为下载某些依赖时会报错，怀疑由于我是M1芯片

   3. 编译源码：
       ```
       wget https://downloads.sourceforge.net/project/bochs/bochs/2.6.11/bochs-2.6.11.tar.gz
       tar -xvf bochs-2.6.11.tar.gz
       cd bochs-2.6.11.tar.gz
       ./configure --prefix=/opt/bochs --disable-docbook --enable-a20-pin --enable-alignment-check --enable-all-optimizations --enable-avx --enable-evex --enable-cdrom --enable-clgd54xx --enable-cpu-level=6 --enable-debugger --enable-debugger-gui --enable-disasm --enable-fpu --enable-iodebug --enable-large-ramfile --enable-logging --enable-long-phy-address --enable-pci --enable-plugins --enable-readline --enable-show-ips --enable-usb --enable-vmx=2 --enable-x86-64 --with-nogui --with-sdl2  --with-term
      make && make install
      ```
踩坑途中发现，我是m1咋编译出x86的机器码呢？？？尝试docker镜像中.  
在M1的mac上运行x86架构的容器，查看进程发现使用该镜像时会自动启动qumu模拟器
```
docker run --rm -v $PWD:/home -it --platform linux/amd64 ubuntu  bash
```
## 2.15
### 初始化8253
现象：运行时先触发了0x20号中断（时钟中断），之后会触发0xd号中断  
思路：注释了8253初始化函数后，发现符合预期  
结论：将下面代码中的类型转化 uint8_t 去掉就正常了（玄学，作者表示没想明白qq
```
 /* 先写入 counter_value 的低 8 位 */ 
 outb(counter_port, (uint8_t)counter_value); 
 /* 再写入 counter_value 的高 8 位 */ 
outb(counter_port, (uint8_t)counter_value >> 8);
```
## 2.23
### 完成简单的内存管理系统
作者在摆烂了几天之后痛定思痛，开始报复性学习了  
内存管理系统真的太烧脑了，前前后后看了两三遍，以后一定要着重复习这部分

## 10.12
下学期实习+毕设，就一直搁置了这件事情，暑假在摆烂  
刚开学学了波python，跟着《500 lines or less》完成了图数据库和解释器的两个玩具demo后，打算十一之后捡起os  
花了10天从头到尾复习（啊不预习）了一遍  
把链表相关的文字敲完之后，发现 c 没有 bool（尴尬


## 10.24 
终于写完 + 调完添加用户进程了，在这里修改了很多古早的bug，当时写的时候对 c 的语法都不太熟练，导致了很多的低级错误集中在这里爆发了   
做好测试很重要（哭

## 23.2.16 
又双叒叕重新开始了，复习（预习）次数++

## 23.2.28 
终于在2月的最后一天完成了第13章硬盘驱动程序，由于细节和新名词太多，一直属于看了忘，忘了看的状态  
在这里犯了很多细节错误，比如条件判断没有加括号和运算符优先级导致的逻辑错误等等，并且以后一定要注意位数，不能随心所欲（苦苦）

## 23.3.8 
实现了创建文件功能，由于前缀知识太多，改动了一大堆文件才实现了创建文件这个函数，导致调试非常困难，debug过程中发现了很多零零碎碎但是非常致命的小错误

## 23.3.10 
实现sys_write功能，继续不断踩坑修复上古bug

## 23.4.13 
最近玩的比较多（反思ing）中途插空写了几个实验和作业（主要是为了逃避修bug）。  
在 sys_stat 中不小心将缓冲区搞坏了，所以在执行 ls 的时候会报 GP 异常，很努力地通过在 buildin_ls 函数中的指令下断点判断出错误的点。这次报错很难直接联想到问题所在的地方，调试能力还是很弱鸡的
现在已经有了个简单的shell啦

## 23.4.14 
调试了一天，修复了很多之前的小坑，现在可以执行外部命令啦啦啦

## 23.4.18  
目前已经支持查看文件 cat 了但是遗留了个巨坑：heap会把pcb覆盖了（暂时不想修

## 23.4.20 
完结撒花🎉