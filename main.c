// 头文件包含（你需要自己决定需要哪些）
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for chdir
#include <sys/wait.h>
#include <stdbool.h>


#define LSH_ALIAS_COUNT 100

typedef struct {
    char *key;
    char *value;    
} Alias;

Alias aliases[LSH_ALIAS_COUNT];
int alias_count = 0;

#define EXIT_SUCESS 0
#define EXIT_FALURE 1

// 常量定义
#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
//DELIM 是 delimiter 的缩写
#define LSH_TOK_DELIM " \t\r\n\a"
#define LSH_HISTORY_COUNT 1000

//数组
char * history_cmd[LSH_HISTORY_COUNT] = {NULL};
int history_count = 0;


// 函数声明
void free_tokens(char ** tokens);
char *lsh_read_line(void);
char **lsh_split_line(char *line);
int lsh_launch(char **args);
int lsh_execute(char **args);
void add_alias(char* key,char* value);
void remove_alias(char* key);
char** expand_alias_tokens(char** args);
char* preprocess_pipe(char* line);
void load_aliases();
void expand_tilde(char **args);

// 内置命令函数声明:
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_history(char **args);
char* lsh_history_expand(char *line);

// 内置命令表（两个并行数组）
char *builtin_str[];
int (*builtin_func[])(char **);
int lsh_num_builtins(void);
int lsh_alias(char** args);
int lsh_unalias(char** args);

// 主循环
void lsh_loop(void);


// 主函数
int main() {
    lsh_loop();
    return EXIT_SUCESS;
}

void lsh_loop(void) {
    char * line = NULL;
    char* expanded = NULL;
    char* processed;
    char ** args = NULL;
    int status;
    char cwd[1024] = {0};
    load_aliases();
    do {
        // 获取当前目录并打印提示符
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s> ", cwd);
        } else {
            printf("> ");  // 获取失败时退化为简单提示符
        }
        fflush(stdout);

        line = lsh_read_line();
        expanded = lsh_history_expand(line);
        processed = preprocess_pipe(expanded);

        printf("==========DEBUG=========\n");
        args = lsh_split_line(processed);
        expand_tilde(args);
        // 最后解析出来的args(tokens结果) 用来DEBUG !
        for (int i = 0; args[i] != NULL;i++) {
            printf("DEBUG :args[%d]: %s \n",i,args[i]);
        }
        printf("==========OUTPUT========\n");

        status = lsh_execute(args);
        free(line);
        free(expanded);
        free(processed);
        free_tokens(args);
    } while (status);
}

void expand_tilde(char **args) {
    char *home = getenv("HOME");
    if(!home) return;
    char* place;
    char* expanded = malloc(LSH_TOK_BUFSIZE);

    for(int i = 0;args[i] != NULL;i++) {
        if(place = strchr(args[i],'~')){
            int before = strlen(args[i]) - strlen(place);
            strncpy(expanded,args[i],before);
            strcpy(expanded + before,home);
            strcpy(expanded + before + strlen(home),args[i] + before + 1);
            free(args[i]);
            args[i] = expanded;
        }
    }
}

void load_aliases() {
    // 获取环境变量
    char *home = getenv("HOME");
    if(!home) return;

    char path[1024];
    snprintf (path,sizeof(path),"%s/.lshrc",home);
    FILE * fp = fopen(path,"r");

    if(!fp) {
        return;
    }

    char* line = NULL;
    size_t len = 0;

    while(getline(&line,&len,fp) != -1) {
        line[strcspn(line,"\n")] = '\0';

        if(line[0] == '\0' || line[0] == '#') continue;
        if(strncmp(line,"alias ",6) != 0) continue;

        char* rest = line + 6;
        while(strchr(LSH_TOK_DELIM,*rest)) rest++;
        char *eq = strchr(rest ,'=');
        if(!eq) return;

        // 实现分割
        *eq = '\0';
        char *key = rest;
        char *value = eq + 1;
        
        char *kend = key + strlen(key) - 1;
        while(kend > key && strchr(LSH_TOK_DELIM,*kend)){
            *kend = '\0';
            kend--;
        }

        while (strchr(LSH_TOK_DELIM,*value)) {value ++;};

        int vlen = strlen(value);
        if(vlen >= 2) {
            if((value[0] == '\'' || value[vlen - 1] == '\'') || (value[0] == '"' && value[vlen - 1] == '"')) {
                    value[vlen - 1] = '\0';
                    value[0] = '\0';
                    value++;
            }
        }
        add_alias(key,value);
    }

    free(line);
    fclose(fp);
}

