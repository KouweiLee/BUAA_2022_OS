# Lab5

## IDE磁盘驱动

### 内存映射I/O系统调用

相关系统调用函数有：

1. 写入外设函数，注意，dev是要操作外设的物理地址，va为内存写入缓冲区，len为写入的长度

   ```c
   //lib/syscall_all.c
   int sys_write_dev(int sysno, u_int va, u_int dev, u_int len)
   {
       if(0x10000000 <= dev && dev + len <= 0x10000020 ||//console
          0x13000000 <= dev && dev + len <= 0x13004200 ||//IDE
          0x15000000 <= dev && dev + len <= 0x15000200){ //rtc
           bcopy(va, dev + 0xA0000000, len);//将va里的内容写入dev
           return 0;
       }
       return -E_INVAL;
   }
   ```

2. 从外设读函数，注意，dev是读外设缓冲区的物理地址，va是内存缓冲区，len为长度

   ```c
   int sys_read_dev(int sysno, u_int va, u_int dev, u_int len)
   {
       if(0x10000000 <= dev && dev + len <= 0x10000020 ||
          0x13000000 <= dev && dev + len <= 0x13004200 ||
          0x15000000 <= dev && dev + len <= 0x15000200){
           bcopy(dev + 0xA0000000, va, len);
           return 0;
       }
       return -E_INVAL;
   }
   ```

### IDE磁盘

磁盘相关基本概念

1. 扇区(sector)：磁盘盘片被划分成很多扇形的区域，叫做扇区。扇区是磁盘执行读写操作的单位，一般是512 字节。扇区的大小是一个磁盘的硬件属性。
2. 磁道(track): 盘片上以盘片中心为圆心，不同半径的同心圆。
3. 柱面(cylinder)：硬盘中，不同盘片相同半径的磁道所组成的圆柱。
4. 磁头(head)：每个磁盘有两个面，每个面都有一个磁头。当对磁盘进行读写操作时，磁头在盘片上快速移动。

Simulated IDE disk 的地址是 0x13000000，I/O 寄存器相对于 0x13000000 的偏移和对应的功能为：

| Offset        | Effect                                         |
| ------------- | ---------------------------------------------- |
| 0x0000        | 写：设置读/写操作相对于磁盘的偏移量            |
| 0x0008        | 写：设置偏移量的高32位                         |
| 0x0010        | 写：设置IDE的ID                                |
| 0x0020        | 写：开始一个读（0）或写（1）操作               |
| 0x0030        | 读：获取操作的返回值（0表示失败，非0表示成功） |
| 0x4000-0x41ff | 读/写：512字节的缓冲区                         |

内核态驱动：

```assembly
#内核态读扇区
LEAF(read_sector)
	sw a0, 0xB3000010 #设置IDE的id
	sw a1, 0xB3000000 #设置读写操作相对于磁盘的偏移量
	li t0, 0
	sb t0, 0xB3000020 #开始读
	lw v0, 0xB3000030 #获取返回值
	nop
	jr ra
	nop
END(read_sector)
```

#### 用户态驱动

1. 读IDE，`diskno`是磁盘ID，`secno`为扇区号，`dst`为读到内存的缓冲区地址，`nsecs`为要读的扇区总数

   > 步骤为：
   >
   > 1.设置IDE的id，0x10
   >
   > 2.设置disk偏移量,0x00
   >
   > 3.开启读模式，0代表读，1代表写, 0x20
   >
   > 4.判断是否成功,0x30
   >
   > 5.读取数据到内存缓冲区,0x4000

   ```c
   //fs/fs.c
   void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs)
   {
       // 0x200: the size of a sector: 512 bytes.
       int offset_begin = secno * 0x200;//设定IDE的偏移量
       int offset_end = offset_begin + nsecs * 0x200;
       int offset = 0;//现在读到IDE的偏移量
       u_int now; 	   		
       u_int zero = 0;
       while (offset_begin + offset < offset_end) {
           now = offset_begin + offset;
           if(syscall_write_dev(&diskno, 0x13000010, 4) < 0)//设定IDE磁盘编号
               user_panic("ide_read_panic!");
           if(syscall_write_dev(&now, 0x13000000, 4) < 0)//设定本次读的disk的偏移量
               user_panic("ide_read_panic!");
           if(syscall_write_dev(&zero, 0x13000020, 4) < 0)//设定模式为读
               user_panic("ide_read_panic!");
           if(syscall_read_dev(&r, 0x13000030, 4) < 0)//判断是否读成功，返回0为失败
               user_panic("ide_read_panic!");
           if(r == 0) 
               user_panic("ide_read_panic!");
           if(syscall_read_dev((u_int)(dst+offset),0x13004000, 0x200) < 0)//从缓冲区读
               user_panic("ide_read_panic!");
           offset+=0x200;
       }
   }
   ```

2. 向IDE写数据。`diskno`为磁盘编号，`secno`为扇区号，`src`为内存缓冲区地址，`nsecs`为写入的扇区数

   > 步骤为：
   >
   > 1.设置IDE的id，0x10
   >
   > 2.设置disk偏移量,0x00
   >
   > 3.向IDE缓冲区中写入数据，0x4000
   >
   > 4.开启写模式，0代表读，1代表写, 0x20
   >
   > 5.判断是否成功,0x30
   >
   > 6.读取数据到内存缓冲区,0x4000

   ```c
   void
   ide_write(u_int diskno, u_int secno, void *src, u_int nsecs)
   {
       // Your code here
       int offset_begin = secno * 0x200;
       int offset_end = offset_begin + nsecs * 0x200;
       int offset = 0;
       int r;
       u_int one = 1;
       u_int now;
       // DO NOT DELETE WRITEF !!!
       writef("diskno: %d\n", diskno);
   
       while (offset_begin + offset < offset_end) {
           now = offset_begin + offset;
           if(syscall_write_dev(&diskno, 0x13000010, 4) < 0)
               user_panic("ide_write_panic!");
           if(syscall_write_dev(&now, 0x13000000, 4) < 0)
               user_panic("ide_write_panic!");
           if(syscall_write_dev((u_int)(src+offset),0x13004000, 0x200) < 0)
               user_panic("ide_write_panic!");
           if(syscall_write_dev(&one, 0x13000020, 4) < 0)
               user_panic("ide_write_panic!");
           if(syscall_read_dev(&r, 0x13000030, 4) < 0)
               user_panic("ide_write_panic!");
           offset+=0x200;
           // if error occur, then panic.
       }
   }                  
   ```

## 磁盘

* 磁盘空间的基本布局

<img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221641887.png" alt="lab5-pic-2" style="zoom: 25%;" />



### 磁盘块

磁盘块 是一个 虚拟概念，是**操作系统可见的磁盘中的最小单位**，是多个相临扇区的组合。

MOS把第0个磁盘块 (4KB) 当作启动扇区和分区表使用，第1个磁盘块作为超级块 (Super Block)，用来描述文件系 统的基本信息，如 Magic Number、磁盘块个数以及根目录的位置。

共有**1K**个磁盘块，一个磁盘块是**4KB**，和一页大小一样，一个磁盘共4MB

使用**位图法**对磁盘块的使用情况进行标记。

* 磁盘块数据结构

```c
//fs/fsformat.c
struct Block {
    uint8_t data[BY2BLK];//BY2BLK is 4096，data[i]为一个字节
    uint32_t type;	     //块类型
} disk[NBLOCK];//NBLOCK is 1024
/* type有 
 * BLOCK_BOOT：启动块
 * BLOCK_SUPER：超级块
 * BLOCK_BMAP：位图块
*/
//include/fs.h
struct Super {
    u_int s_magic;      // Magic number: FS_MAGIC
    u_int s_nblocks;    // Total number of blocks on disk
    struct File s_root; // Root directory node
};  
```

### 创建磁盘镜像

`fsformat.c`文件创建磁盘镜像文件 gxemul/fs.img，这个磁盘镜像可以模拟与真实的磁盘文件设备之间的交互。

<img src="https://hyggge.github.io/2022/05/30/OS-Lab5函数解读/fsformat.drawio.svg" alt="img" style="zoom: 67%;" />

#### fsformat.c

##### 主函数

明确一点，执行这个是用的linux的编译器，创建磁盘镜像的。

在Lab5，执行的指令是`./fsformat ./fs.img motd newmotd`，该指令创建了fs.img磁盘镜像，并将`fs/motd`和`fs/newmotd`文件存入磁盘的根目录下。

