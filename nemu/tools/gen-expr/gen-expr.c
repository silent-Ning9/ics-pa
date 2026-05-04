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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum {
  MAX_BUF = 65536,
  MAX_EXPR_DEPTH = 12,
  MAX_EXPR_USED = 60000,
};

static char buf[MAX_BUF] = {};
static int buf_pos = 0;

static inline uint32_t choose(uint32_t n) {
  return rand() % n;
}

static inline bool buf_has_space(int need) {
  return buf_pos + need < MAX_BUF;
}

static inline void gen_char(char c) {
  if (!buf_has_space(2)) return;
  buf[buf_pos++] = c;
  buf[buf_pos] = '\0';
}

static inline void gen_str(const char *s) {
  while (*s != '\0') gen_char(*s++);
}

static inline void gen_spaces(void) {
  while (choose(4) == 0) gen_char(' ');
}

static inline void gen_num(void) {
  char num_buf[32];
  snprintf(num_buf, sizeof(num_buf), "%u", (unsigned)(rand() % 100));
  gen_str(num_buf);
  gen_char('u');
}

static inline void gen_rand_op(void) {
  switch (choose(4)) {
    case 0: gen_char('+'); break;
    case 1: gen_char('-'); break;
    case 2: gen_char('*'); break;
    default: gen_char('/'); break;
  }
}

static void gen_rand_expr(int depth) {
  if (depth > MAX_EXPR_DEPTH || buf_pos > MAX_EXPR_USED) {
    gen_num();
    return;
  }

  gen_spaces();
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
      gen_spaces();
      gen_rand_op();
      gen_spaces();
      gen_rand_expr(depth + 1);
      break;
  }
  gen_spaces();
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);

  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }

  char code_buf[MAX_BUF + 1024] = {};
  int generated = 0;

  while (generated < loop) {
    buf_pos = 0;
    buf[0] = '\0';
    gen_rand_expr(0);

    int written = snprintf(
      code_buf, sizeof(code_buf),
      "#include <stdio.h>\n"
      "int main() { unsigned result = %s; printf(\"%%u\", result); return 0; }",
      buf
    );
    if (written <= 0 || written >= (int)sizeof(code_buf)) continue;

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc -O0 -Werror=div-by-zero /tmp/.code.c -o /tmp/.expr 2> /dev/null");
    if (ret != 0) continue;

    FILE *pp = popen("/tmp/.expr", "r");
    assert(pp != NULL);

    unsigned result = 0;
    if (fscanf(pp, "%u", &result) == 1) {
      printf("%u %s\n", result, buf);
      generated++;
    }
    pclose(pp);
  }

  return 0;
}
