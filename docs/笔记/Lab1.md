# Lab1

### 1.前置知识的学习

#### 编译与链接

首先编译器进行预处理，预处理会将头文件的内容添加到源文件中。`gcc -E filename`

printf函数的实现是在**链接(Link)**这一步骤中被插入到最终的可执行文件中的。而链接器通过.o文件中的信息进行链接，.o文件就是一种ELF文件。

链接的本质是，合并不同文件里相同的节

#### ELF文件

ELF 文件是一种对可执行文件、目标文件和库使用的文件格式。.o文件称为可重定位文件，还有两种文件类型为可执行文件和共享对象(shared object)，这两种文件都需要链接器对.o文件进行处理才能生成。用file命令可以查看文件类型。

ELF文件包含5个部分，分别为：

1. ELF Header，包括程序的基本信息，如下表

   | 英文                             | 中文                      | 命名        |
   | -------------------------------- | ------------------------- | ----------- |
   | Program Header Table file offset | 程序头表相对ELF文件偏移量 | e_phoff     |
   | Program header table entry size  | 程序头表每项(指针)的大小  | e_phentsize |
   | Program header table entry count | 程序头表项数              | e_phnum     |
   | Section header table file offset | 节头表相对ELF文件偏移量   | e_shoff     |
   | Section header table entry size  | 节头表每项的大小          | e_shentsize |
   | Section header table entry count | 节头表项数                | e_shnum     |

   更详细的：<img src="C:\Users\24245\AppData\Roaming\Typora\typora-user-images\image-20220422163649079.png" alt="image-20220422163649079" style="zoom:50%;" />

2. Section header table，在Elf文件中并不真实存在一个这样的结构体。但它的地址可以寻找section header。它由section header(节头表项)组成，节头表项的结构为Elf32_Shdr(section header)，给出section header table address，逐个加上entry size （从0开始加）就是每个section header的地址。

3. Sections for **Linkable**，Section节记录了程序的**代码段、数据段等各个段的内容**，**主要是编译和链接要用**，其中最为重要的3个Section节为：

   * .text 保存可执行文件的操作指令。 
   * .data 保存已初始化的全局变量和静态变量。
   * .bss 保存未初始化的全局变量和静态变量。

   纪录信息如下表：

   | 英文         | 中文                                                         | 变量名  |
   | ------------ | ------------------------------------------------------------ | ------- |
   | section addr | 如果本节的内容需要映射到进程空间中去，地址为映射的起始地址；如果不需要映射，此值为 0。 | sh_addr |

4. Program Header Table

   Segments for **executable**，记录了每一段数据需要**被载入到内存的哪个位置**.*其中的.bss段中的数据只是被标记，不被装入到可执行文件中，其数据在内存里补0，导致内存大小可能大于文件大小*

   | 英文                                    | 中文                                     | 变量     |
   | --------------------------------------- | ---------------------------------------- | -------- |
   | offset from elf file head of this entry | 该段相对ELF表头的偏移量                  | p_offset |
   | virtual addr of this segment            | 该段最终需要被加载到内存的哪个位置       | p_vaddr  |
   | file size of this segment               | 该段数据在文件中的大小，也就是现在的大小 | p_filesz |
   | memory size of this segment             | 该段数据未来在内存中的大小               | p_memsz  |

   注意，segment包含一个或多个section。程序头只对可执行文件或共享目标 文件有意义，对于其它类型的目标文件，该信息可以忽略。

#### MIPS程序地址内存布局

解释：

1. User Space(kuseg) 0x00000000-0x7FFFFFFF(2G)：用户模式，通过**MMU**映射到物理地址。
2. kseg0 0x80000000-0x9FFFFFFF(512MB)：将高地址3位(1位即可)清零，就转换为物理地址。通过cache访问。在0x8001_0000~0x8040_0000放内核
3. kseg1 0xA0000000-0xBFFFFFFF(512MB):将高地址3位清零，但**不通过 cache存取**，一般访问外设
4. kseg2 0xC0000000-0xFFFFFFFF(1GB):只在内核态下使用且 需要**MMU**的转换