```c
int main(int argc, char **argv) {
    int i;
    
    init_disk();//初始化磁盘
    
    if(argc < 3 || (strcmp(argv[2], "-r") == 0 && argc != 4)) {
        fprintf(stderr, "\
Usage: fsformat gxemul/fs.img files...\n\
       fsformat gxemul/fs.img -r DIR\n");
        exit(0);
    }

    if(strcmp(argv[2], "-r") == 0) {//若有-r参数，则写入directory
        for (i = 3; i < argc; ++i) {
            write_directory(&super.s_root, argv[i]);
        }
    }
    else {//若无，正如lab5
        for(i = 2; i < argc; ++i) {
            write_file(&super.s_root, argv[i]);//将内存中的文件写入磁盘根目录下
        }
    }

    flush_bitmap();//更新磁盘bitmap，根据nextbno确定已经占用的磁盘块。注意在磁盘创建的过程中磁盘块是依次按顺序占用的
    finish_fs(argv[1]);//创建磁盘镜像
    //Finish all work, dump block array into physical file.
    return 0;
}              
```

##### 1.init_disk

初始化磁盘：初始磁盘位图和超级块。**位图位置为1，表示空闲，0表示占用**，位图是1bit表示一个磁盘块的使用情况。

> `memset`函数：将s中当前位置后面的n个字节用 ch 替换并返回 s 。
>
> ```c
> void *memset(void *s, int ch, size_t n);
> ```

```c
//fs/fsformat.c
void init_disk() {
    int i, r, diff;

    // Step 1: Mark boot sector block.
    disk[0].type = BLOCK_BOOT;

    // Step 2: Initialize boundary. 
    nbitblock = (NBLOCK + BIT2BLK - 1) / BIT2BLK;//即NBLOCK/BIT2BLK向上取整，即需要多少个磁盘块存放位图
    nextbno = 2 + nbitblock;

    // Step 2: 初始化位图块，从第2块到第nbitblock+1块为位图区域 
    for(i = 0; i < nbitblock; ++i) {
        disk[2+i].type = BLOCK_BMAP;//标记这些磁盘块为位图区域
    }
    for(i = 0; i < nbitblock; ++i) { 
        memset(disk[2+i].data, 0xff, BY2BLK);//将data的前1024个字节（整个data）设为0xff，即都标记为未使用的状态。
    }
    if(NBLOCK != nbitblock * BIT2BLK) {//BIT2BLK=1024*8
        diff = NBLOCK % BIT2BLK / 8;//先取余，得到位图上有多少位无法使用，再除以8，得到有多少字节无法使用
        //将位图上磁盘不存在的部分置为0，表示无法使用这些磁盘块。之前for循环将此块全置为1
        memset(disk[nbitblock+1].data+diff, 0x00, BY2BLK - diff);
    }
       // Step 3: Initialize super block.
    disk[1].type = BLOCK_SUPER;
    //struct Super super;
    super.s_magic = FS_MAGIC;//魔数，用于识别该文件系统
    super.s_nblocks = NBLOCK;//记录本文件系统有多少磁盘块，1024
    super.s_root.f_type = FTYPE_DIR;//标记为根目录
    strcpy(super.s_root.f_name, "/");//目录文件名为"/"
}
```

##### 2.write_file

将内存中`path`文件写入dirf目录下。

> ```c
> #include <unistd.h>
> off_t lseek(int fildes, off_t offset, int whence);
> //改变文件的读写位置，在whence的基础上变化offset字节。返回当前的读写位置相对于文件开头有多少个字节
> /*whence:
> *SEEK_SET:设定offset为新的读写位置.
> *SEEK_CUR:当前读写位置增加offset
> *SEEK_END:读写位置指向文件末尾后再增加offset 个位移量
> */
> ```

```c
void write_file(struct File *dirf, const char *path) {
    int iblk = 0, r = 0, n = sizeof(disk[0].data);
    uint8_t buffer[n+1], *dist;
    struct File *target = create_file(dirf);//dirf目录下新建一个文件的FCB，并返回这个FCB的指针

    /* in case `create_file` is't filled */
    if (target == NULL) return;

    int fd = open(path, O_RDONLY);//使用的是库函数
 
    // Get file name with no path prefix.
    const char *fname = strrchr(path, '/');//该函数是查找'/'最后一次出现的位置，并返回其地址
    if(fname)
        fname++;//获得文件名
    else
        fname = path;//没有'/'，说明就是文件名
    strcpy(target->f_name, fname);
 
    target->f_size = lseek(fd, 0, SEEK_END);//获取文件大小，文件读写位置移到文件末尾。
    target->f_type = FTYPE_REG;
 
    // Start reading file.
    lseek(fd, 0, SEEK_SET);
    while((r = read(fd, disk[nextbno].data, n)) > 0) {//将fd文件的BY2BLK个字节传入到disk[nextbno].data中，即存储文件内容的磁盘块
        save_block_link(target, iblk++, next_block(BLOCK_DATA));//文件刚创建，iblk为0
    }
    close(fd); // Close file descriptor.
}
```

##### 2.1create_file

`create_file`，在当前目录下新建一个文件的FCB，并返回这个FCB的指针。

> 不必过分细究这个返回值具体是多少

```c
struct File *create_file(struct File *dirf) {
    struct File *dirblk;
    int i, bno, found,j;
    int nblk = dirf->f_size / BY2BLK;//目录一共占用了多少个磁盘块。这些磁盘块中存储着FCB。一个磁盘块最多可以存储16个FCB

    for(i=0;i<nblk;i++){//在所有的磁盘块中寻找是否有空的FCB
        if(i<NDIRECT)
            bno = dirf->f_direct[i];
        else 
            bno = ((int *)disk[dirf->f_indirect].data)[i];//MUST change type to int*
        dirblk = (struct File *)(disk[bno].data);//disk[bno].data[]中存放内容着的是FCB，因此data是一个指针，需要转化成struct File*，指向data[]中的FCB
        for(j=0;j<FILE2BLK;j++){//FILE2BLK = 16, 
            if(dirblk[j].f_name[0] == '\0'){//通过文件名判断是否FCB空闲
                return dirblk + j;
            }
        }
    }
    bno = make_link_block(dirf, nblk);//如果没有FCB空闲，增加一个磁盘块存储FCB
    return (struct File*)(disk[bno].data);
}   
```

##### 2.2make_link_block

`int make_link_block(struct File *dirf, int nblk)`，为目录dirf的第nblk个磁盘块指针，找一个数据块存储数据 

> 什么时候更新位图？？？为什么用nextbno???

```c
//fs/fsformat.c
//为一个目录dirf的第nblk个磁盘块指针，指向一个新的数据块
int make_link_block(struct File *dirf, int nblk) {
    int bno = next_block(BLOCK_FILE); //获取一个空闲的blockNO.
    save_block_link(dirf, nblk, bno);
    dirf->f_size += BY2BLK;//BY2BLK is 4KB，目录所占的磁盘块加了1个，因此目录大小加4KB
    return bno;
}
//获取一个空闲磁盘块，返回该磁盘块的id
int next_block(int type) {
    disk[nextbno].type = type;
    return nextbno++;
}

//为文件f的第nblk个磁盘块指针赋值，指向ID为bno的磁盘块
void save_block_link(struct File *f, int nblk, int bno)
{
    assert(nblk < NINDIRECT); // if not, file is too large !
    //NINDIRECT is 1024
    if(nblk < NDIRECT) { //NDIRECT is 10
        f->f_direct[nblk] = bno;
    }
    else {
        if(f->f_indirect == 0) {//
            // create new indirect block.
            f->f_indirect = next_block(BLOCK_INDEX);//f_indirect指向一个间接磁盘块，申请一个间接磁盘块
        }
        ((uint32_t *)(disk[f->f_indirect].data))[nblk] = bno;//为了方便计算，间接磁盘块的计数和直接磁盘块一致
    }
}
```

##### 3.flush_bitmap

根据nextbno更新位图。

```c
void flush_bitmap() {                        
    int i;
    // update bitmap, mark all bit where corresponding block is used.
    for(i = 0; i < nextbno; ++i) {
        ((uint32_t *)disk[2+i/BIT2BLK].data)[(i%BIT2BLK)/32] &= ~(1<<(i%32));
    }
}
```

## 文件系统进程底层函数

### 文件控制块

> 这是在哪里保存的，是磁盘还是内存 ？磁盘中也有，内存中也有，os要对磁盘进行操作先要将其读入内存中

