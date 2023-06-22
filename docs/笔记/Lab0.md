# Lab0

可参考：[Shell](C:\Users\24245\Desktop\MdForStudy\Shell.md)

操作系统核心部分称为内核（英文：Kernel），与其相对的就是它最外层的壳（英文：Shell）。Shell 是用于访问操作系统服务的用户界面。

Linux命令在系统中有两种类型：内置Shell（外壳）命令和Linux命令。

## 1.基本 Linux命令

* 快捷键

- Ctrl+C 终止当前程序的执行
- Ctrl+Z 挂起当前程序
- Ctrl+D 终止输入（若正在使用Shell，则退出当前Shell）
- Ctrl+I 清屏

​    其中，**Ctrl+Z挂起程序后会显示该程序挂起编号，若想要恢复该程序可以使用 fg [job_spec] 即可**，job_spec即为挂起编号，不输入时默认为最近挂起进程。

```shell
ls [-al] [file]
-a:显示隐藏文件
-l:显示详细信息
用法实例：
ls -l | grep '^d'| wc -l
其中，ls -l列出详细信息，每个文件以一行的形式列出；grep 搜索以d打头的信息，也就是dir， wc -l 计算行数。这个命令也就是计算本目录下有多少子目录

rmdir 
用法:rmdir [选项]... 目录...，只能删除空目录

rm - remove files or directories
用法:rm [选项]... 文件...
选项（常用）：
        -r      递归删除目录及其内容
        -f      强制删除。忽略不存在的文件，不提示确认
        
cp 
用法:cp [选项]... 源文件... 目录
选项（常用）：
        -r      递归复制目录及其子目录内的所有内容
        
mv - move/rename file
用法:mv [选项]... 源文件... 目录
#mv file ../file_mv就是将当前目录中的file文件移动至上一层目录中且重命名为file_mv，故可以使用
mv oldname newname#重命名
```

**~**表示用户登录的目录，**pwd**可以显示当前文件夹的绝对路径，当使用**cd**进入路径时，可以用TAB键补足全名，两次Tab可以类似ls显示，

```
在需要键入文件名/目录名时，可以使用TAB键补足全名，当有多种补足可能时双击TAB键可以显示所有可能选项。你可以试着在屏幕上输入cd /h 然后按下TAB，就会自动补全为cd /home，如果你输入的是cd /，再按两下TAB，会给你看你可以的选择，有些像ls
```

使用rm删除时，可先创建一个回收站文件夹，使用cp命令先把要删除的文件保存进去，再删除

```shell
cp hello_world.c trashbin/
rm hello_world.c
也可以写成
mv hello_world.c trashbin/
```

### Linux进阶

1. 两个常见的查找命令：**find **& **grep**

```shell
find [path] -name filename
#可以在当前目录下递归的查找文件名为filename的文件，并输出它的文件路径
参数：
path:指定查找路径，省略则为当前目录下
-print "%P"
#打印该文件名，不包含路径
```

```shell
grep 
#- print lines matching a pattern可以根据正则表达式搜索文件中的内容，并将该文件的路径和pattern所在行输出
#用法:grep [选项]... PATTERN [FILE]...
#选项（常用）：
-a 不忽略二进制数据进行搜索
-i 忽略文件大小写差异
-r 从文件夹递归查找
-n 显示行号
#如 grep '^void' sig.c显示此文件以void开头的行。
```



```shell
tree
#根据命令生成文件树，类似于ls
用法: tree [选项] [目录名]
选项(常用)：
-a 列出全部文件
-d 只列出目录
```

2. Linux **文件调用权限**

分为3级：文件拥有者、群组、其他。使用chmod可以确定文件如何被他人调用

```shell
chmod
用法: chmod 权限设定字串 文件...
权限设定字串格式 :
[ugoa...][[+-=][rwxX]...][,...]
```

