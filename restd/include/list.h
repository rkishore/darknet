#ifndef LIST_H
#define LIST_H
#include "darknet.h"

list *make_list();
int list_find(list *l, void *val);

void list_insert(list *, void *);


void free_list_contents(list *l);

int get_list_len(list *l);
  
#endif