```c
//include/fs.h
struct File {
    u_char f_name[MAXNAMELEN];  // filename，最大长度为128
    u_int f_size;           // file size in bytes
    u_int f_type;           // file type，分有普通文件 (FTYPE_REG) 和文件夹 (FTYPE_DIR) 
    u_int f_direct[NDIRECT];//NDIRECT=10个文件的直接指针，是存放文件的数据块的编号，最大可以表示40KB的文件
    u_int f_indirect;//是一个间接磁盘块的编号，该磁盘块存放指向文件内容的磁盘块指针(int*类型)，共有1024个。不使用前10个

    struct File *f_dir;     // the pointer to the dir where this file is in, valid only in memory.
    u_char f_pad[BY2FILE - MAXNAMELEN - 4 - 4 - NDIRECT * 4 - 4 - 4];
    //BY2FILE = 256
};
```

对于普通的文件，f_direct指向的磁盘块存储着文件内容，而对于目录文件来说，其指向的磁盘块存储着该目录下各个文件对应的的文件控制块FCB。

### 块缓存

块缓存指的是借助虚拟内存来实现磁盘块缓存的设计。**文件系统服务**是一个用户进程，将DISKMAP 到DISKMAP+DISKMAX 这一段虚存地址空间(0x10000000-0x4ffffff) 作为缓冲区，当磁盘读入内存时，用来映射相关的页。可提高文件系统性能。（注意，是文件系统服务进程是这么干的）

```c
//id为n的磁盘块(从0开始)，在内存中，被映射到虚拟地址空间的地址为DISKMAP+(n*BY2BLK)
#define DISKMAP     0x10000000
/* Maximum disk size we can handle (1GB) */
#define DISKMAX     0x40000000
```

<img src="C:\Users\24245\AppData\Roaming\Typora\typora-user-images\image-20220531172615175.png" alt="image-20220531172615175" style="zoom: 33%;" />

### fs/fs.c

**以下函数均在`fs/fs.c`中定义，用于文件系统服务进程，注意，地址空间是文件系统进程的**，由文件系统管理磁盘，是通过文件系统进程在内存中管理，但是其实真正是在管理。

#### 工具函数

* 计算blockno编号的磁盘块对应的虚拟地址

```c
//fs/fs.c
u_int diskaddr(u_int blockno)
{
    if(super != NULL && blockno >= super->s_nblocks)//blockno 是从0开始还是从1开始
        user_panic("diskaddr is wrong!");
    return DISKMAP + blockno*BY2BLK;   //什么是DISKMAP？块缓存的基地址
}
```

以下的函数都是在文件系统进程完成的，`bitmap`是操作系统文件系统中保留存放磁盘对应位图的。也就是说，他们都是从磁盘读到内存里，文件系统进行管理的。

#### 空闲磁盘块的分配

##### alloc_block

`alloc_block`函数，分配一个**空闲磁盘块**，then map it to memory，并返回该磁盘块编号。

```c
//fs/fs.c
int alloc_block(void)
{
    int r, bno;
    // Step 1: find a free block.
    if ((r = alloc_block_num()) < 0) { //alloc_block_num返回空闲的磁盘块编号
        return r;
    }
    bno = r;
    // Step 2: map this block into memory.
    if ((r = map_block(bno)) < 0) {
        free_block(bno);                            
        return r;
    }

    // Step 3: return block number.
    return bno;
}
```

* 下面介绍使用到的函数

1. `alloc_block_num`函数，返回空闲磁盘块号，并标记该磁盘块已被使用

   ```c
   //fs/fs.c
   int alloc_block_num(void)                                     
   {   
       int blockno;
   	//遍历bitmap，找到空闲的磁盘块号
       for (blockno = 3; blockno < super->s_nblocks; blockno++) {
           if (bitmap[blockno / 32] & (1 << (blockno % 32))) { // the block is free
               bitmap[blockno / 32] &= ~(1 << (blockno % 32));
               write_block(blockno / BIT2BLK + 2); // 更新磁盘的位图
               return blockno;
           }
       }
       // no free blocks.
       return -E_NO_DISK;
   }
   
   ```

2. `map_block(blockno)`函数。为编号为blockno的虚拟地址分配内存。当一个磁盘块载入到内存中时，我们需要为之分配对应的物理内存，当结束使用这一磁盘块时，需要释放对应的物理内存以回收操作系统资源。fs/fs.c 中的map_block 函数和unmap_block 函数实现了这一功能。

   ```c
   //fs/fs.c
   int map_block(u_int blockno)
   {
       // 判断该磁盘块是否已经被映射到内存
       if(block_is_mapped(blockno) != 0) 
           return 0;
       // 为该块分配一页内存
       return syscall_mem_alloc(0, diskaddr(blockno), PTE_V | PTE_R);           
   }
   ```

   * `block_is_mapped(blockno)`函数，检查blockno虚页是否已经被映射。若已经被映射，返回对应的起始虚拟地址；未被映射，返回0。（映射的意思是在内存中有物理页与之对应）

     ```c
     u_int block_is_mapped(u_int blockno)
     {
         u_int va = diskaddr(blockno);
         if (va_is_mapped(va)) {
             return va;
         }
         return 0;
     }
     ```

   * `va_is_mapped(va)`函数，检查虚拟地址va是否被映射到了物理页。返回1表示有物理页对应，0为无

     ```c
     u_int va_is_mapped(u_int va)
     {
         return (((*vpd)[PDX(va)] & (PTE_V)) && ((*vpt)[VPN(va)] & (PTE_V)));
     }
     ```

     

#### 磁盘块的回收

##### unmap_block

`unmap_block(blockno)`函数，解除磁盘块与物理页面的映射。

```c
void unmap_block(u_int blockno)
{
    int r;
    u_int addr = block_is_mapped(blockno);//通过vpt 和 vpd判断  
    //如果这个磁盘块页面不是空闲的而且内存中被写过，则需要先写回到磁盘
    if(!block_is_free(blockno) && block_is_dirty(blockno))
        write_block(blockno);
    r = syscall_mem_unmap(0, addr);
    //if(r<0) return;
    user_assert(!block_is_mapped(blockno));
}   
```

* 用到的函数:

1. `int block_is_free(blockno)`函数，判断磁盘块是否空闲，空闲返回1，否则返回0

   ```c
   //fs/fs.c
   int block_is_free(u_int blockno)
   {
       //struct Super* super
       if (super == 0 || blockno >= super->s_nblocks) {//判断超级块是否有效
           return 0;
       }
   	//u_int *bitmap
       if (bitmap[blockno / 32] & (1 << (blockno % 32))) {//bitmap在内存里
           return 1;
       }
   
       return 0;
   }
   ```

2. `free_block`函数，设置磁盘块为空闲。？？？为什么不更新磁盘的位图

   ```c
   void free_block(u_int blockno)
   {
       // Step 1: Check if the parameter `blockno` is valid (`blockno` can't be zero).
       if(blockno == 0 || (super!=NULL && blockno >= super->s_nblocks))
           return;
       bitmap[blockno / 32] |= (1 << (blockno % 32));
       // Step 2: Update the flag bit in bitmap.
   }
   ```

#### 读已使用的磁盘块

1. `read_block`函数，将指定编号的已经被使用的磁盘块读入到内存。首先检查这块磁盘块是否已经在内存中，如果不在，先分配一页物理内存，然后调用 ide_read 函数来读取磁盘上的数据到对应的虚存地址处。

   > 返回值为0表示操作成功。
   >
   > 返回值isnew, 如果磁盘块已经在内存中，则*isnew=0。否则为1；
   >
   > 返回值blk赋值为blockno的虚拟地址 

   ```c
   int read_block(u_int blockno, void **blk, u_int *isnew)
   {
       u_int va;
   
       // Step 1: validate blockno. 
       if (super && blockno >= super->s_nblocks) {
           user_panic("reading non-existent block %08x\n", blockno);
       }
       // Step 2: 确保blockno块不是空闲的。另外要确保bitmap已经从磁盘调入到内存了，否则bitmap为null
       if (bitmap && block_is_free(blockno)) {
           user_panic("reading free block %08x\n", blockno);
       }
   
       // Step 3: 转化为虚拟地址
       va = diskaddr(blockno);
   
       // Step 4: 读磁盘
       if (block_is_mapped(blockno)) { //磁盘块之前已经被read_block到内存里了
           if (isnew) {          
               *isnew = 0;
           }
       } else {            // the block is not in memory
           if (isnew) {
               *isnew = 1;
           }
           syscall_mem_alloc(0, va, PTE_V | PTE_R);//没有共享标记
           ide_read(0, blockno * SECT2BLK, (void *)va, SECT2BLK);
       }
   
       // Step 5: if blk != NULL, set `va` to *blk.
       if (blk) {
           *blk = (void *)va;
       }
       return 0;
   }
   ```

