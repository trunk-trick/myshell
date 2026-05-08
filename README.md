## README.md

```markdown
# lsh - 一个简单的 Linux Shell

一个用 C 语言从零开始编写的 Linux Shell，支持管道、重定向、别名、历史记录等功能。

## 功能

- 执行外部命令（`ls`、`cat`、`grep` 等）
- 内置命令：`cd`、`exit`、`history`、`alias`
- 管道（`|`），支持多级管道
- 输出重定向（`>`）
- 引号处理（`"` 和 `'`）
- 历史记录与扩展（`!!`、`!数字`）
- 别名系统（`alias` / `unalias`）
- 配置文件（`~/.lshrc`）
- `~` 展开为用户家目录

## 快速开始

```bash
# 编译
gcc -o lsh main.c

# 运行
./lsh

# 创建配置文件（可选）
cat > ~/.lshrc << EOF
alias ls='ls --color=auto'
alias ll='ls -la'
alias grep='grep --color=auto'
alias ..='cd ..'
EOF
```

## 使用示例

```bash
# 基本命令
/home/user> ls -la
/home/user> cd /tmp

# 管道
/home/user> ls | grep main | wc -l

# 引号
/home/user> echo "hello world"
/home/user> grep "main" file.txt

# 别名
/home/user> alias ll='ls -la'
/home/user> ll

# 历史
/home/user> history
/home/user> !!        # 执行上一条命令
/home/user> !5        # 执行历史中第 5 条命令

# 重定向
/home/user> ls > output.txt
```

## 学习过程

### 第一阶段：基础框架

从 `main` 函数和 `lsh_loop` 开始，实现了最基础的 REPL（Read-Eval-Print Loop）：

- `lsh_read_line`：使用 `getline` 读取用户输入
- `lsh_split_line`：使用 `strtok` 分割命令和参数
- `lsh_launch`：使用 `fork + execvp` 执行外部命令
- `lsh_execute`：调度内置命令和外部命令

### 第二阶段：内置命令

实现了三个基础内置命令：

- `cd`：使用 `chdir` 切换工作目录
- `exit`：退出 Shell
- `history`：查看命令历史

### 第三阶段：管道

这是最核心也最复杂的部分。

**核心概念理解：**

- **fd（文件描述符）**：进程内部的文件编号，是内核打开文件表的索引
- **pipe**：内核中的内存缓冲区，有读端和写端
- **dup2**：复制文件描述符，让管道端口"伪装"成标准输入/输出
- **fork 复制 fd**：子进程继承父进程的文件描述符表

**管道实现：**
```
ls | grep main | wc -l

cmd0 (ls)      cmd1 (grep)      cmd2 (wc)
stdout → pipe0 → stdin
               stdout → pipe1 → stdin
```

**踩过的坑：**

1. **后置自增 vs 前置自增**：`cmd_starts[cmd_index++]` 覆盖了第一个命令的指针，导致管道无输出
2. **循环变量混淆**：`cmd_starts[j]` 错写为 `cmd_starts[i]`，数组越界
3. **管道关闭不彻底**：父子进程都要关闭不用的管道端口，否则读端收不到 EOF 会永久阻塞
4. **`fflush(stdout)`**：提示符不以 `\n` 结尾时，需要手动刷新缓冲区

### 第四阶段：引号处理

用状态机重写了 `lsh_split_line`：

- 遇到 `"` 或 `'` 进入引号模式
- 引号内的空格不作为分隔符
- 每个 token 独立 `malloc`，不再依赖 `strtok` 原地修改

### 第五阶段：别名系统

实现了 `~/.lshrc` 配置文件和别名展开：

- 键值对映射：`alias ll='ls -la'` 存储为 `key="ll"`, `value="ls -la"`
- 只在命令位置展开别名（管道每个命令的第一个 token）
- `alias` 和 `unalias` 内置命令

### 第六阶段：重定向与展开

- `>` 输出重定向：使用 `open` + `dup2` 将 stdout 重定向到文件
- `~` 展开：替换为 `HOME` 环境变量
- `preprocess_pipe`：在 `|` 前后自动添加空格，支持 `ls|grep` 无空格写法

## 技术要点

### 为什么每个子进程都要关闭管道？

管道是共享资源，所有持有端口的进程都关闭后管道才真正关闭。如果子进程不关闭不用的端口：
1. 读端收不到 EOF（因为还有进程持有写端）
2. 文件描述符泄漏

### 为什么 waitpid 需要循环？

`WIFEXITED` 和 `WIFSIGNALED` 才是真正的退出。`Ctrl+Z` 会让进程进入 `WIFSTOPPED` 状态（暂停），此时不应该认为进程已退出。

### 为什么别名的值要 strdup？

`args` 数组在 `free_tokens` 时会被释放，别名表中的指针如果不复制就会变成野指针。

## 待实现功能

- [ ] 输入重定向 `<`
- [ ] 追加输出重定向 `>>`
- [ ] 后台运行 `&`
- [ ] 环境变量 `$HOME`、`$PATH`
- [ ] 通配符展开 `*`
- [ ] Ctrl+C 不退出 Shell
- [ ] Tab 补全

## 参考资料

- [Tutorial - Write a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) by Stephen Brennan
- APUE（Advanced Programming in the UNIX Environment）
```