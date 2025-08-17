#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct list_head {
  struct list_head *next;
  struct list_head *prev;
} list_head_t;

#define LIST_INIT(name) {&(name), &(name)}

static inline void list_init(list_head_t *h) {
  h->next = h;
  h->prev = h;
}

static inline bool list_is_empty(const list_head_t *h) { return h->next == h; }

static inline void list_insert_tail(list_head_t *h, list_head_t *n) {
  n->prev = h->prev;
  n->next = h;
  h->prev->next = n;
  h->prev = n;
}

static inline void list_insert_head(list_head_t *h, list_head_t *n) {
  n->next = h->next;
  n->prev = h;
  h->next->prev = n;
  h->next = n;
}

static inline void list_remove(list_head_t *n) {
  n->next->prev = n->prev;
  n->prev->next = n->next;
  n->next = n->prev = n; // Optional: poison self
}

#define container_of(ptr, type, member)                                        \
  ((type *)((char *)(ptr) - offsetof(type, member)))

#endif // !LIST_H