#### 写回磁盘块到磁盘

1. `write_block(blockno)`写回磁盘函数，将内存中的磁盘块内容写回磁盘。

   ```c
   void write_block(u_int blockno)
   {
       u_int va;
       if (!block_is_mapped(blockno)) {
           user_panic("write unmapped block %08x", blockno);
       }
       // Step2: write data to IDE disk. (using ide_write, and the diskno is 0)
       va = diskaddr(blockno);
       ide_write(0, blockno * SECT2BLK, (void *)va, SECT2BLK);//SECT2BLK是1个BLK有多少个sectors，有8个
       syscall_mem_map(0, va, 0, va, (PTE_V | PTE_R | PTE_LIBRARY));
   }   
   ```

#### 对文件操作

##### 将文件指定块读入内存

**该函数主要为后面的函数做铺垫**

1. `int file_get_block(struct File *f, u_int filebno, void **blk)`函数，将文件f内容的第filebno个磁盘块读入内存。返回值blk保存磁盘块所在内存的虚拟地址。

   ```c
   int file_get_block(struct File *f, u_int filebno, void **blk)
   {
       int r;
       u_int diskbno;
       u_int isnew;
   
       // Step 1: find the disk block number is `f` using `file_map_block`.
       if ((r = file_map_block(f, filebno, &diskbno, 1)) < 0) {//找到磁盘块的编号，如果文件没有这个磁盘块，创建一个
           return r;
       }
   
       // Step 2: read the data in this disk to blk.
       if ((r = read_block(diskbno, blk, &isnew)) < 0) {//读入内存，如果已经读入则isnew为0
           return r;
       }
       return 0;
   }
   ```

2. `file_map_block`函数，在文件控制块f中找到文件内容所在的第filebno个磁盘块，返回值diskbno保存该磁盘块编号。若`alloc`为1， 则如果该磁盘块不存在，那么分配一个

   ```c
   int file_map_block(struct File *f, u_int filebno, u_int *diskbno, u_int alloc)
   {
       int r;
       u_int *ptr;//*ptr = 文件f的filebno磁盘块的编号
   
       // Step 1: find the pointer for the target block.
       if ((r = file_block_walk(f, filebno, &ptr, alloc)) < 0) {
           return r;
       }
   
       // Step 2: if the block not exists, and create is set, alloc one.
       if (*ptr == 0) {
           if (alloc == 0) {
               return -E_NOT_FOUND;
           }
   
           if ((r = alloc_block()) < 0) {                        
               return r;
           }
           *ptr = r;
       }
   
       // Step 3: set the pointer to the block in *diskbno and return 0.
       *diskbno = *ptr;
       return 0;
   }
   ```

3. `int file_block_walk(struct File *f, u_int filebno, u_int **ppdiskbno, u_int alloc)`函数，在文件控制块f中找到文件内容所在的第filebno个磁盘块，存放该磁盘块编号的项的虚拟地址保存到*ppdiskbno。

   ```c
   int file_block_walk(struct File *f, u_int filebno, u_int **ppdiskbno, u_int alloc)
   {
       int r;
       u_int *ptr;
       void *blk;
   
       if (filebno < NDIRECT) {
           ptr = &f->f_direct[filebno];
       } else if (filebno < NINDIRECT) {
           if (f->f_indirect == 0) {//若f的间接磁盘块为空
               if (alloc == 0) {
                   return -E_NOT_FOUND;
               }
   
               if ((r = alloc_block()) < 0) {//若alloc，则分配一个磁盘块
                   return r;
               }
               f->f_indirect = r;
           }
   
           // Step 3: read the new indirect block to memory.
           if ((r = read_block(f->f_indirect, &blk, 0)) < 0) {//读间接磁盘块到内存，虚拟地址为blk
               return r;
           }
           ptr = (u_int *)blk + filebno;
       } else { //fileno超出最大
           return -E_INVAL;
       }
   
       // Step 4: store the result into *ppdiskbno, and return 0.
       *ppdiskbno = ptr;
       return 0;
   }
   ```

##### **查找文件*

`dir_lookup`函数，查找dir目录下是否有文件名为name的文件（相对文件名，包括目录），并将其PCB的指针保存在*file中。

```c
int dir_lookup(struct File *dir, char *name, struct File **file)
{   
    int r;
    u_int i, j, nblock;
    void *blk;
    struct File *f;
    
    // Step 1: Calculate nblock: how many blocks are there in this dir？
    nblock = dir->f_size / BY2BLK;
    for (i = 0; i < nblock; i++) {//遍历该目录的所有目录项 
        // Step 2: Read the i'th block of the dir.
        // Hint: Use file_get_block.
        if((r = file_get_block(dir, i, &blk)) < 0){//将dir的第i个磁盘块读入内存，地址为blk
            return r;
        }
        for(j = 0; j < FILE2BLK; j++){//FILE2BLK指一个磁盘块可以放多少FCB，共16个
            f = ((struct File*)blk) + j;
            if(strcmp(f->f_name, name) == 0){//这里不支持文件名和目录名相同时，不能查找出正确的结果
                *file = f; 
                f->f_dir = dir;//只在内存中有效
                return 0;
            }
        }
    }
    return -E_NOT_FOUND;
}

```

##### 打开文件

`int file_open(char *path, struct File **file)`，打开path路径下的文件，file为该文件的FCB

```c
//fs/fs.c
int file_open(char *path, struct File **file)                           
{       
    return walk_path(path, 0, file, 0);
} 
```

###### walk_path

`int walk_path(char *path, struct File **pdir, struct File **pfile, char *lastelem)`函数，在根目录下查找路径path，例如查找`/path1/file`1。

若能找到`file1`，则pdir为file1所在的直接目录，pfile为`file1`的FCB，并返回0

若没有找到`file1`，pdir为`path1`，lastelem为`file1`的名字。

若不存在`path1`，则返回负值，pdir和pfile与lastelem不赋值

```c
//fs/fs.c
int walk_path(char *path, struct File **pdir, struct File **pfile, char *lastelem)
{
    char *p;
    char name[MAXNAMELEN];//128
    struct File *dir, *file;
    int r;

    // start at the root.
    path = skip_slash(path);//跳过'/'
    file = &super->s_root;
    dir = 0;
    name[0] = 0;

    if (pdir) {
        *pdir = 0;
    }

    *pfile = 0;
    // find the target file by name recursively.
    while (*path != '\0') {
        dir = file;//dir为当前查找的目录
        p = path;

        while (*path != '/' && *path != '\0') {
            path++;
        }

        if (path - p >= MAXNAMELEN) {
            return -E_BAD_PATH;
        }

        user_bcopy(p, name, path - p);
        name[path - p] = '\0';//获取当前要查找的文件或目录名
        path = skip_slash(path);//path跳过'/'，故此可以查找'/dir/'

        if (dir->f_type != FTYPE_DIR) {//如果前一步的dir_lookup查找到的文件正好和目录同名，但是不是目录，说明没有这个目录
            return -E_NOT_FOUND;
        }
        if ((r = dir_lookup(dir, name, &file)) < 0) {//若找到，则重复while循环。没有找到，退出
            if (r == -E_NOT_FOUND && *path == '\0') {
                if (pdir) {
                    *pdir = dir;
                }

                if (lastelem) {
                    strcpy(lastelem, name);//将未找到的文件名赋值给lastelem
                }

                *pfile = 0;
            }

            return r;
        }
    }
    if (pdir) {
        *pdir = dir;
    }

    *pfile = file;
    return 0;
}
```

##### 创建文件

```c
//fs/fs.c
int file_create(char *path, struct File **file)
{
    char name[MAXNAMELEN];
    int r;
    struct File *dir, *f;

    if ((r = walk_path(path, &dir, &f, name)) == 0) {//判断文件是否存在
        return -E_FILE_EXISTS;
    }

    if (r != -E_NOT_FOUND || dir == 0) {//判断文件所在的各级目录是否存在
        return r;
    }

    if (dir_alloc_file(dir, &f) < 0) {
        return r;
    }

    strcpy((char *)f->f_name, name);
    *file = f;
    return 0;
}
```

* `dir_alloc_file(struct File *dir, struct File **file)`函数，分配新的FCB在目录dir下。函数执行逻辑与查找文件的`dir_lookup`函数相像。

  ```c
  //  Alloc a new File structure under specified directory. Set *file
  //  to point at a free File structure in dir.
  ```

##### 删除文件