其中：u 表示该文件的拥有者，g 表示与该文件的拥有者属于同一个群组，o 表示其 他以外的人，a 表示这三者皆是。+ 表示增加权限、-表示取消权限、= 表示唯一设定权 限。r 表示可读取，w 表示可写入，x 表示可执行，X 表示只有当该文件是个子目录或 者该文件已经被设定过为可执行。

chmod也可以用数字进行权限设置：

```shell
chmod abc filename 
```

abc 为三个数字，分别表示拥有者，群组，其他人的权限。r=4，w=2，x=1，用这些数字的8进制的加和来表示权限。例如 chmod 777 file 和 chmod a=rwx file 效果相同。

3. **重定向和管道**

shell使用三种流：

* 标准输入：stdin ，由0表示
* 标准输出：stdout，由1表示
* 标准错误：stderr，由2表示

重定向和管道可以重定向以上的流。

三种流可以同时重定向，举例:

```shell
    command < input.txt 1>output.txt 2>err.txt
```

**管道**:

管道符号“|”可以连接命令：`    command1 | command2 | command3 | ...`

以上内容是将command1的stdout发给command2的stdin，command2的stdout发给command3的stdin，依此类推。

举例： `cat < my.sh | grep "Hello" > output.txt`

上述命令重定向和管道混合使用，功能为将my.sh的内容作为cat指令标准输入，cat指令stdout发给grep指令的stdin，grep在其中查找字符串，最后将结果输出到output.txt。



**diff**用来比较两个文件的差异

```
diff [选项] 文件 1 文件 2
常用选项
-b 不检查空格字符的不同
-B 不检查空行
-q 仅显示有无差异，不显示详细信息

```

#### 流编辑器

1. **sed**流编辑器，文件处理工具，可以实现对数据行进行替换、删除、新增、选取，注意，只是对输入文本的输入进行操作，并不会对输入文件产生影响(加-i可以)

   sed是处理的是输入流，对当前文件读一行处理一行，不具备行计算能力，

```shell
sed [选项] '命令' 输入文本 #命令两边的''不能少，既可以是双引号和单引号，双引号是为了加转义\和变量。输入文本即为文件
#选项(常用):
-n: 使用安静模式。在一般 sed 的用法中，输入文本的所有内容都会先被输出。加上 -n 参数后，则只有经过 sed 命令处理的内容才会被显示。
-e: 进行多项编辑，即对输入行应用多条 sed 命令时使用。可以用一个sed实现多个sed操作，如
# sed -e '3a\str' -e '2a\str' my.txt
# sed -e '8p' -e '32p' -n file 
-i: 直接修改读取的档案内容，而不是输出到屏幕。即保存运算结果到文件
#命令(常用)：
a : 新增， a 后紧接着\，在当前行的后面添加一行文本
c : 取代， c 后紧接着\，用新的文本取代本行的文本
i : 插入， i 后紧接着\，在当前行的上面插入一行文本
d : 删除，删除当前行的内容，sed -n  '/main/d' my.txt 输出除了有main的行
p : 显示，把选择的内容输出。通常 p 会与参数 sed -n 一起使用。如sed -n '/main/p' my.txt,将my.txt中含有main的行打印出来
# 使用这些命令前，要加行号，如：
# sed '2,$d' my.txt， 表示剔除从第2行到最后一行所有内容
s : 取代，格式为 s/re/string，re 表示正则表达式，string 为字符串，功能为将正则表达式替换为字符串。例如：sed '3s/re/string/g' my.txt 将my.txt中的第三行替换，不改变my.txt
#例如who | sed 's/ .*//'显示每一行的第一个单词

```

练习：写一个shell脚本，输入参数，完成字符串替换

```shell
#!/bin/bash
sed -i "s/$2/$3/g" $1
#也可以写成sed -i 's/'$2'/'$3'/g' $1
#使用 bash ..sh my.txt char int
```

2. awk，流编辑器，有存储能力，