#### Linker Script

lds类型文件，(可以用来控制内核加载地址（把内核文件放在模拟硬件的某个物理位置上）)。Linker Script中记录了各个section应该如何映射到segment， 以及各个segment应该被加载到的位置。

语法：视SECTIONS为基本命令，内部

```c
ENTRY(_start)         //指定程序从哪条指令开始执行
SECTIONS
{
  . = 0x10000;        // .为location counter，其值会随着output section的大小增加，
  .text : { *(.text) }//colon左侧的.text就是output section, {}里的 内容则是input section，将被放                         入output section中。*是通配符，表示所有输入文件的所有.text。
  . = 0x8000000;
  .data : { *(.data) }
  .bss : { *(.bss) }  //根据第一条，.bss将会存放在.data的下一个地址位置，当然为了安全可能链接器会产                         生一个小gap在两者之间
}
```

同时，可以通过`ENTRY(function name)`指令来设置程序入口，使链接后的程序从function开始执行第一条指令。

#### 处理变长参数

当参数个数不确定时，使用va_list宏进行动态处理。

va_list的使用方法：

a)  首先定义`va_list`型的变量，这个变量是指向参数的指针。

b)  然后用`va_start(va_list ap, lastarg) `宏初始化变量刚定义的va_list变量，使其指向第一个可变参数的地址。lastarg 是该函数最后一个命名的形参。

c)  然后`va_arg(va_list ap, 类型) `返回可变参数，va_arg的第二个参数是你要返回的参数的类型（如果多个可变参数，依次调用va_arg获取各个参数）。

d)  最后使用`va_end(va_list ap)`宏结束可变参数的获取。

例如：

```c
void printf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
	int num = va_arg(ap, int);
    va_end(ap);
}
```

原理解析：C语言程序，调用函数时，参数会被压入栈中，传参时，从右到左依次进栈，栈顶元素就是*fmt。可变参数列表va_list就是在对栈进行操作，ap就类似于指向栈中元素的指针。

#### .S文件

定义了程序的入口 函数，_start函数，用于初始化栈指针和CP0,并跳转到main函数

*LEAF、NESTED 和END三个宏。定义在 .h文件中*

*LEAF 用于定义那些不包含对其他函数调用的函数*

END用于结束函数的定义

### 2.操作系统的启动

Lab1主要讲解了操作系统的启动，内核如何启动，并在内核中实现了printf函数。内核，虽然是一个可执行文件vmlinux，但是是由xxx/文件夹下诸多模块、.o文件链接成的，所以本实验中修改各文件其实就是在修改内核。

首先，Makefile是内核的地图，里面提到的modules为模块，模块也是目标对象文件，但是是链接到内核中，为了扩充内核功能的。使用Makefile的总目标就是**vmlinux可执行文件**，也就是我们的内核。

内核vmlinux准备好之后，gxemul需要根据内核的各segment中的virtual address 将其加载到内存的相应位置上。由于载入内核时未建立虚存机制，不能使用MMU，而且kseg1一般用于访问外部设备，故放在kseg0的这个位置。

那如何放在这个位置呢？当内核文件的.o文件写好后，需要链接，在链接过程中section的地址就可以改变！*(由于segment是由section组成，section地址改变后，segment也跟着变了)* 而在链接过程中，.o文件被看成section的集合，最关心的3个section就是.data,.text,.bss了，因此，只要在链接过程中控制这三个section的位置，就可以将内核加载到正确的位置。那如何控制呢？通过自定义链接脚本Linker Script即可控制3个section链接后在内存中的排布信息。

#### 丰富内核：printf

这个printf函数是在要内核中实现。让控制台输出一个字符,实际上是对某一个内存地址写一串数据，这个操作由drivers/gxconsole/gxconsole.c 中的printcharc函数实现，由lib/printf.c的myoutput函数调用，具体的细节由print.c中的**lp_Print()**函数实现。

易错点主要是：牢记fmt指针处理完要执行++的操作。