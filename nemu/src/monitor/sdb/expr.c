/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ,
  TK_NUM,      // decimal number
  TK_HEX,      // hex number
  TK_REG,      // register
  TK_DEREF,    // pointer dereference

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"-", '-'},           // minus
  {"\\*", '*'},         // multiply (or dereference)
  {"/", '/'},           // divide
  {"\\(", '('},         // left parenthesis
  {"\\)", ')'},         // right parenthesis
  {"0x[0-9a-fA-F]+", TK_HEX},  // hex number
  {"[0-9]+", TK_NUM},   // decimal number
  {"\\$[a-zA-Z0-9]+", TK_REG}, // register
  {"==", TK_EQ},        // equal
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[65536] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            // skip spaces
            break;
          default:
            // record token
            tokens[nr_token].type = rules[i].token_type;
            int len = substr_len < 32 ? substr_len : 31;
            strncpy(tokens[nr_token].str, substr_start, len);
            tokens[nr_token].str[len] = '\0';
            nr_token++;
            break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

/* Check if expression is surrounded by a matched pair of parentheses */
static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  int balance = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') balance++;
    else if (tokens[i].type == ')') balance--;
    
    if (balance < 0) return false;
    
    // 关键修复：如果在到达最后一个字符之前，括号的匹配度已经降到了 0，
    // 说明首尾的括号并不是相互匹配的一对 (例如 "(1) + (2)")。
    if (balance == 0 && i != q) {
      return false;
    }
  }

  return (balance == 0);
}

/* Find the main operator in expression [p, q] */
static int find_main_op(int p, int q) {
  int op = -1;
  int min_prio = 5; // Start with priority higher than any operator

  // Find the operator with lowest priority that is not in parentheses
  int balance = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') balance++;
    else if (tokens[i].type == ')') balance--;
    else if (balance == 0) {
      // Not in parentheses, check if it's an operator
      int prio = 0;
      
      // 关键修复：+ 和 - 的优先级更低，意味着它们应该被“最后”计算，
      // 所以应该赋予它们更小的值，这样才能被 min_prio 捕获，作为主运算符（树的根）。
      if (tokens[i].type == '+' || tokens[i].type == '-') prio = 1;
      else if (tokens[i].type == '*' || tokens[i].type == '/') prio = 2;
      else prio = 0;

      if (prio > 0) {
        if (prio < min_prio) {
          op = i;
          min_prio = prio;
        } else if (prio == min_prio) {
          // 同等优先级：选择最右侧的运算符作为主运算符，
          // 因为在语法树中最后计算右侧，这恰好实现了运算的“左结合性”(Left-associative)。
          op = i;
        }
      }
    }
  }

  return op;
}

/* Recursive expression evaluation */
static uint32_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }

  if (p == q) {
    // Single token - should be a number
    if (tokens[p].type == TK_NUM) {
      *success = true;
      return (uint32_t)strtoul(tokens[p].str, NULL, 10);
    } else if (tokens[p].type == TK_HEX) {
      *success = true;
      return (uint32_t)strtoul(tokens[p].str + 2, NULL, 16);
    } else if (tokens[p].type == TK_REG) {
      *success = true;
      return isa_reg_str2val(tokens[p].str + 1, success);
    } else {
      *success = false;
      return 0;
    }
  }

  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }

  int op = find_main_op(p, q);
  if (op == -1) {
    // No operator found, single token
    if (p == q) {
      return eval(p, q, success);
    }
    *success = false;
    return 0;
  }

  uint32_t val1 = eval(p, op - 1, success);
  if (!*success) return 0;

  uint32_t val2 = eval(op + 1, q, success);
  if (!*success) return 0;

  switch (tokens[op].type) {
    case '+': *success = true; return val1 + val2;
    case '-': *success = true; return val1 - val2;
    case '*': *success = true; return val1 * val2;
    case '/':
      if (val2 == 0) {
        *success = false;
        return 0;
      }
      *success = true;
      return val1 / val2;
    default:
      *success = false;
      return 0;
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  *success = true;
  return (word_t)eval(0, nr_token - 1, success);
}
