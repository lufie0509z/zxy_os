#include <kernel/list.h>
#include <kernel/interrupt.h>
#include <kernel/debug.h>

void list_init (struct list* list) {
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;    
}

//在元素 before 的前面插入elem
void list_insert_before(struct list_elem* before, struct list_elem* elem) {
    //对于链表的操作需要是原子的
    enum intr_status old_status = intr_disable();

    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;
    
    intr_set_status(old_status);
}

//在队头插入一个元素（head的后面）
void list_push(struct list* plist, struct list_elem* elem) {
    list_insert_before(plist->head.next, elem);
}

//在队尾插入一个函数
void list_append(struct list* plist, struct list_elem* elem) {
    list_insert_before(&plist->tail, elem);
}

void list_remove(struct list_elem* pelem) {
    enum intr_status old_status = intr_disable();
    
    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;

    intr_set_status(old_status);
}

//弹出链表的第一个元素
struct list_elem* list_pop(struct list* plist) {
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;
}

bool list_empty(struct list* plist) {
    return plist->head.next == &plist->tail ? true : false;
}

uint32_t list_len(struct list* plist) {
    struct list_elem* elem = plist->head.next;
    uint32_t len = 0;
    while (elem != &plist->tail)
    {
        len++;
        elem = elem->next;
    }
    return len;
}
    
bool elem_find(struct list* plist, struct list_elem* obj_elem) {
    struct list_elem* elem = plist->head.next;
    while (elem != &plist->tail)
    {
        if (elem == obj_elem) return true;
        elem = elem->next;
    }
    return false;    
}

struct list_elem* list_traversal(struct list* plist, function func, int arg) {
    if (list_empty(plist)) return NULL;
    struct list_elem* elem = plist->head.next;
    while (elem != &plist->tail)
    {
        if (func(elem, arg)) return elem;
        elem = elem->next;
    }
    return NULL;
}
