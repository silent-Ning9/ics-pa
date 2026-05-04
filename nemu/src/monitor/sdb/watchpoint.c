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

#include "sdb.h"
#include <string.h>

#define NR_WP 32
#define WP_EXPR_MAX_LEN 255

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  char expr[WP_EXPR_MAX_LEN + 1];
  word_t last_val;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

static WP* new_wp() {
  Assert(free_ != NULL, "No free watchpoint");
  WP *wp = free_;
  free_ = free_->next;
  wp->next = head;
  head = wp;
  return wp;
}

static void free_wp(WP *wp) {
  Assert(wp != NULL, "free_wp: null wp");
  if (head == wp) {
    head = head->next;
  } else {
    WP *prev = head;
    while (prev != NULL && prev->next != wp) prev = prev->next;
    Assert(prev != NULL, "free_wp: wp not in active list");
    prev->next = wp->next;
  }

  wp->next = free_;
  free_ = wp;
}

bool wp_add(const char *expr_str) {
  if (expr_str == NULL) return false;
  while (*expr_str == ' ') expr_str++;
  if (*expr_str == '\0') return false;

  bool success = false;
  word_t val = expr((char *)expr_str, &success);
  if (!success) return false;

  WP *wp = new_wp();
  strncpy(wp->expr, expr_str, WP_EXPR_MAX_LEN);
  wp->expr[WP_EXPR_MAX_LEN] = '\0';
  wp->last_val = val;

  printf("Watchpoint %d: %s (init = " FMT_WORD ")\n", wp->NO, wp->expr, wp->last_val);
  return true;
}

bool wp_delete(int no) {
  WP *cur = head;
  while (cur != NULL) {
    if (cur->NO == no) {
      free_wp(cur);
      printf("Watchpoint %d deleted\n", no);
      return true;
    }
    cur = cur->next;
  }
  return false;
}

void wp_display(void) {
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("Num\tWhat\tValue\n");
  for (WP *cur = head; cur != NULL; cur = cur->next) {
    printf("%d\t%s\t" FMT_WORD "\n", cur->NO, cur->expr, cur->last_val);
  }
}

bool wp_check_and_update(void) {
  bool hit = false;
  for (WP *cur = head; cur != NULL; cur = cur->next) {
    bool success = false;
    word_t new_val = expr(cur->expr, &success);
    if (!success) continue;
    if (new_val != cur->last_val) {
      printf("Watchpoint %d triggered: %s\n", cur->NO, cur->expr);
      printf("Old value = " FMT_WORD "\n", cur->last_val);
      printf("New value = " FMT_WORD "\n", new_val);
      cur->last_val = new_val;
      hit = true;
    }
  }
  return hit;
}
