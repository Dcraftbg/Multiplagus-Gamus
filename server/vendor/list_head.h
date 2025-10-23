#pragma once
// Copyright (c) 2025 Dcraftbg
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#include <stddef.h>
#include <stdbool.h>
// NOTE: This implementation is a slightly modified version of that of the linux kernel itself.
// https://github.com/torvalds/linux/blob/b1bc554e009e3aeed7e4cfd2e717c7a34a98c683/tools/firewire/list.h
//
// Things like list_insert, list_append are all literally the exact same, with the exception 
// of list_remove which also closes in the array element removed since the linux version didn't clean it up
struct list_head {
    struct list_head *next, *prev;
};
static void list_init(struct list_head* list) {
    list->next = list;
    list->prev = list;
}
static struct list_head* list_last(struct list_head* list) {
    return list->prev != list ? list->prev : NULL;
}

static struct list_head* list_next(struct list_head* list) {
    return list->next != list ? list->next : NULL;
}
static inline void list_insert(struct list_head *link, struct list_head *new) {
    new->prev = link->prev;
    new->next = link;
    new->prev->next = new;
    new->next->prev = new;
}
static void list_append(struct list_head *into, struct list_head *new) {
    list_insert(into->next, new);
}
static void list_remove(struct list_head *list) {
    list->prev->next = list->next;
    list->next->prev = list->prev;
    list->next = list;
    list->prev = list;
}
static void list_move(struct list_head *to, struct list_head *what) {
    to->next = what->next;
    to->prev = what->prev;
    to->prev->next = to;
    to->next->prev = to;
    what->next = what;
    what->prev = what;
}
static inline bool list_empty(struct list_head *list) {
    return list->next == list;
}
static size_t list_len(struct list_head *list) {
    size_t n = 0;
    struct list_head* first = list;
    list = list->next;
    while(first != list) {
        n++;
        list = list->next;
    }
    return n;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define list_foreach(head, list) for(struct list_head* head = (list)->next; head != (list); head = head->next)