void add_alias(char* key,char* value) {
    remove_alias(key);
    if(alias_count < LSH_ALIAS_COUNT) {
        // 这里一定要复制，因为后面的 free (args)把所有的token 都给你处理了!
        aliases[alias_count].key = strdup(key);
        aliases[alias_count].value = strdup(value);
        alias_count++;
    }
}

void remove_alias(char* key) {
    for(int i = 0; i< alias_count;i++) {
        if(strcmp(aliases[i].key,key) == 0) {
            free(aliases[i].key);
            free(aliases[i].value);
            for (int j = i; j < alias_count - 1;j++) {
                aliases[j] = aliases[j + 1];
            }
            alias_count--;
            return;
        }
    }
}

char * preprocess_pipe(char* line){
    int count = 0;
    for (int i = 0; line[i] != '\0';i++) {
        if(line[i] == '|') count++;
    }
    if(count == 0) return strdup(line);
    //这个动态分配内存是常见的操作
    //这里需要注意的一点是 strlen(str) 读取的时候是不计入 '\0'的!,所以我们分配往往是strlen(str) + 1
    char *newline = malloc(strlen(line) + count * 2 + 1);
    int pos = 0;
    for (int i = 0; line[i] != '\0';i++) {
        if(line[i] == '|') {
            newline[pos++] = ' ';
            newline[pos++] = '|';
            newline[pos++] = ' ';
        } else {
            newline[pos++] = line[i];
        }
    }
    newline[pos] = '\0';
    return newline;
}


