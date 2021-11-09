#pragma once
struct vm_gc_t;
struct vm_gc_entry_t;
struct vm_gc_obj_t;
typedef struct vm_gc_t vm_gc_t;
typedef struct vm_gc_entry_t vm_gc_entry_t;
typedef struct vm_gc_obj_t vm_gc_obj_t;

#include "libc.h"
#include "obj.h"
#include "vm.h"

void vm_gc_start(vm_gc_t *out);
void vm_gc_stop(vm_gc_t *gc);

int vm_gc_type(vm_gc_entry_t *ent);

vm_gc_entry_t *vm_gc_array_new(vm_gc_t *gc, size_t len);
vm_gc_entry_t *vm_gc_string_new(vm_gc_t *gc, size_t len);

void vm_gc_extend(vm_gc_entry_t *to, vm_gc_entry_t *from);
int vm_gc_sizeof(vm_gc_entry_t *ptr);
vm_obj_t vm_gc_get_index(vm_gc_entry_t *ptr, int index);
void vm_gc_set_index(vm_gc_entry_t *ptr, int index, vm_obj_t value);
vm_obj_t vm_gc_concat(vm_gc_t *gc, vm_obj_t lhs, vm_obj_t rhs);

struct vm_gc_t
{
    vm_gc_entry_t *first;
    size_t remain;
    vm_obj_t *low;
    vm_obj_t *high;
};

struct vm_gc_entry_t
{
    vm_gc_entry_t *next;
    bool keep: 1;
    uint32_t type: 31;
    uint32_t data;
};

typedef struct
{
    vm_gc_entry_t *next;
    bool keep: 1;
    uint32_t type: 31;
    uint32_t len;
    uint32_t alloc;
    vm_obj_t *obj;
} vm_gc_entry_array_t;

typedef struct
{
    vm_gc_entry_t *next;
    bool keep: 1;
    uint32_t type: 31;
    uint32_t len;
    uint8_t obj[0];
} vm_gc_entry_string_t;