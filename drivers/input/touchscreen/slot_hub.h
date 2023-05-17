#ifndef _SLOT_HUB_H_
#define _SLOT_HUB_H_

#include <linux/input.h>
#include <linux/list.h>

typedef struct {
    void *identity;
    int slot_original;
    int slot_unite;
    int slot_source;
    int tp_status;
    struct list_head identity_head;
    struct list_head algorithm_head;
} _information_t, *information_t;

typedef struct {
    void *identity;
    struct list_head slot_head;
    struct list_head list;
} _identity_node_t, *identity_node_t;

typedef struct {
    int slot_original;
    int slot_unite;
    int slot_source;
    struct list_head list;
} _slot_node_t, *slot_node_t;

#define TOUCH_DOWN_T 1
#define TOUCH_UP_T 2
#define ERRVEL -1

#define VIRTUAL_MOVE		0x02
#define VIRTUAL_DOWN		0x01
#define VIRTUAL_UP			0x00

#endif