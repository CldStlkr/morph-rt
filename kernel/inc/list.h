#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct list_head {
  struct list_head *next;
  struct list_head *prev;
} list_head_t;

#define list_iter(pos, head)                                                   \
  for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_iter_mut(pos, n, head)                                            \
  for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define container_of(ptr, type, member)                                        \
  ((type *)((char *)(ptr) - offsetof(type, member)))

static inline void list_init(list_head_t *h) {
  h->next = h;
  h->prev = h;
}

static inline void list_insert_before(list_head_t *n, list_head_t *pos) {
  n->next = pos;
  n->prev = pos->prev;
  pos->prev->next = n;
  pos->prev = n;
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
  n->next = n->prev = n; // Poison self
}

static inline void list_move_to_tail(list_head_t *h, list_head_t *n) {
  list_remove(n);
  list_insert_tail(h, n);
}

static inline void list_move_to_head(list_head_t *h, list_head_t *n) {
  list_remove(n);
  list_insert_head(h, n);
}

#endif // !LIST_H
