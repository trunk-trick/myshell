// 头文件包含（你需要自己决定需要哪些）
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for chdir

#define EXIT_SUCESS 0
#define EXIT_FALURE 1

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
int main(int argc, char **argv) {
    lsh_loop();
    return EXIT_SUCESS;
}

void lsh_loop(void) {
    char * line;
    char ** args;
    int status;
    do {
        printf("> ");
        line = lsh_read_line();
        char ** args = lsh_split_line(line);
        status = lsh_execute(args);
    }
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

    if(getline(&line, &buff_size,stdin)) {
        if(feof(stdin)){
            exit(EXIT_SUCESS);
        } else {
            //错误会被自动设置(而且你也不知道是什么原因,当然只能是系统自己设置)
            perror("readline error");
            exit(EXIT_FALURE);
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
                exit(EXIT_FAILURE)
            }
            token = strtok(NULL,LSH_TOK_DELIM);
        }
    }
    tokens[position] = NULL;
    return tokens;
}

char* builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[]) (char**) {
    &lsh_cd, 
    &lsh_help, 
    &lsh_exit
};


int lsh_num_builtins () {
    return sizeof(builtin_str) / sizeof (char*);
}

// 内置函数实现

int lsh_cd(char ** args) {
    if(args[1] == NULL) {
        fprintf(stderr, "lsh : expected arguments to \"cd\" \n");
    } else {
        if(chdir(args[1] != 0)) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_execute(char ** args) {
    if (args[0] = NULL) {
        // An empty conmmad is input
        return 1;
    }
}