```c
//fs/fs.c
// Overview:
//  Remove a file by truncating it and then zeroing the name.
int file_remove(char *path)
{
    int r;
    struct File *f;
    // Step 1: find the file on the disk.
    if ((r = walk_path(path, 0, &f, 0)) < 0) {
        return r;
    }

    // Step 2: truncate it's size to zero.
    file_truncate(f, 0);//文件大小变为0

    // Step 3: clear it's name.
    f->f_name[0] = '\0';

    // Step 4: flush the file.
    file_flush(f);//Flush the contents of file f out to disk.
    if (f->f_dir) {
        file_flush(f->f_dir);
    }

    return 0;
}
```

* `file_truncate(struct File *f, u_int newsize)`，将文件压缩到新的大小

  ```c
  void
  file_truncate(struct File *f, u_int newsize)
  {   
      u_int bno, old_nblocks, new_nblocks;
  
      old_nblocks = f->f_size / BY2BLK + 1;
      new_nblocks = newsize / BY2BLK + 1;
      
      if (newsize == 0) {
          new_nblocks = 0;
      }
  
      if (new_nblocks <= NDIRECT) {
          for (bno = new_nblocks; bno < old_nblocks; bno++) {
              file_clear_block(f, bno);//调用free_block，将f的第bno个磁盘块释放
          }
          if (f->f_indirect) {
              free_block(f->f_indirect);
              f->f_indirect = 0;
          }
      } else { 
          for (bno = new_nblocks; bno < old_nblocks; bno++) {
              file_clear_block(f, bno);
          }
      }
          
      f->f_size = newsize;
  }
  
  ```

  

## 用户进程的调用函数

引入文件描述符（file descriptor）作为用户程序管理、操作文件资源的方式。

### 文件描述符

当用户进程试图打开一个文件时，需要一个文件描述符来存储文件的基本信息和用户进程中关于文件的状态。文件系统进程会将文件描述符记录在内存中，同时将用户进程请求的地址映射到内存的同一物理页，一个文件描述符独占1页，并有固定的虚拟地址。当用户进程获取文件描述符后，再向文件系统发送请求将文件内容映射到内存指定空间.

```c
// file descriptor
struct Fd {
    u_int fd_dev_id;//文件对应的设备id
    u_int fd_offset;//文件指针当前位置
    u_int fd_omode;//文件打开模式
};
//fd_omode：
/* File open modes */
#define O_RDONLY    0x0000      /* open for reading only */
#define O_WRONLY    0x0001      /* open for writing only */
#define O_RDWR      0x0002      /* open for reading and writing */
#define O_ACCMODE   0x0003      /* mask for above modes */

#define O_CREAT     0x0100      /* create if nonexistent */
#define O_TRUNC     0x0200      //文件大小变为0
#define O_EXCL      0x0400      /* error if already exists */
#define O_MKDIR     0x0800      /* create directory, not regular file */
```

文件描述符中存储的数据毕竟是有限的，有的时候我们需要将`Fd*`强制转换为`Filefd*`从而**获取到文件控制块**，从而获得更多文件信息，比如文件大小等等。

```c
// file descriptor + file FCB
struct Filefd {
    struct Fd f_fd;
    u_int f_fileid;
    struct File f_file;
};  
```

#### 相关函数

**这些函数都是用户进程调用的**

1. `int fd_alloc(struct Fd **fd)`函数，找到一个空闲的文件描述符并返回

   ```c
   //user/fd.c
   int fd_alloc(struct Fd **fd)
   {
       u_int va;
       u_int fdno;
   
       for (fdno = 0; fdno < MAXFD - 1; fdno++) {//MAXFD = 32，找到最小fdno且没有被映射的
           va = INDEX2FD(fdno);//FDTABLE+(i)*BY2PG;
           					//FDTABLE = FILEBASE-PDMAP = 0x60000000 - 4MB
   
           if (((* vpd)[va / PDMAP] & PTE_V) == 0) {
               *fd = (struct Fd *)va;
               return 0;
           }
   
           if (((* vpt)[va / BY2PG] & PTE_V) == 0) {   //the fd is not used
               *fd = (struct Fd *)va;
               return 0;
           }
       }
   
       return -E_MAX_OPEN;
   }
   ```

2. `u_int fd2data(struct Fd *fd)`，该函数返回fd对应的文件要存储的虚拟地址。对于用户进程来说（非文件系统进程），每个fd对应着固定的文件存放地址。

   ```c
   u_int fd2data(struct Fd *fd)
   {
       return INDEX2DATA(fd2num(fd));
       //#define INDEX2DATA(i)   (FILEBASE+(i)*PDMAP)   FILEBASE=0x6000_0000
   }
   ```

3. `int fd2num(struct Fd *fd)`，返回这是第几个文件描述符

   ```c
   int fd2num(struct Fd *fd)
   {
       return ((u_int)fd - FDTABLE) / BY2PG;
   }     
   ```

4. `void fd_close(struct Fd *fd)`，解除fd的物理地址映射

   ```c
   void fd_close(struct Fd *fd)
   {
       syscall_mem_unmap(0, (u_int)fd);
   }     
   ```

5. `int fd_lookup(int fdnum, struct Fd **fd)`，查询fdnum编号的文件描述符，通过*fd返回。成功返回0，如果fdnum目前还不存在，返回负数

   ```c
   int fd_lookup(int fdnum, struct Fd **fd)
   {
       // Check that fdnum is in range and mapped.  If not, return -E_INVAL.
       // Set *fd to the fd page virtual address.  Return 0.
       u_int va;
   
       if (fdnum >= MAXFD) {//32
           return -E_INVAL;
       }
   
       va = INDEX2FD(fdnum);
   
       if (((* vpt)[va / BY2PG] & PTE_V) != 0) {   //the fd is used
           *fd = (struct Fd *)va;
           return 0;
       }
   
       return -E_INVAL;                                                                                                          
   }
   ```

   

6. `int close(int fdnum)`，关闭文件描述符fdnum，将其对应的文件关闭（虚拟地址解除映射），fd对应的虚拟地址解除映射。

   ```c
   int close(int fdnum)
   {
       int r;
       struct Dev *dev;
       struct Fd *fd;
   
       if ((r = fd_lookup(fdnum, &fd)) < 0//获取文件描述符fd
           ||  (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
           return r;
       }
   
       r = (*dev->dev_close)(fd);//执行对fd对应的文件的unmap，对于管道文件，会先解除fd的映射，后解除文件的映射
       fd_close(fd);             //执行对fd所在的页面的unmap
       return r;
   }
   ```

### 外设

* 外设结构体为

```c
//user/fd.h
struct Dev {
    int dev_id;
    char *dev_name;
    int (*dev_read)(struct Fd *, void *, u_int, u_int);
    int (*dev_write)(struct Fd *, const void *, u_int, u_int);
    int (*dev_close)(struct Fd *);
    int (*dev_stat)(struct Fd *, struct Stat *);
    int (*dev_seek)(struct Fd *, u_int);
};
```

* 文件对应的外设结构体为

```c
//user/file.c
struct Dev devfile = {                              
    .dev_id =   'f',
    .dev_name = "file",
    .dev_read = file_read,
    .dev_write = file_write,
    .dev_close = file_close,
    .dev_stat = file_stat,
}; 
```

相应的函数为：

1. `file_read(struct Fd *fd, void *buf, u_int n, u_int offset)`，读文件函数。文件已经被打开，并在内存中。从文件偏移offset处读取n字节到buf中。返回真正读到的字节数

   ```c
   //user/file.c
   static int file_read(struct Fd *fd, void *buf, u_int n, u_int offset)
   {
       u_int size;
       struct Filefd *f;
       f = (struct Filefd *)fd;
   
       // Avoid reading past the end of file.
       size = f->f_file.f_size;
   
       if (offset > size) {
           return 0;
       }
   
       if (offset + n > size) {
           n = size - offset;
       }
   
       user_bcopy((char *)fd2data(fd) + offset, buf, n);
       return n;
   }  
   ```

