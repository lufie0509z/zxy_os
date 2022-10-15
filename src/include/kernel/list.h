#ifndef _LIB_KERNEL_LIST_H
#define _LIB_KERNEL_LIST_H
#include <kernel/global.h>

/**
 * 获取结构体内成员在结构体的偏移.
 * elem2entry 是将指针 elem_ptr 转换成 struct_type 类型的指
 * offset 用来获取结构体成员 member 在结构体中的偏移量。
 */ 
#define offset(struct_type,member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
	 (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

/**
 * 链表节点.
 */ 
struct list_elem {
   struct list_elem* prev; // 前躯结点
   struct list_elem* next; // 后继结点
};

/**
 * 链表结构.
 */ 

struct list {
   // head是队首,是固定不变的，不是第1个元素,第1个元素为head.next 
   struct list_elem head;
   // tail是队尾,同样是固定不变的 
   struct list_elem tail;
};

/**
 * 用于链表遍历的回调函数.
 */ 
typedef bool (function)(struct list_elem*, int arg);

void list_init (struct list*);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_elem* elem);  
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(struct list* plist);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_elem* obj_elem);

# endif
