/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

// #include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <time.h>
// #include <assert.h>
// #include <string.h>

// // this should be enough
// static char buf[65536] = {};
// static char code_buf[65536 + 128] = {}; // a little larger than `buf`
// static char *code_format =
// "#include <stdio.h>\n"
// "int main() { "
// "  unsigned result = %s; "
// "  printf(\"%%u\", result); "
// "  return 0; "
// "}";

// static void gen_rand_expr() {
//   buf[0] = '\0';
// }

// int main(int argc, char *argv[]) {
//   int seed = time(0);
//   srand(seed);
//   int loop = 1;
//   if (argc > 1) {
//     sscanf(argv[1], "%d", &loop);
//   }
//   int i;
//   for (i = 0; i < loop; i ++) {
//     gen_rand_expr();

//     sprintf(code_buf, code_format, buf);

//     FILE *fp = fopen("/tmp/.code.c", "w");
//     assert(fp != NULL);
//     fputs(code_buf, fp);
//     fclose(fp);

//     int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
//     if (ret != 0) continue;

//     fp = popen("/tmp/.expr", "r");
//     assert(fp != NULL);

//     int result;
//     ret = fscanf(fp, "%d", &result);
//     pclose(fp);

//     printf("%u %s\n", result, buf);
//   }
//   return 0;
// }


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static char buf[65536] = {};
static int buf_pos = 0;

// 选择一个随机数 [0, n-1]
static inline uint32_t choose(uint32_t n) {
    return rand() % n;
}

// 向 buf 中追加单个字符
static inline void gen_char(char c) {
    if (buf_pos < sizeof(buf) - 2) {
        buf[buf_pos++] = c;
        buf[buf_pos] = '\0';
    }
}

// 向 buf 中追加字符串
static inline void gen_str(const char *s) {
    while (*s) gen_char(*s++);
}

// 随机插入 0 个或多个空格
static inline void gen_space() {
    while (choose(4) == 0) { // 25% 的概率增加一个空格
        gen_char(' ');
    }
}

// 生成随机无符号数字
static inline void gen_num() {
    char num_buf[32];
    // 限制数字大小，避免过快溢出，也能测试到正常计算
    snprintf(num_buf, sizeof(num_buf), "%d", rand() % 100); 
    gen_str(num_buf);
    gen_char('u'); // 关键：追加 'u' 保证生成的 C 代码进行无符号运算
}

// 生成随机操作符
static inline void gen_rand_op() {
    switch (choose(4)) {
        case 0: gen_char('+'); break;
        case 1: gen_char('-'); break;
        case 2: gen_char('*'); break;
        case 3: gen_char('/'); break;
    }
}

// 递归生成随机表达式
static void gen_rand_expr(int depth) {
    // 防止递归过深导致栈溢出，以及 buf 溢出
    if (depth > 12 || buf_pos > 60000) {
        gen_num();
        return;
    }

    gen_space();
    switch (choose(3)) {
        case 0:
            gen_num();
            break;
        case 1:
            gen_char('(');
            gen_rand_expr(depth + 1);
            gen_char(')');
            break;
        default:
            gen_rand_expr(depth + 1);
            gen_space();
            gen_rand_op();
            gen_space();
            gen_rand_expr(depth + 1);
            break;
    }
    gen_space();
}

int main(int argc, char *argv[]) {
    int seed = time(0);
    srand(seed);
    int loop = 1;
    if (argc > 1) {
        sscanf(argv[1], "%d", &loop);
    }

    char code_buf[65536 + 1000];
    int i = 0;
    
    while (i < loop) {
        // 1. 初始化并生成随机表达式
        buf_pos = 0;
        buf[0] = '\0';
        gen_rand_expr(0);

        // 2. 将表达式嵌入到 C 代码中
        snprintf(code_buf, sizeof(code_buf),
                 "#include <stdio.h>\n"
                 "int main() { unsigned result = %s; printf(\"%%u\", result); return 0; }", buf);

        // 3. 将 C 代码写入临时文件
        FILE *fp = fopen("/tmp/.code.c", "w");
        fputs(code_buf, fp);
        fclose(fp);

        // 4. 编译 C 代码
        // 使用 -Werror=div-by-zero 在编译期屏蔽掉一些常量折叠时被发现的除零错误
        // 2> /dev/null 屏蔽编译器的错误输出信息，保持终端整洁
        int ret = system("gcc -O0 -Werror=div-by-zero /tmp/.code.c -o /tmp/.expr 2> /dev/null");
        if (ret != 0) {
            // 编译失败（大概率是编译期除以0被抓），直接丢弃，重新生成
            continue;
        }

        // 5. 运行编译出的程序并获取结果
        FILE *pp = popen("/tmp/.expr", "r");
        unsigned result = 0;
        
        // 6. 读取结果，如果读取失败说明程序运行时崩溃了（运行时除零异常）
        if (fscanf(pp, "%u", &result) == 1) {
            // 运行成功，输出测试用例： 结果 表达式
            printf("%u %s\n", result, buf);
            i++; // 只有成功生成并计算出结果，才算完成一个用例
        }
        
        pclose(pp);
    }
    return 0;
}