2. `int file_write(struct Fd* fd, void *buf, u_int n, u_int offset)`，向已经打开的文件写入buf，大小为n，文件偏移量为offset

   ```c
   static int
   file_write(struct Fd *fd, const void *buf, u_int n, u_int offset)
   {
       int r;
       u_int tot;
       struct Filefd *f;
   
       f = (struct Filefd *)fd;
   
       // Don't write more than the maximum file size.
       tot = offset + n;
   
       if (tot > MAXFILESIZE) {
           return -E_NO_DISK;
       }
   
       // Increase the file's size if necessary
       if (tot > f->f_file.f_size) {
           if ((r = ftruncate(fd2num(fd), tot)) < 0) {//改变文件大小，在内存中的
               return r;
           }
       }                  
       
       // Write the data
       user_bcopy(buf, (char *)fd2data(fd) + offset, n);
       return n;
   }
   ```

   * `int ftruncate(int fdnum, u_int size)`函数，如果size 小于原文件大小，则截减文件；大于，则增大文件大小

     ```c
     int ftruncate(int fdnum, u_int size)                                
     {
         int i, r;
         struct Fd *fd;
         struct Filefd *f;
         u_int oldsize, va, fileid;
     
         if (size > MAXFILESIZE) {
             return -E_NO_DISK;
         }
     
         if ((r = fd_lookup(fdnum, &fd)) < 0) {
             return r;
         }
     
         if (fd->fd_dev_id != devfile.dev_id) {
             return -E_INVAL;
         }
          f = (struct Filefd *)fd;
         fileid = f->f_fileid;
         oldsize = f->f_file.f_size;
         f->f_file.f_size = size;
     
         if ((r = fsipc_set_size(fileid, size)) < 0) {
             return r;
         }
     
         va = fd2data(fd);
     
         // Map any new pages needed if extending the file
         for (i = ROUND(oldsize, BY2PG); i < ROUND(size, BY2PG); i += BY2PG) {
             if ((r = fsipc_map(fileid, i, va + i)) < 0) {//将文件偏移i的磁盘块所在的物理页面映射给va+i地址
                 fsipc_set_size(fileid, oldsize);
                 return r;
             }
         }
          // Unmap pages if truncating the file
         for (i = ROUND(size, BY2PG); i < ROUND(oldsize, BY2PG); i += BY2PG)
             if ((r = syscall_mem_unmap(0, va + i)) < 0) {
                 user_panic("ftruncate: syscall_mem_unmap %08x: %e", va + i, r);
             }
     
         return 0;
     }
     ```

     



`dev_lookup(int dev_id, struct Dev **dev)`，查找id为dev_id的dev，通过dev返回

```c
int dev_lookup(int dev_id, struct Dev **dev)
{   
    int i;
    
    for (i = 0; devtab[i]; i++)
        if (devtab[i]->dev_id == dev_id) {
            *dev = devtab[i];
            return 0;
        }
    
    writef("[%08x] unknown device type %d\n", env->env_id, dev_id);
    return -E_INVAL;
}
```



### 用户读写文件函数

用户层使用的函数

#### open

`open(char *path, int mode)`函数，打开文件，如果成功返回该文件描述符的编号fdnum。函数分为两部分，第一部分先将path对应的文件描述符加载到内存里，第二部分则根据文件描述符信息将文件内容拷贝到内存里。一个文件最大4MB

```c
//user/file.c
int open(const char *path, int mode)
{
    struct Fd *fd;
    struct Filefd *ffd;
    u_int size, fileid;
    int r;
    u_int va;
    u_int i;
//第一部分，得到文件的文件描述符
    // Step 1: Alloc a new Fd, return error code when fail to alloc.
    if((r = fd_alloc(&fd)) < 0) //fd在FDTABLE这块
        return r;
    
    // Step 2: Get the file descriptor of the file to open.
    if((r = fsipc_open(path, mode, fd)) < 0) //fd此时已经被文件系统赋好值了，已经有相关文件信息了
        return r;

//第二部分，根据文件描述符内容将文件拷贝至内存
    va = fd2data(fd);//返回fd对应的文件要存储的虚拟地址，每个fd都对应有固定的文件存放位置    
    ffd = fd;//ffd此时在FDTABLE那块
    size = ffd->f_file.f_size;
    fileid = ffd->f_fileid;

    // Step 4: Alloc memory, map the file content into memory.
    for(i=0;i<size;i+=BY2PG){
        r = syscall_mem_alloc(0, va+i, PTE_V|PTE_R);
        if(r)
            return r;
        r = fsipc_map(fileid, i, va+i);//将文件的第i个磁盘块映射到va+i所在的物理页面
        if(r) 
            return r;
    }
    int fdnum = fd2num(fd);//get fdno of fd
    /*if(mode & O_APPND){
        seek(fdnum, size);
    }*/
    // Step 5: Return the number of file descriptor.
    return fdnum;
}
```



#### write

`write`函数，向文件描述符编号为fdnum的文件写入长度为n的buf。返回写入的长度

> ```c
> struct Dev {
> int dev_id;
> char *dev_name;
> int (*dev_read)(struct Fd *, void *, u_int, u_int);
> int (*dev_write)(struct Fd *, const void *, u_int, u_int);
> int (*dev_close)(struct Fd *);
> int (*dev_stat)(struct Fd *, struct Stat *);
> int (*dev_seek)(struct Fd *, u_int);
> };   
> ```

```c
//user/fd.c
int write(int fdnum, const void *buf, u_int n)
{
    int r;
    struct Dev *dev;//Dev is used to read and write data from corresponding device.
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd)) < 0//找到fdnum(0-31)对应的fd，若没有被使用则返回负值
        ||  (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
        return r;
    }

    if ((fd->fd_omode & O_ACCMODE) == O_RDONLY) {//判断fd是否只读
        writef("[%08x] write %d -- bad mode\n", env->env_id, fdnum);
        return -E_INVAL;
    }

    r = (*dev->dev_write)(fd, buf, n, fd->fd_offset);//向fd写入长度n的buf，起始偏移量为fd_offset

    if (r > 0) {
        fd->fd_offset += r;//更新文件读写位置
    }

    return r;        
}
```

#### read

`read`函数，从文件描述符编号为fdnum的文件中，当前偏移量下读n个字符到buf中，返回真实读入的长度

```c
//user/fd.c
int read(int fdnum, void *buf, u_int n)
{
    int r;
    struct Dev *dev;
    struct Fd *fd;

    // Similar to 'write' function.
    // Step 1: Get fd and dev.
    if((r = fd_lookup(fdnum, &fd)) < 0
        || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
        return r;
    }
    // Step 2: Check open mode.
    if((fd->fd_omode & O_ACCMODE) == O_WRONLY) {
        writef("[%08x] read %d -- bad mode\n", env->env_id, fdnum);
        return -E_INVAL;
    }
    // Step 3: Read starting from seek position.
    r = (*dev->dev_read)(fd, buf, n, fd->fd_offset);
    // Step 4: Update seek position and set '\0' at the end of buf.
    if(r > 0){
        fd->fd_offset += r;
    }
    ((char*)buf)[r] = '\0';//将读到的buf的标志结束符                                                                          
    return r;
}
```

#### remove

删除指定路径文件

```c
int remove(const char *path)
{                              
    return fsipc_remove(path);
}
```



### 进程请求文件系统服务函数

用户层使用的函数会调用这些函数

#### user/fsipc.c

1. `int fsipc(u_int type, void *fsreq, u_int dstva, u_int *perm)`函数，与文件系统通信的**核心函数**。将type(request code)发送给文件系统，`fsreq`为传递给文件系统的`req`结构体的起始地址。同时接受文件系统的回复，接收地址为dstva。返回值为收到文件系统进程传来的value

   ```c
   static int fsipc(u_int type, void *fsreq, u_int dstva, u_int *perm)
   {   
       u_int whom;
       // NOTEICE: Our file system no.1 process!
       ipc_send(envs[1].env_id, type, (u_int)fsreq, PTE_V | PTE_R);//用户进程可以访问到，pmap.c执行过指令boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R);     
       return ipc_recv(&whom, dstva, perm);
   }
   //type为用户向文件系统申请的请求，有：
   #define FSREQ_OPEN  1                                         
   #define FSREQ_MAP   2
   #define FSREQ_SET_SIZE  3
   #define FSREQ_CLOSE 4
   #define FSREQ_DIRTY 5
   #define FSREQ_REMOVE    6
   #define FSREQ_SYNC  7
   ```

2. `int fsipc_open(char *path, u_int omode, struct Fd *fd)`函数，打开文件请求给文件系统服务，包括路径名path和打开方式omode, 返回值文件描述符fd保存文件对应的描述符

   ```c
   //user/fsipc.c
   int fsipc_open(const char *path, u_int omode, struct Fd *fd)
   {
       u_int perm;
       struct Fsreq_open *req;
   
       req = (struct Fsreq_open *)fsipcbuf;//定义在entry.S的data段，共一页大小
   
       // The path is too long.
       if (strlen(path) >= MAXPATHLEN) {//MAXPATHLEN = 1024
           return -E_BAD_PATH;
       }
   
       strcpy((char *)req->req_path, path);
       req->req_omode = omode;
       return fsipc(FSREQ_OPEN, req, (u_int)fd, &perm);//fd接受文件系统 传来的文件基本信息
   }   
   ```

