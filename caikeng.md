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