```shell
格式：awk 'program' file
program为一段程序，形式：
pattern1 {action1} pattern2 {action2}....，pattern为条件，action为命令。模式之间是或的关系
pattern:ed的正则表达式，逻辑表达式
action:如print,printf
awk -F, '{print $2}' my.txt#-F选项用来指定用于分隔的字符，默认是空格。所以该命令的$n就是用逗号分隔的第n项了。由于没有写pattern，无条件执行
awk '$1>2 {print $1,$3}' my.txt #打印第一项大于2的行的第1和第3项。
awk '{print NR,": "$0}' my.txt# NR是行号，$0为当前行，即显示行号和当前行
```

## 2.编译

### GCC的使用

使用GCC编译器是一种简单快捷生成可执行文件的途径

```markdown
语法:gcc [选项]... [参数]...
选项（常用）：
        -o      指定生成的输出文件；将多个文件进行编译连接：gcc testfun.c test.c -o test，注意：-o后面加生成的可执行程序 
        -S      将C代码转换为汇编代码
        -Wall   显示警告信息
        -c      只把文件转换为.o类型
        -M      列出依赖     
        -g 		生成调试信息
        -include filename   编译时用来包含头文件，功能相当于在代码中使用#include<filename> 
        -l<name> 编译时需要的库
        -Ipath   **编译(-o -c都要用)**时指定头文件目录，使用标准库时不需要指定目录，-I参数可以用相对路径，比如头文件在当前目录，可以用-I.来指定；如果不是，则用-I../src(举例)
参数：
    C源文件：指定C语言源代码文件
e.g.

$ gcc test.c
#默认生成a.out可执行文件
#Windows平台为a.exe

$ gcc test.c -o test
#使用-o选项生成名为test的可执行文件,注意，这样不会生成test.o
#也可以这样，
$ gcc -c test.c
$ gcc test.o -o test

```

### makefile的使用

推荐：