3. `fsipc_map`函数，将fileid文件的offset处磁盘块，映射到dstva虚拟地址里，一次映射大小为一页

   ```c
   int fsipc_map(u_int fileid, u_int offset, u_int dstva)
   {
       int r;
       u_int perm;
       struct Fsreq_map *req;
   
       req = (struct Fsreq_map *)fsipcbuf;
       req->req_fileid = fileid;//fileid为 打开文件的编号
       req->req_offset = offset;
   
       if ((r = fsipc(FSREQ_MAP, req, dstva, &perm)) < 0) {
           return r;
       }
   
       if ((perm & ~(PTE_R | PTE_LIBRARY)) != (PTE_V)) {
           user_panic("fsipc_map: unexpected permissions %08x for dstva %08x", perm,
                      dstva);
       }
   
       return 0;
   }   
   ```

4. `fsipc_remove`函数，请求文件系统删除一个文件。 

   ```c
   int fsipc_remove(const char *path)                                           
   {
       if(path[0] == '\0' || strlen(path) >= MAXPATHLEN)
           return -E_BAD_PATH;
       struct Fsreq_remove *req = (struct Fsreq_remove *)fsipcbuf;
       strcpy((char *)req->req_path, path);
       // Step 4: Send request to fs server with IPC.
       return fsipc(FSREQ_REMOVE, req, 0, 0);
   }
   ```

5. 

## 文件系统服务进程

### 文件系统服务的创建

文件系统本身是个用户进程，是envs[1]，由`ENV_CREATE(fs_serv)`创建，进程函数`umain`为

```c
//fs/serv.c
void umain(void){
    serve_init();
    fs_init();
    serve();
}
```

1. `serve_init`函数，初始化全局的文件打开记录表opentab

   > ```c
   > struct Open {
   > struct File *o_file;    //被打开文件的FCB
   > u_int o_fileid;     	// file id
   > int o_mode;     		// open mode
   > struct Filefd *o_ff;    // 地址从FILEVA开始，占一页
   > };   
   > ```

   ```c
   //fs/serv.c
   //初始化opentab是为了强制载入数据段???why?
   struct Open opentab[MAXOPEN] = { { 0, 0, 1 } };//???为啥不是4个值了
   void serve_init(void)
   {
       int i;
       u_int va;
       // Set virtual address to map.
       va = FILEVA;//0x6000_0000
       // Initial array opentab.
       for (i = 0; i < MAXOPEN; i++) {//MAXOPEN = 1024
           opentab[i].o_fileid = i; //fileid是从0到1024的，表示打开文件的编号             
           opentab[i].o_ff = (struct Filefd *)va;
           va += BY2PG;
       }
   }
   ```

    

2. `fs_init`函数，初始化文件系统

   ```c
   //fs/fs.c
   void fs_init(void)                               
   {
       read_super();//将磁盘中的超级块读入内存，保存在struct Super* super中。调用read_block(1, &blk, 0)函数
       check_write_block();//检查write_block是否工作
       read_bitmap();//read bitmap blocks from disk to memory， bitmap的起始地址就是磁盘中bitmap block映射在内存中的虚拟地址
   }
   ```

### 文件系统服务函数

`serve()`函数：文件系统进程创建后，运行的函数。

```c
//fs/serv.c
void serve(void)
{
    u_int req, whom, perm;

    for (;;) {
        perm = 0;
        req = ipc_recv(&whom, REQVA, &perm);//REQVA = 0x0ffff000，将whom发送过来的页面（保存有req结构体）映射到REQVA虚拟地址，权限为Perm。req为发送方传过来的value，即type

        if (!(perm & PTE_V)) {
            writef("Invalid request from %08x: no argument page\n", whom);
            continue; // just leave it hanging, waiting for the next request.
        }
        
        switch (req) {
            case FSREQ_OPEN:
                serve_open(whom, (struct Fsreq_open *)REQVA);
                break;

            case FSREQ_MAP:
                serve_map(whom, (struct Fsreq_map *)REQVA);
                break;

            case FSREQ_SET_SIZE:
                serve_set_size(whom, (struct Fsreq_set_size *)REQVA);
                break;

            case FSREQ_CLOSE:
                serve_close(whom, (struct Fsreq_close *)REQVA);
                break;

            case FSREQ_DIRTY:
                serve_dirty(whom, (struct Fsreq_dirty *)REQVA);
                break;

            case FSREQ_REMOVE:
                serve_remove(whom, (struct Fsreq_remove *)REQVA);
                break;

            case FSREQ_SYNC:
                serve_sync(whom);
                break;

            default:
                writef("Invalid request code %d from %08x\n", whom, req);
                break;
        }

        syscall_mem_unmap(0, REQVA);
    }
}
```

#### 服务函数

1. `serve_remove`函数，删除文件，并通知删除操作的请求方进程envid

   ```c
   //fs/serv.c
   void serve_remove(u_int envid, struct Fsreq_remove *rq)
   {
       int r;
       u_char path[MAXPATHLEN];
   
       // Step 1: Copy in the path, making sure it's terminated.
       // Notice: add \0 to the tail of the path
       user_bcopy(rq->req_path, path, MAXNAMELEN);
       path[MAXPATHLEN-1] = '\0';
       // Step 2: Remove file from file system and response to user-level process.
       // Call file_remove and ipc_send an approprite value to corresponding env.
       r = file_remove(path);
       ipc_send(envid, r, 0, 0);                                         
   }
   ```

2. `void serve_open(u_int envid, struct Fsreq_open *rq)`函数，

   ```c
   //fs/serv.c
   void serve_open(u_int envid, struct Fsreq_open *rq)//envid为请求打开文件的进程
   {
       writef("serve_open %08x %x 0x%x\n", envid, (int)rq->req_path, rq->req_omode);
   
       u_char path[MAXPATHLEN];
       struct File *f;
       struct Filefd *ff;
       int fileid;
       int r;
       struct Open *o;
   
       // Copy in the path, making sure it's null-terminated
       user_bcopy(rq->req_path, path, MAXPATHLEN);
       path[MAXPATHLEN - 1] = 0;
   
       // Find a file id.
       if ((r = open_alloc(&o)) < 0) {
           user_panic("open_alloc failed: %d, invalid path: %s", r, path);
           ipc_send(envid, r, 0, 0);
       }
   
       fileid = r;//r = o->o_fileid;
       // Open the file.
       if ((r = file_open((char *)path, &f)) < 0) {//打开文件，并将f指向FCB
           ipc_send(envid, r, 0, 0);
           return ;
       }
   
       // Save the file pointer.
       o->o_file = f;
   
       // Fill out the Filefd structure
       ff = (struct Filefd *)o->o_ff;
       ff->f_file = *f;
       ff->f_fileid = o->o_fileid;
       o->o_mode = rq->req_omode;
       ff->f_fd.fd_omode = o->o_mode;
       ff->f_fd.fd_dev_id = devfile.dev_id;
   
       ipc_send(envid, 0, (u_int)o->o_ff, PTE_V | PTE_R | PTE_LIBRARY);
       //将struct Filefd所在的页面传给用户进程
   }
   ```

   * `int open_alloc(struct Open **o)`函数，在opentab[]中找到`struct Filefd *o_ff`所在页引用次数为0或1的Open结构体，返回

     ```c
     int open_alloc(struct Open **o)
     {
         int i, r;
     
         // Find an available open-file table entry
         for (i = 0; i < MAXOPEN; i++) {//MAXOPEN = 1024
             switch (pageref(opentab[i].o_ff)) {//o_ff的虚拟地址位于0x6000_0000以上，注意这是文件系统进程
                 case 0://没有进程使用这struct Filefd页面
                     if ((r = syscall_mem_alloc(0, (u_int)opentab[i].o_ff,
                                                PTE_V | PTE_R | PTE_LIBRARY)) < 0) {
                         return r;
                     }//case0之后会执行case1
                 case 1://只有文件系统进程使用这个struct Filefd
                     opentab[i].o_fileid += MAXOPEN;//为什么需要加MAXOPEN？？？
                     *o = &opentab[i];
                     user_bzero((void *)opentab[i].o_ff, BY2PG);
                     return (*o)->o_fileid;
             }
         }
     
         return -E_MAX_OPEN;
     }
     ```

   * 

