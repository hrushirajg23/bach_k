#include "list.h"
#include <stddef.h>

void INIT_LIST_HEAD(struct list_head* list)
{
    list->next = list;
    list->prev = list;
}

void INIT_LIST_NULL(struct list_head* node)
{
    node->next = NULL;
    node->prev = NULL;
}
void generic_add(struct list_head* beg, struct list_head* mid, struct list_head* end)
{
    mid->next = end;
    mid->prev = beg;
    beg->next = mid;
    end->prev = mid;
}

void generic_del(struct list_head *mid)
{
    mid->next->prev = mid->prev;
    mid->prev->next = mid->next;
}

void list_add(struct list_head *head, struct list_head *new)
{
    generic_add(head, new, head->next);
}
void list_add_tail(struct list_head *head, struct list_head *new)
{
    generic_add(head->prev, new, head);
}

void list_del(struct list_head* entry)
{
    generic_del(entry);
    /* INIT_LIST_NULL(entry); */
    INIT_LIST_HEAD(entry);
}

void list_del_init(struct list_head* entry)
{
    generic_del(entry);
    INIT_LIST_HEAD(entry);
}

int list_is_empty(struct list_head* head)
{
    if(head->next == head && head->prev == head){
        return 1;
    }
    else{
        return 0;
    }
}