[makefile介绍 — 跟我一起写Makefile 1.0 文档 (seisman.github.io)](https://seisman.github.io/how-to-write-makefile/introduction.html)

Makefiles are a simple way to organize code compilation. 

如果只有make，执行第一个目标。

command前如果没有@，执行make会先打印指令内容。

格式：

```makefile
target: dependencies

    command 1

    command 2

    ...

    command n
//target是我们构建(Build)的目标，可以使.o,.exe,label。而dependencies是构建该目标所需的其它文件或其他目标。之后是构建出该目标所需执行的指令（任意shell指令）。每一个commmand指令前必须有tab,每一个commmand依赖的文件需要在dependencies里。
```

工作原理：只要dependencies中有一个以上的文件比target新的话，就会执行command指令

问题：label和.exe如何区分

先创建**Makefile**文件，然后在里面输入上述格式，执行时使用指令 **make (target)**

#### 变量的使用

类似于c语言的macro，例如

```makefile
objects = main.o kbd.o command.o display.o \
     insert.o search.o files.o utils.o
```

\表示换行符，读起来方便。

使用时$(objects)就行，变量可以使用在目标、依赖目标、command以及其他地方

* 定义变量
  * ?= 如果变量没有被定义过，则执行；否则不执行，如`FOO ?= bar`
  * :=前面的变量不能使用后面的变量，只能使用前面已定义好了的变量。但如果使用了后面的变量，则视为没有后面的这个变量在式子里。在定义时扩展。
  * = 前面的变量可以使用后面的变量，在执行时扩展，允许递归扩展（使用要加$，定义不加）
  * +=可以给变量追加值
    * :=和=的区别：:=取得是此时变量的当前值，而=取得是最终值，推荐用:=

特殊变量：

$@指代当前目标，表示着目前规则中所有的目标的集合，就像一个数组

$< 指代第一个前置条件。比如，规则为 t: p1 p2，那么\$< 就指代p1。

$? 指代比目标更新的所有前置条件，之间以空格分隔。比如，规则为 t: p1 p2，其中 p2 的时间戳比 t 新，\$?就指代p2。

\$^ 指代所有前置条件，之间以空格分隔。比如，规则为 t: p1 p2，那么 $^ 就指代 p1 p2 。

\$* 指代匹配符 % 匹配的部分， 比如% 匹配 f1.txt 中的f1 ，$* 就表示 f1。

$(MAKE),也就是make，可以跟参数：--directory=xxx ,这个参数的意思是在xxx相对路径下 使用make指令

* 变量的高级用法：

替换变量中的共有的部分，其格式是 `$(var:a=b)`。把变量“var”中所有以“a”字串“结尾”的“a”替换成“b”字串。这里的“结尾”意思是“空格”或是“结束符”结尾。

如：

```
foo := a.o b.o c.o
bar := $(foo:.o=.c) bar 最后为a.c b.c c.c
```

3. 清空

每个Makefile中都应该写一个清空目标文件（ `.o` 和执行文件）的规则。

```makefile
.PHONY : clean
clean :
    -rm edit $(objects)
#-的意思是，如果有文件出现问题，不要管，继续执行
```

4. 引用其他的makefile

```makefile
include ./src/makefile
```

但如果要在不同目录下生成文件，可以用cd 的形式进入，并调用make

5. 在不同目录下寻找文件

* 可以在文件前加上路径。

* 也可以定义特殊变量 `VPATH`，make就会在当前目录找不到的情况下，到所指定的目录中去找寻文件。如`VPATH = src:../headers`, **:**分割目录名。**注意，这里的找文件是指makefile文件中对文件名进行引用时，比如在依赖文件里的文件在其他目录下，要使用VPATH**，但是gcc指令-I的文件要自己写出目录路径

* 也可以使用vpath关键字。指定不同的文件在不同的搜索目录。用法有以下三种

  * ```
    vpath <pattern> <directories>
    ```

    为符合模式\<pattern>的文件指定搜索目录\<directories>。

    ```
    vpath <pattern>
    ```

    清除符合模式\<pattern>的文件的搜索目录。

    ```
    vpath
    ```

    清除所有已被设置好了的文件搜索目录。

    <pattern>需要包含 `%` 字符。 `%` 的意思是匹配零或若干字符.例如 `%.h` 表示所有以 `.h` 结尾的文件。如：`vpath %.h ../headers`

#### 多目标与静态模式

多个目标同时依赖于一个文件，并且其生成的命令大体类似，可以使用多目标，此时使用$@.

静态模式可以更加容易地定义多目标的规则,

#### 命令执行

要让上一条命令的结果应用在下一条命令时，你应该使用分号分隔，例如

```
exec:
    cd /home/hchen; pwd 也可以用&&连接
```

#### 条件循环

1. 条件

```makefile
ifeq ($(CC),gcc)
  libs=$(libs_for_gcc)
else
  libs=$(normal_libs)
endif
```

2. 循环

```makefile
LIST = one two three
all:
    for i in $(LIST); do \
        echo $i; \
    done

# 等同于

all:
    for i in one two three; do \
        echo $i; \
    done
```



#### 使用函数

函数调用

```
$(<function> <arguments>)
```

函数名和参数以空格分割，参数间以逗号分割，

常见函数：

1. patsubst

```makefile
$(patsubst <pattern>,<replacement>,<text>)
*名称：模式字符串替换函数。

*功能：查找 <text> 中的单词（单词以“空格”、“Tab”或“回车”“换行”分隔）是否符合模式 <pattern> ，如果匹配的话，则以 <replacement> 替换。这里， <pattern> 可以包括通配符 % ，表示任意长度的字串。如果 <replacement> 中也包含 % ，那么， <replacement> 中的这个 % 将是 <pattern> 中的那个 % 所代表的字串。（可以用 \ 来转义，以 \% 来表示真实含义的 % 字符）

返回：函数返回被替换过后的字符串。
```

#### 自动化变量

这种变量会把模式中所定义的一系列的文件自动地挨个取出，直至所有的符合模式的文件都取完。只会出现在command中

#### 模式规则Pattern

模式匹配，目标的定义需要有 `%` 字符。 `%` 的意思是表示一个或多个任意字符，是一个匹配符。在依赖目标中同样可以使用 `%` ，只是依赖目标中的 `%` 的取值，取决于其目标。示例如下：

```makefile
%指的是OBJ变量中的？
%.o : %.c ; <command ......>;
#这是将当前目录下的.c文件编译成对应的对象文件
```

Makefile文件示例：

```makefile
IDIR =../include#定义include文件目录
CC=gcc
CFLAGS=-I$(IDIR)#gcc编译选项

ODIR=obj#定义obj文件目录
LDIR =../lib

LIBS=-lm#数学库

_DEPS = hellomake.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS)
#DEPS = ../include/hellomake.h

_OBJ = hellomake.o hellofunc.o 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
#即 OBJ = $(patsubst %,obj/%,hellomake.o hellofunc.o)
#会被替换为 obj/hellomake.o obj/hellofunc.o

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
# $@表示目标文件的集合，$<第一个依赖文件，也就是%.c集合，他们都是一个个取出来的。即：$@ 表示所有的目标的挨个值， $< 表示了第一个依赖目标的挨个值。该操作将所有的 .c 文件都编译成 .o 文件，注意，这不是第一个目标，亲测如果不写出具体名字，而只写%.o，不会认为是第一个目标的，因为第一个目标得有确定的名字。

hellomake: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)
# $^指所有依赖目标的集合，并且去掉重复的。
.PHONY: clean#表名clean是伪目标，make clean时不会检查是否有clean这个文件，否则如果有的话不会执行。同时无论依赖文件是否被修改，一定保证它被执行。总之，.PHONY的目标一定会执行
clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
```

## 3.Git

### 基本命令

1. 删除rm

```git
git rm --cached <file>：从暂存区删除文件，工作区则不做出改变。可以连用git commit和git push，将远程库的修改撤销掉
git rm <file>:从暂存区和工作区删除
如果删除之前修改过并且已经放到暂存区域的话，也就是说如果文件没有存到版本库中，则必须要用强制删除选项 -f。你看人家是为了保护你的文件吧！

git clean -n 演练，告知那些文件将被删除
git clean -f强力删除工作区里未被追踪（add）的文件
git clean -df 删除工作区里未被追踪的文件夹和文件
```

2. 回退

```
git reset --hard HEAD^ #回退到上一个版本，HEAD^^指上两个版本，HEAD~50是上50个版本，也可以使用git reset --hard <Hash-code> 来回退
git reset HEAD：暂存区的目录树会被重写，被 master 分支指向的目录树所替换，但是工作区不受影响，使暂存区返回到原来的状态
git reset HEAD <file>可以把暂存区的修改撤销掉。
也就是说，git reset HEAD只对暂存区进行操作
```

3. 文件替换

```
git checkout -- <file> 会用暂存区指定的文件替换工作区的文件，做题常用，可以丢弃工作区的修改，就是让这个文件回到最近一次git commit或git add时的状态。

git checkout HEAD <file>” 命令时，会用 HEAD 指向的 master 分支中的指定文件替换暂存区和以及工作区中的文件。git checkout 其实是用版本库里的版本替换工作区的版本，无论工作区是修改还是删除，都可以“一键还原”。
```

4. git 文件的四种状态

**未修改**（unmodified）  表示某文件在跟踪（add）后一直没有改动过或者改动已经被提交（commit）

5. 允许重复提交一个相同的代码

```
git commit -m "xxx" --allow-empty
```



### 远程操作

* 先有本地库，后有远程库

1. 关联远程仓库

```git
$ git remote add origin git@github.com:michaelliao/learngit.git
```

一个本地仓库可以关联多个远程仓库，这时，可以：

```git
git remote rm origin #先删除关联origin的关联关系
git remote add Gitee ...
git remote add GitHub ...
```

2. 本地库推到远程仓库。(第一次推送时在push后加-u表示master与远程库关联起来）

```
git push origin <本地分支名> : <远程分支名>
#若本地分支名和远程分支名相等，:后面内容可以不写

#也可以写成，自动在远程创建一个新的分支，并关联当前分支
git push origin <本地分支名>
```

* 先有远程库，后有本地库

1. 克隆远程库

```
$ git clone git@github.com:michaelliao/<yourfile>.git
$ cd //进入该文件夹
```

* **克隆注意事项**：

克隆时所有分支都被克隆，但只有HEAD指向的master分支被checkout（**检出某分支指的是在该分支有对应的本地分支，使用 git checkout 后会 在本地检出一个同名分支自动跟踪远程分支**）。使用git checkout name 可以自动切换到该分支。

获取远程仓库的更新

```git
git fetch <remote> <branch> #抓取远程名和远程分支名到本地，但不会合并
git merge #检查无误后，合并到当前分支
```

也可以直接使用：

```
git pull <remotename> <branchname> #相当于fetch + merge
```

### 分支操作

1. 创建分支

```
# 创建一个基于当前分支产生的分支，其名字为<branch-name>，新建分支不会切换到新分支上
$ git branch <branch-name>
```

2. 删除分支

```
# 删除一个名字为<branch-name>的分支，D为强制删除
$ git branch -d(D) <branch-name>
```

3. 查看所有分支

```git
# 查看所有的远程与本地分支
$ git branch -a
# 前面带*的分支是当前分支
# 带remotes是远程分支
```

4. 切换分支

```git
# 切换到<branch-name>代表的分支，这时候HEAD游标指向新的分支
$ git checkout <branch-name>
```

5. 合并分支

```
# 将分支name2合并到name1
git checkout name1
git merge name2 
```



## 4.Shell脚本

1. shell脚本的创建和使用

批处理文件里存有多条linux指令，执行时可以全部将其内容执行。例如要执行my.sh，使用指令

```shell
source my.sh
#或
./my.sh#执行当前目录下的my.sh，如果不写./ ，可能执行别的地方的
```

但是自己写的shell脚本不能直接运行，需要添加可运行权限：chmod +x my.sh

在脚本中将\#**!/bin/bash**写到首行，以保证我们的脚本默认会使用 bash。注释也是以#开头

2. shell传递参数和函数

例如my.sh内容为

```shell
echo $1
```

执行命令：`./my.sh msg`，就会将msg传给$1,$n 就是说从左往右数的第几个参数，$0是命令，也就是./my.sh

除此之外还有一些可能有用的符号组合 

*  $# 传递的参数个数 

*  $* 一个字符串显示传递的全部参数

另外，shell的函数也可以用类似方法传递参数。

函数定义：

```shell
    function 函数名 ()
    {
        commands
        [return int]
    }
# function和()可以省略一个
```

举例：

```
    fun(){
        echo $1
        echo $2
        echo "the number of parameters is $#"
    }
    fun 1 2
```

3. 变量

shell定义变量时，变量不加$;

使用变量时，变量前加$,如$name, ${name}，注意：只有使用变量时才加

单引号里的任何字符都会原样输出，**双引号**里可以**有变量**。

- 单引号里的任何字符都会原样输出，单引号字符串中的变量是无效的；
- 单引号字串中不能出现单独一个的单引号（对单引号使用转义符后也不行），但可成对出现，作为字符串拼接使用。
- 双引号里可以有变量
- 双引号里可以出现转义字符

4. 表达式

表达式要用``反引号括起来，运算符和变量之间要有**空格**

也可以用$[ 1024 + 2048]

举例：

```
    a=1
    while [ $a -ne 10 ]
    do
        mkdir file$a
        a=$[$a+1]
    done
```

5. 流程结构

   注意，以下的conditon,要加空格，如`[ -e test ]`，判断test文件是否存在

   1. if

   ```shell
     if condition
       then
           command1 
           command2
           ...
       fi
   或写成一行
   if condition; then command1; command2; ...; fi
   #也可以加elif
   if
   then
   ...
   elif []
   then 
   ...
   fi
   #也可以用else
   if []
   then 
   ...
   else
   ..
   fi
   ```

也就是说，如果有条件[]，如if和elif，后面要加then

      2. while

   ```
    while condition
    do
        commands
    done
   ```

   while语句可以使用continue和break这两个循环控制语句。

6. 运算符

&&连接两个command命令时，只有前一个为真（命令返回值 $? == 0），后面的命令才会执行

​					

## [5.Vim](https://www.runoob.com/linux/linux-vim.html)

用过的按键

描述范围时，**.**代表当前行，$代表最后一行，而且可以做数学运算，如`.,.+3p`意为显示当前和接下来的3行；也可以配合/xxx/，（//是查找，查找会返回当前行）

| 一般模式 | 说明                                                       |
| -------- | ---------------------------------------------------------- |
| dG       | 删除光标所在到最后一行的所有数据                           |
| yy       | 复制光标所在行                                             |
| p/P      | p 为将已复制的数据在光标下一行贴上，P 则为贴在游标上一行！ |
| /word    | 向下搜索word，加参数\c为忽略大小写，加\C为不忽略           |
| n/N      | n为重复前一个搜索的动作，N为反向                           |
| 自动对齐 | gg先到第一行，按=，然后按G调到最后一行。其中=是对齐模式    |
| .        | 重复上一个命令                                             |
| v        | 进入块模式，以块为单位进行操作                             |

光标移动更有效率：

| :n             | 到第n行                                                      |
| -------------- | ------------------------------------------------------------ |
| gg             | 到第1行                                                      |
| G              | 到最后一行                                                   |
| w              | 到下一个单词的开头                                           |
| e              | 到下一个单词的末尾                                           |
| b              | 到上一个单词 开头                                            |
| B              | 到上一个WORD开头，WORD是纯单词                               |
| %              | 匹配括号移动（先把光标移动到括号上，%会自动移动到匹配的括号上） |
| *，#           | *匹配当前相同单词的下一个，#匹配上一个                       |
| : e <filename> | 打开别的文件                                                 |
| :bn            | 转到下一个文件，:bp是转到上一个文件                          |

vim打**:**就是临时进入ed状态，可以执行ed命令

| :模式          |                   |
| -------------- | ----------------- |
| ! <shell指令\> | 临时进入shell状态 |
|                |                   |
|                |                   |

| Insert模式   |          |
| ------------ | -------- |
| <c+n>或<c+p> | 自动补全 |
|              |          |
|              |          |

### ctags

跳转到函数

```
ctags -R *
Ctrl+]跳转
Ctrl+o返回
```

### 跳转文件

使用g+f命令来跳转到别的头文件，可以用

```c
:set path ? //查询当前查找路径有哪些
:set path+=./include,.//增加查询路径，当前路径和当前路径下的include文件夹，目录之间以,分割
```



## 6.tmux

```
ctrl+b %    垂直分屏
ctrl+b d    退出会话，回到shell的终端环境
ctrl+b o	切换tmux分屏
```

## 7. C语言

1. main函数传参

```c
int main(int argc, char *argv[])
//argc是int类型，表示运行程序的时候给main函数传递了几个参数,不必传argc，只传argv
//argv是一个字符串数组，这个数组用来存储argc个字符串，每个字符串就是我们给main函数传的一个参数argv[0] 存储程序的名称，argv[1] 是一个指向第一个命令行参数的指针，如果没有提供任何参数，argc 将为 1，就是指可执行文件名称
```

2. 文件读写

   1. 打开文件

      ```c
      FILE *fopen( const char * filename, const char * mode );
      //mode 若为“rb”，表示对二进制文件进行读
      ```

   2. 重置文件指针

      ```c
      int fseek(FILE *stream, long int offset, int whence)
      //使stream指向whence+offset处，whence可以为SEEK_SET,SEEK_CUR,SEEK_END，分别为文件开头、当前、末尾
      long int ftell(FILE *stream)
      //返回文件流当前位置，配合feek可以得到文件大小
      ```

   3. 文件读写

      ```
      size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
      将stream流读nmemb个size大小的数据到ptr中，返回成功读取的元素总数
      ```

3. 关键字

   1. **static**：对变量和函数修饰时，起到隐藏作用，别的源文件无法访问。修饰的局部变量在.data段中，不在栈里
   2. **extern**:对函数、变量声明时，表明他们是外部定义的，自己引用。

4. type定义

   ```c
   typedef unsigned short int u_int16_t//2字节
   typedef unsigned int u_int32_t//4字节
   typedef unsigned long u_int64_t//8字节
   ```

5. 结构体内存布局

   ```c
   struct node{
   	int i;
   	double j;
   };
   //指针的大小都是4字节，（1字节是8bits）也就是32bits；
   //结构体大小是以最大的元素所占的字节数为基准，分配的空间是该字节数的倍数。如以上的结构体大小为16字节（double占8字节）
   struct node1{
       int i;
       char c;
       double j;
   };//sizeof(node1)=16,因为内存布局中，i和c会共用8字节
   struct node2{
       int i;
       double j;
       char c;
   };//sizeof(node2)=24,内存布局中，i占8字节，之后double占8字节，然后，c占8字节
   ```

   注意，结构体大小与变量的定义顺序有关，按最大对齐。

6. 左移右移：对于左移，都是低位补0；但是右移>>，对于有符号数int，会根据符号位右移补0或1，对于无符号数，右移高位补0

7. sizeof和strlen的区别：sizeof指的是字符串占的内存的大小，算'/0'，但是strlen是算'/0'前的字符长度

8. 大小端 问题：大端存储，即数据的高位存在低地址位置，类似于字符串类型。

9. ```c
   int a = ({int b = 12; ++b});
   //a = 13
   ```

10. 函数指针和回调函数

    函数指针就是指向函数的指针，函数指针可以像一般函数一样被调用

    ```C
    //定义
    //返回值 (* func)(参数)
    int (* p)(int, int)
    ```

    **回调函数**是指，将函数指针作为某个函数的参数。

    ```c
    // 回调函数
    void populate_array(int *array, size_t arraySize, int (*getNextValue)(void))
    {
        for (size_t i=0; i<arraySize; i++)
            array[i] = getNextValue();
    }
    
    int getNextRandomValue(void)
    {
        return rand();
    }
    
    //调用，注意，getNextRandomValue不能加括号
    populate_array(myarray, 10, getNextRandomValue);
    ```

11. 关于头文件的事情：

    同一文件夹下，从一个源文件**a1.c** 里调用另一个源文件 **a2.c** 的函数。有两种办法：

    1. 在**a1.c**中声明一下被调用的函数，直接用。
    2. 包含一个头文件，其有被调用函数的声明。

## 8.内存和硬盘的区别

  1.内存(16G)是计算机的工作场所，硬盘(512G)用来存放暂时不用的信息。

  2.内存是半导体材料制作，分为RAM和ROM。硬盘是磁性材料制作。

  3.内存中的信息会随掉电而丢失，硬盘中的信息可以长久保存。

## 9.Gxemul调试

## 10.MIPS寄存器约定

`$t`寄存器为调用者保存，调用者可以选择保存也可以选择不保存。

`$s`寄存器为被调用者保存，也就是说你一个被调用的函数，如果 要想使用s寄存器，必须自己先保存