3. `serve_map`

   ```c
   void serve_map(u_int envid, struct Fsreq_map *rq)
   {
   
       struct Open *pOpen;
   
       u_int filebno;
   
       void *blk;
   
       int r;
   
       if ((r = open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
           ipc_send(envid, r, 0, 0);
           return;
       }
   
       filebno = rq->req_offset / BY2BLK;
   
       if ((r = file_get_block(pOpen->o_file, filebno, &blk)) < 0) {
           ipc_send(envid, r, 0, 0);
           return;
       }
   
       ipc_send(envid, 0, (u_int)blk, PTE_V | PTE_R | PTE_LIBRARY);
   }
   ```

   * `int open_lookup(u_int envid, u_int fileid, struct Open **po)`函数，

     ```c
     int
     open_lookup(u_int envid, u_int fileid, struct Open **po)
     {
         struct Open *o;
     
         o = &opentab[fileid % MAXOPEN];
     
         if (pageref(o->o_ff) == 1 || o->o_fileid != fileid) {//为什么？？？
             return -E_INVAL;
         }
     
         *po = o;
         return 0;
     }             
     ```

     

## 实验流程

## 创建磁盘镜像

`fsformat.c`文件创建磁盘镜像文件 gxemul/fs.img，这个磁盘镜像可以模拟与真实的磁盘文件设备之间的交互。

<img src="https://hyggge.github.io/2022/05/30/OS-Lab5函数解读/fsformat.drawio.svg" alt="img" style="zoom: 67%;" />

### fsformat.c

#### 主函数

明确一点，执行这个是用的linux的编译器，创建磁盘镜像的。

在Lab5，执行的指令是`./fsformat ./fs.img motd newmotd`，该指令创建了fs.img磁盘镜像，并将`fs/motd`和`fs/newmotd`文件存入磁盘的根目录下。

```c
int main(int argc, char **argv) {
    int i;
    
    init_disk();//初始化磁盘
    
    if(argc < 3 || (strcmp(argv[2], "-r") == 0 && argc != 4)) {
        fprintf(stderr, "\
Usage: fsformat gxemul/fs.img files...\n\
       fsformat gxemul/fs.img -r DIR\n");
        exit(0);
    }

    if(strcmp(argv[2], "-r") == 0) {//若有-r参数，则写入directory
        for (i = 3; i < argc; ++i) {
            write_directory(&super.s_root, argv[i]);
        }
    }
    else {//若无，正如lab5
        for(i = 2; i < argc; ++i) {
            write_file(&super.s_root, argv[i]);//将内存中的文件写入磁盘根目录下
        }
    }

    flush_bitmap();//更新磁盘bitmap，根据nextbno确定已经占用的磁盘块。注意在磁盘创建的过程中磁盘块是依次按顺序占用的
    finish_fs(argv[1]);//创建磁盘镜像
    //Finish all work, dump block array into physical file.
    return 0;
}              
```

#### 1.init_disk

对磁盘进行初始化，把disk[0]作为引导扇区和分区表所在的磁盘块，把disk[1]作为超级块。此外，还要根据磁盘块总个数（NBLOCK）为位图分配磁盘空间（分配nbitblock个磁盘块）。

#### 2.write_file

将内存中`path`文件写入dirf目录下。

> ```c
> #include <unistd.h>
> off_t lseek(int fildes, off_t offset, int whence);
> //改变文件的读写位置，在whence的基础上变化offset字节。返回当前的读写位置相对于文件开头有多少个字节
> /*whence:
> *SEEK_SET:设定offset为新的读写位置.
> *SEEK_CUR:当前读写位置增加offset
> *SEEK_END:读写位置指向文件末尾后再增加offset 个位移量
> */
> ```

```c
void write_file(struct File *dirf, const char *path) {
    int iblk = 0, r = 0, n = sizeof(disk[0].data);
    uint8_t buffer[n+1], *dist;
    struct File *target = create_file(dirf);//dirf目录下新建一个文件的FCB，并返回这个FCB的指针

    /* in case `create_file` is't filled */
    if (target == NULL) return;

    int fd = open(path, O_RDONLY);//使用的是库函数
 
    // Get file name with no path prefix.
    const char *fname = strrchr(path, '/');//该函数是查找'/'最后一次出现的位置，并返回其地址
    if(fname)
        fname++;//获得文件名
    else
        fname = path;//没有'/'，说明就是文件名
    strcpy(target->f_name, fname);
 
    target->f_size = lseek(fd, 0, SEEK_END);//获取文件大小，文件读写位置移到文件末尾。
    target->f_type = FTYPE_REG;
 
    // Start reading file.
    lseek(fd, 0, SEEK_SET);
    while((r = read(fd, disk[nextbno].data, n)) > 0) {//将fd文件的BY2BLK个字节传入到disk[nextbno].data中，即存储文件内容的磁盘块
        save_block_link(target, iblk++, next_block(BLOCK_DATA));//文件刚创建，iblk为0
    }
    close(fd); // Close file descriptor.
}
```

#### 2.1create_file

`create_file`，在当前目录下新建一个文件的FCB，并返回这个FCB的指针。

> 不必过分细究这个返回值具体是多少

```c
struct File *create_file(struct File *dirf) {
    struct File *dirblk;
    int i, bno, found,j;
    int nblk = dirf->f_size / BY2BLK;//目录一共占用了多少个磁盘块。这些磁盘块中存储着FCB。一个磁盘块最多可以存储16个FCB

    for(i=0;i<nblk;i++){//在所有的磁盘块中寻找是否有空的FCB
        if(i<NDIRECT)
            bno = dirf->f_direct[i];
        else 
            bno = ((int *)disk[dirf->f_indirect].data)[i];//MUST change type to int*
        dirblk = (struct File *)(disk[bno].data);//disk[bno].data[]中存放内容着的是FCB，因此data是一个指针，需要转化成struct File*，指向data[]中的FCB
        for(j=0;j<FILE2BLK;j++){//FILE2BLK = 16, 
            if(dirblk[j].f_name[0] == '\0'){//通过文件名判断是否FCB空闲
                return dirblk + j;
            }
        }
    }
    bno = make_link_block(dirf, nblk);//如果没有FCB空闲，增加一个磁盘块存储FCB
    return (struct File*)(disk[bno].data);
}   
```

##### 2.2make_link_block

`int make_link_block(struct File *dirf, int nblk)`，为目录dirf的第nblk个磁盘块指针，找一个数据块存储数据 

> 什么时候更新位图？？？为什么用nextbno???

```c
//fs/fsformat.c
//为一个目录dirf的第nblk个磁盘块指针，指向一个新的数据块
int make_link_block(struct File *dirf, int nblk) {
    int bno = next_block(BLOCK_FILE); //获取一个空闲的blockNO.
    save_block_link(dirf, nblk, bno);
    dirf->f_size += BY2BLK;//BY2BLK is 4KB，目录所占的磁盘块加了1个，因此目录大小加4KB
    return bno;
}
//获取一个空闲磁盘块，返回该磁盘块的id
int next_block(int type) {
    disk[nextbno].type = type;
    return nextbno++;
}

//为文件f的第nblk个磁盘块指针赋值，指向ID为bno的磁盘块
void save_block_link(struct File *f, int nblk, int bno)
{
    assert(nblk < NINDIRECT); // if not, file is too large !
    //NINDIRECT is 1024
    if(nblk < NDIRECT) { //NDIRECT is 10
        f->f_direct[nblk] = bno;
    }
    else {
        if(f->f_indirect == 0) {//
            // create new indirect block.
            f->f_indirect = next_block(BLOCK_INDEX);//f_indirect指向一个间接磁盘块，申请一个间接磁盘块
        }
        ((uint32_t *)(disk[f->f_indirect].data))[nblk] = bno;//为了方便计算，间接磁盘块的计数和直接磁盘块一致
    }
}
```

#### 3.flush_bitmap

根据nextbno更新位图。

```c
void flush_bitmap() {                        
    int i;
    // update bitmap, mark all bit where corresponding block is used.
    for(i = 0; i < nextbno; ++i) {
        ((uint32_t *)disk[2+i/BIT2BLK].data)[(i%BIT2BLK)/32] &= ~(1<<(i%32));
    }
}
```

## 实验难点

这次实验的主要难点主要集中在各个函数的调用关系，以及Lab5究竟做了哪几部分东西。

## 难点图示

### 用户进程和文件系统进程地址空间示意图

![Lab5地址空间](https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221639020.svg)

### 用户进程申请文件系统服务步骤

以`open`函数为例，当用户进程执行`open`函数后，

<img src="https://mdpics4lgw.oss-cn-beijing.aliyuncs.com/aliyun/202306221640193.svg" alt="用户申请文件系统" style="zoom:67%;" />