// 在执行管道命令的过程中，我意识到了一个问题:
// 我一直使用 ... | grep "ls" 类似于这样的命令在操作，但是问题是这个本来我就没有处理
// 在真正的 shell 环境下面，我们知道 grep 加上 "" 是为了保护特殊字符 Like '*' 以及 ' '
// grep * 和  grep "*" 是完全不一样的 ，但是这个是shell
int lsh_execute_pipe(char **args) {
    int num_cmds = 1;
    int split_part_token;
    for(int i = 0; args[i] != NULL; i++) {
        //逐个计数token,记录管道中间的命令个数
        if(strcmp(args[i],"|") == 0) {
            num_cmds++;
        }
    }

    char** cmd_starts[num_cmds];
    int cmd_index = 0;
    cmd_starts[cmd_index] = args;

    for (int i = 0; args[i] != NULL;i++) {
        if(strcmp(args[i],"|") == 0) {
            //实现正真的分开!
            args[i] = NULL;
            cmd_starts[++cmd_index] = &args[i + 1];
        }
    }

    bool free_list[1024] = {0};

    //下面处理管道的 alias 设置:
    for (int i = 0;i <= cmd_index;i++) {
        for (int j = 0; j < alias_count;j++) {
            if(strcmp(cmd_starts[i][0],aliases[j].key) == 0) {
                cmd_starts[i] = expand_alias_tokens(cmd_starts[i]);
                free_list[i] = true;
            }
        }
    }

    // 管道数组:
    // 每个管道有两个文件描述符：[0] 读端，[1]写端
    int pipes[num_cmds - 1][2];
    for(int i = 0; i < num_cmds - 1;i++) {
        //创建管道
        //这个只是临时的!
        if(pipe(pipes[i]) == -1){
            perror("lsh : pipe");
            return 1;
        }
    }

    // Walkthrough:
    // 这里以 ls | grep main | wc -l 为例进行说明!
    //      i=0: ls
    //   - 不重定向 stdin（从终端读）
    //   - stdout → pipes[0][1]（写入管道0）

    //      i=1: grep main  
    //   - stdin → pipes[0][0]（从管道0读，即 ls 的输出）
    //   - stdout → pipes[1][1]（写入管道1）

    //      i=2: wc -l
    //   - stdin → pipes[1][0]（从管道1读，即 grep 的输出）
    //   - 不重定向 stdout（输出到终端）
    pid_t pids[num_cmds];
    for (int i = 0; i < num_cmds;i++) {
        //管道不保证子进程的执行顺序，也不需要保证。
        pids[i] = fork();
        if(pids[i] == 0) {
            //下面是重定向:
            if(i > 0) {
                // 那么这个是在干什么呢??
                // 所以相当于是我们使用 pipes 获取方向，然后把这个传给标准输入和输出
                // 使得原来指向终端的stdin 和 pipes[i - 1][0] 指向同样的东西---管道的读端(相当于pipe是中间商!)
                // 之后所有的 stdin 的读取操作其实都是从管道读取
                // 0 - 读取 || 1 - 写入
                dup2(pipes[i - 1][0],STDIN_FILENO);
            }
            if (i < num_cmds - 1) {
                dup2(pipes[i][1],STDOUT_FILENO);
            }
            // 为什么这里每个子进程都要close 管道呢??
            // 因为这里的fork() 出来的子进程是会完全复制父进程的地址空间，包括:
            // 代码,数据,堆，栈
            // 文件描述符(fd),是每个文件的唯一标识
            // 所以父和子进程同时持有管道的读端和写端!
            for (int j = 0;j < num_cmds - 1;j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // 检查是否是内置命令
            for (int j = 0;j < lsh_num_builtins();j++) {
                if(strcmp(builtin_str[j],cmd_starts[i][0]) == 0) {
                    // 对应的一组tokens
                    // 这里使用 exit 看起来奇怪但是这里是在子进程所以退出是合理的!
                    exit((*builtin_func[j])(cmd_starts[i]));
                }
            }
            //外部命令:
            if(execvp(cmd_starts[i][0],cmd_starts[i]) == -1) {
                perror("lsh");
                exit(EXIT_FAILURE);
            }
        }
    }
    // 父进程
    for (int i = 0; i < num_cmds - 1;i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // 等待所有的子进程结束
    for (int i = 0; i < num_cmds;i ++) {
        waitpid(pids[i],NULL,0);
    }
    // 父进程
    for(int i = 0; i < 1024;i++) {
        if(free_list[i]){
            free(cmd_starts[i]);
        }
    }
    return 1;
}

char* lsh_history_expand(char * dirty_line) {
    dirty_line[strcspn(dirty_line, "\n")] = '\0';
    char *line = dirty_line;
    if(line[0] == '!') {
        if(line[1] >= '0' && line[1] <= '9') {
            int num = atoi(line + 1);
            if(num < 1 || num > history_count){
                fprintf(stderr, "lsh: !%d : event not found!\n",num);
                return strdup("\n");
            }
            // 这里要 复制一份避免重复 free !
            return strdup(history_cmd[num - 1]);
        }
    }
    char * bang_bang = strstr(line, "!!");
    if(bang_bang != NULL){
        if(history_count == 0){
            fprintf(stderr,"lsh: no history");
            // return "\n"; 存在风险和错误!
            // 因为 常量是无法 free的 ，
            // 所以为了避免这个问题，同时也是保持语义的一致性，避免后面分类 我们仍然使用 strdup 分配内存
            return strdup("\n");
        }
        char* last_cmd = strdup(history_cmd[history_count - 1]);
        //解释: strcspn 是找到了第一个"\n" 的索引位置!,在这里是为了去掉"\n"，设置为 "\0" 相当于切除后面的所有内容!
        // ‘i ’ 和 "i" 一个是 char 一个是 char* 
        last_cmd[strcspn(last_cmd,"\n")] = '\0';

        size_t pre_fix_len = bang_bang - line;
        size_t suffix_len = strlen(bang_bang + 2);
        size_t new_length = pre_fix_len + suffix_len + strlen(last_cmd);

        char* result = malloc(new_length + 1);
        strncpy(result,line,pre_fix_len);
        strcpy(result + pre_fix_len,last_cmd);
        strcpy(result + pre_fix_len + strlen(last_cmd),bang_bang + 2);
        free(last_cmd);
        return result;
    }
    if(history_count < LSH_HISTORY_COUNT) {
        history_cmd[history_count] = strdup(line);
        history_count++;
    }
    return strdup(line);
}

char* lsh_read_line(void) {
    char * line = NULL;
    size_t buff_size = 0;
    // getline 返回 -1 只有两种情况：
    // 1. 读到文件末尾（EOF） : 用 feof(stdin) 检查
    // 2.发生错误 : 用 ferror(stdin) 检查
    // 你不需要自己 malloc，getline 会帮你做。
    // 注意：getline 保留换行符
    // getline 需要修改这两个变量,所以传入的是引用

    if(getline(&line, &buff_size,stdin) == -1) {
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
    // token = strtok(line,LSH_TOK_DELIM);
    // while (token != NULL) {
    //     tokens[position] = token;
    //     position ++;
        
    //     if(position >= buff_size ) {
    //         buff_size += LSH_TOK_BUFSIZE;
    //         tokens = realloc(tokens,buff_size * sizeof(char*));
    //         if(!tokens) {
    //             // 标准错误输出
    //             // 为什么不用 printf 打印错误？
    //             // 如果用户这么执行: ./mysh > output.txt
    //             // 这时：
    //             // # - printf 的内容 → 去了 output.txt（用户可能看不到错误）
    //             // # - fprintf(stderr, ...) → 仍然显示在屏幕上（用户能看到错误）
    //             fprintf(stderr,"allocation errors \n");
    //             exit(EXIT_FAILURE);
    //         }
    //     }
    //     token = strtok(NULL,LSH_TOK_DELIM);
    // }
    // tokens[position] = NULL;
    // return tokens;
    char * p = line;
    char* token_start = NULL;
    int token_len = 0;
    // 用于表示当前的引号类型
    char quote_char = 0;
    while(*p != '\0') {
        if (quote_char) {
            // 找到了一个括号的闭环!
            if(*p == quote_char) {
                tokens[position] = malloc(token_len + 1);
                strncpy(tokens[position],token_start,token_len);
                //实现分割
                tokens[position][token_len] = '\0';
                position++;

                if(position >= buff_size) {
                    buff_size += LSH_TOK_BUFSIZE;
                    tokens = realloc(tokens, buff_size * sizeof(char*));
                    if(!tokens) {
                        fprintf(stderr,"lsh : allocation errors");
                        exit(EXIT_FAILURE);
                    }
                }
                quote_char = 0;
                token_start = NULL;
                token_len = 0;
                p++;
            }
            else {
                token_len++;
                p++;
            }
        }
        else {
            // 核心处理 跳过 '"' 和 '\''
            if(*p == '\'' || *p == '"') {
                if(token_start){
                    tokens[position] = malloc(token_len + 1);
                    if(!tokens[position]) {
                        fprintf(stderr,"allocation errors\n");
                    }
                    strncpy(tokens[position],token_start,token_len);
                    //实现分割
                    tokens[position][token_len] = '\0';
                    position++;
                }
                quote_char = *p;
                token_start = p + 1;
                token_len = 0;
                p++;
            } else if (strchr(LSH_TOK_DELIM,*p)) {
                // 下面是分隔符的处理逻辑
                if(token_start) {
                    tokens[position] = malloc(token_len + 1);
                    strncpy(tokens[position],token_start,token_len);
                    tokens[position][token_len] = '\0';
                    position++;
                    if(position >= buff_size) {
                        // token数量不够了! 翻倍!
                        buff_size += LSH_TOK_BUFSIZE;
                        tokens = realloc (tokens, buff_size * sizeof (char*));
                        if(!tokens) {
                            fprintf(stderr, "lsh : allocation errors");
                            // 直接退出程序，这是最安全和稳健的做法!
                            exit(EXIT_FAILURE);
                        }
                    }
                    token_len = 0;
                    token_start = NULL;
                }
                p++;
            } else {
                if(!token_start) {
                    token_start = p;
                }
                token_len ++;
                p++;
            }
        }
    }
    if(token_start) {
        tokens[position] = malloc(token_len + 1);
        strncpy(tokens[position],token_start,token_len);
        tokens[position][token_len]= '\0';
        position++;
    }
    //用 NULL 结尾！
    tokens[position] = NULL;
    return tokens;
}

void free_tokens(char ** tokens) {
    for (int i = 0; tokens[i] != NULL;i++) {
        free(tokens[i]);
    }
    free(tokens);
}

char* builtin_str[] = {
    "cd",
    "exit",
    "history",
    "alias"
};
 
int (*builtin_func[]) (char**) = {
    &lsh_cd,  
    &lsh_exit,
    &lsh_history,
    &lsh_alias
};


int lsh_num_builtins () {
    return sizeof(builtin_str) / sizeof (char*);
}

// 内置函数实现

int lsh_history (char **args){
    for (int i = 0;i < history_count;i ++) {
        printf("%d %s\n", i + 1,history_cmd[i]);
    }
    return 1;
}

int lsh_alias(char** args) {
    if(args[1] == NULL) {
           for (int i = 0; i < alias_count;i++) {
                printf("alias: %s = '%s' \n",aliases[i].key,aliases[i].value);
           }
           return 1;
    }
    // 这里和 标准的 shell 不同的是 
    // 标准的 shell的处理: alias ec= "echo hello world" 是不合法的
    // 但是我这里因为是把空格当作分隔符token化了，所以自然就分开了。所以我觉得标准shell的处理是识别格式的!
    // 至于原因是什么我也不知道
    char * eq = strchr(args[1], '=');
    if(eq) {
        *eq = '\0';
        char* key = args[1];
        char* value = args[2];
        add_alias(key,value);
    }
    return 1;
}

int lsh_cd(char ** args) {

    //printf("===Tian's shell====\n");
    //printf("===redirecting to \"%s\"\n",args[1]);
    if(args[1] == NULL) {
        fprintf(stderr, "lsh : expected arguments to \"cd\" \n");
    } else {
        // 调试：打印参数，用引号包围以看到隐藏字符
        // chdir 成功返回 0 失败返回 1
        if(chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}

int lsh_exit (char ** args) {
    for (int i = 1;i <= history_count;i++) {
        free(history_cmd[i - 1]);
        // 这里要小心野指针，因为这里的虽然指针的内存被释放了，但是问题是仍然保留这指针!
        //所以我们让这个指向 NULL
        history_cmd[i - 1] = NULL;
    }
    return 0;
}

//这里的问题是，虽然lsh_launch 可以执行外部程序，但是正真的问题是这里只能是有独立可执行文件的!
//像是history 这样的内置命令的话就是不行的!
int lsh_launch(char **args) {
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

char** expand_alias_tokens(char** args) {
    char** expanded_args = malloc(LSH_ALIAS_COUNT * 8);
    char** split_part_args = NULL;
    int index = 0;
    for (int i = 0; i < alias_count;i++) {
        if(strcmp(aliases[i].key,args[0]) != 0) continue;
        args[0] = NULL;
        split_part_args = lsh_split_line(aliases[i].value);
        while (split_part_args[index] != NULL) {
            expanded_args[index] = strdup(split_part_args[index]);
            index++;
        }
        break;
    }
    // 这一行很重要为什么呢

    for (int i = 0; args[i] != NULL || (i == 0);i++) {
        if(args[i]){
            expanded_args[index] = args[i];
            index++;
        }
    }
    expanded_args[index] = NULL;

    free(split_part_args);
    return expanded_args;
}

int lsh_execute(char ** args) {
    char** expanded_args;
    if (args[0] == NULL) {
        // An empty conmmad is input
        // 为什么我们是检测是否 args[0] = NULL 也就是第一个token是 空的(这其实是说明了全部的都是delimiter)
        return 1;
    }
    for (int i = 0; args[i] != NULL;i ++) {
        //这里是 "|" 还是 '|' 呢 ?
        if(strcmp(args[i],"|") == 0) {
            return lsh_execute_pipe(args);
        } 
    }   

    // 这里的处理要注意了，展开只是在第一个token上面
    // 例如:
    // quest@Quest:~/myshell/myshell$ alias x="ls"
    // quest@Quest:~/myshell/myshell$ echo x
    // x
    // quest@Quest:~/myshell/myshell$ x
    // README.md  main  main.c  my_func  output  te  test   

    expanded_args = expand_alias_tokens(args);
    for (int i = 0; i < lsh_num_builtins();i++) {
        if(strcmp(expanded_args[0],builtin_str[i]) == 0) {
            //free(split_part_args);
            return (*builtin_func[i])(expanded_args);
        }
    }
    return lsh_launch(expanded_args);
}


