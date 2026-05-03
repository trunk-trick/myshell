// 头文件包含（你需要自己决定需要哪些）
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for chdir
#include <sys/wait.h>


// 常量定义
#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
//DELIM 是 delimiter 的缩写
#define LSH_TOK_DELIM " \t\r\n\a"

// 函数声明
char *lsh_read_line(void);
char **lsh_split_line(char *line);
int lsh_launch(char **args);
int lsh_execute(char **args);

// 内置命令函数声明
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

// 内置命令表（两个并行数组）
char *builtin_str[];
int (*builtin_func[])(char **);
int lsh_num_builtins(void);

// 主循环
void lsh_loop(void);


// 主函数
int main() {
    lsh_loop();
    return EXIT_SUCCESS;
}

void lsh_loop(void) {
    char * line;
    char ** args;
    int status;
    char cwd[1024];
    do {
        // 获取当前目录并打印提示符
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s> ", cwd);
        } else {
            printf("> ");  // 获取失败时退化为简单提示符
        }

        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
}

char* lsh_read_line(void) {
    char * line = NULL;
    ssize_t buff_size = 0;
    // getline 返回 -1 只有两种情况：
    // 1. 读到文件末尾（EOF） : 用 feof(stdin) 检查
    // 2.发生错误 : 用 ferror(stdin) 检查
    // 你不需要自己 malloc，getline 会帮你做。
    // 注意：getline 保留换行符
    // getline 需要修改这两个变量,所以传入的是引用

    // 这里的 line是被 getline 自动调用 malloc 分配空间的,所以后面要 free (line) 
    if(getline(&line, &buff_size,stdin) == -1) {
        if(feof(stdin)){
            exit(EXIT_SUCCESS);
        } else {
            //错误会被自动设置(而且你也不知道是什么原因,当然只能是系统自己设置)
            perror("readline error");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

// printf("Hello\n"); ====> fprintf(stdout, "Hello\n");
// fprintf 的第一个参数：FILE *stream
// 这个 stream 可以是：
// 标准输出流：stdout
// 标准错误流：stderr
// 文件流：fopen() 返回的指针
// 其他流：如 fdopen() 返回的指针



char ** lsh_split_line (char* line) {
    //而 strtok '默认'不把换行符当fprintf(stderr, ".作分隔符,但是我们定义的 LSH_TOK_DELIM 有回车 "\n"
    int buff_size = LSH_TOK_BUFSIZE;
    int position = 0;
    char ** tokens = malloc(buff_size * sizeof (char*));
    char * token = NULL;
    if(!tokens) {
        // 为什么这里不使用fprintf 呢 ?
        // 因为我们知道
        // | 流   | 名称 | 默认去向 | 用途 |
        // |------|------|----------|------|
        // | `stdin`  | 标准输入 | 键盘 | 读取输入 |
        // | `stdout` | 标准输出 | 屏幕 | 正常输出 |
        // | `stderr` | 标准错误 | 屏幕 | 错误消息 |
        // 使用:
        // | 函数 | 去向 | 何时用 |
        // |------|------|--------|
        // | `printf("...")`          | `stdout` | 正常输出 |
        // | `fprintf(stderr, "...")` | `stderr` | 错误/诊断信息 |
        // | `perror("...")`          | `stderr` | 系统调用失败时（自动加错误描述） |
        // 并且 stderr 不会经过 缓冲区,直接输出

        fprintf(stderr,"lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }
    // 如果一开始 就是分割符，那么不会返回 NULL
    // 但是如果整个 line 都是 delimiter 那么就会返回 NULL 
    token = strtok(line,LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position ++;
        
        if(position >= buff_size ) {
            buff_size += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens,buff_size * sizeof(char*));
            if(!tokens) {
                // 标准错误输出
                // 为什么不用 printf 打印错误？
                // 如果用户这么执行: ./mysh > output.txt
                // 这时：
                // # - printf 的内容 → 去了 output.txt（用户可能看不到错误）
                // # - fprintf(stderr, ...) → 仍然显示在屏幕上（用户能看到错误）
                fprintf(stderr,"allocation errors \n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL,LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

char* builtin_str[] = {
    "cd",
    "exit"
};

int (*builtin_func[]) (char**) = {
    &lsh_cd,  
    &lsh_exit
};


int lsh_num_builtins () {
    return sizeof(builtin_str) / sizeof (char*);
}

// 内置函数实现

int lsh_cd(char ** args) {
    printf("Now you are using Tian's shell \n"); 
    if(args[1] == NULL) {
        fprintf(stderr, "lsh : expected arguments to \"cd\" \n");
    } else {
        // chdir 成功返回 0 失败返回 1
        if(chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_exit (char ** args) {
    return 0;
}

int lsh_launch(char **args) {
    // 为什么我们要单独给外部的程序开一个子进程呢??
    // 因为 exec() \ execvp() 会完全取代这个子进程(保留 pid)!
    // 一旦 exec("ls",args) 执行，那么
    // 步骤 1：清空当前进程的内存
    //      释放代码段（原来的 Shell 指令没了）
    //      释放数据段（全局变量没了）
    //      释放堆（malloc 的内存没了）
    //      释放栈（局部变量没了）
    // 步骤 2：加载新程序
    //      打开你指定的文件 ./ls
    //      读取它的 ELF 头部（可执行文件格式）
    //      将 ls 的代码段加载进来
    //      将 ls 的数据段加载进来
    //      初始化新的栈和堆
    // 步骤 3：跳转执行
    //      CPU 的指令指针（IP）指向新程序的入口点（_start 或 main）
    //      开始执行 ls 的第一条指令
    //      所以这个 launch 可以运行任何的外部程序(可执行文件)!
    pid_t pid, wpid;
    int status;
    pid = fork();
    // | 视角 | `pid` 的值 |
    // |------|-----------|
    // | 父进程 | 子进程的 PID（大于 0） |
    // | 子进程 | 0 |
    if (pid == 0) {
        // child process
        if(execvp(args[0],args) == -1) {
            perror("lsh");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("lsh");
        // why not we exit here ??
    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
            //WIFEXITED 和 WIFSIGNALED 是 
            // | 宏 | 什么时候为真（非 0） |
            // |---|---|
            // | `WIFEXITED(status)`   | 子进程正常退出（调用了 `exit` 或 `return`）|
            // | `WIFSIGNALED(status)` | 子进程被信号杀死（比如段错误、`kill -9`） |
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        // 这里的 waitpid实际上是对status进行修改，然后交由循环判断!
        // 可是为什么这里需要循环呢??
        // 因为我们知道: 虽然 waitpid一次就可以得到结果，但是问题是这并不意味着正真的退出!
        // 只有WIFEXITED 和 WIFSIGNALED是正真的退出了!
        // 那么那些不是真正的退出呢??
        // 用户按下 Ctrl+Z 时，前台进程组会收到 SIGTSTP 信号，默认动作是暂停进程。
        // $ ./long_running_program
        // ^Z
        // [1]+  Stopped                 ./long_running_program
        // 但是其实是可以通过 fg 回复的!!
    }
    return 1;
}

int lsh_execute(char ** args) {
    if (args[0] == NULL) {
        // An empty conmmad is input
        // 为什么我们是检测是否 args[0] = NULL 也就是第一个token是 空的(这其实是说明了全部的都是delimiter)
        return 1;
    }
    for (int i = 0; i < lsh_num_builtins();i++) {
        if(strcmp(args[0],builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return lsh_launch(args);
}
