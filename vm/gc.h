#pragma once
struct vm_gc_t;
struct vm_gc_entry_t;
struct vm_gc_obj_t;
typedef struct vm_gc_t vm_gc_t;
typedef struct vm_gc_entry_t vm_gc_entry_t;
typedef struct vm_gc_obj_t vm_gc_obj_t;

#include "obj.h"
#include "libc.h"

struct vm_gc_t
{
    vm_gc_entry_t *first;
    int64_t remain;
};

enum {
    VM_GC_ENTRY_TYPE_ARRAY,
    VM_GC_ENTRY_TYPE_STATIC_ARRAY,
};

struct vm_gc_entry_t
{
    vm_gc_entry_t *next;
    uint32_t keep: 1;
    uint32_t alloc: 31;
    uint32_t len;
    vm_obj_t arr[0];
};

#include "vm.h"

void vm_gc_start(vm_gc_t *out);
void vm_gc_stop(vm_gc_t *gc);
void vm_gc_run1(vm_gc_t *gc, vm_obj_t *low, vm_obj_t *high);

vm_int_t vm_gc_type(vm_gc_entry_t *ent);

vm_obj_t vm_gc_dup(vm_gc_t *gc, vm_obj_t obj);

vm_gc_entry_t *vm_gc_static_array_new(vm_gc_t *gc, size_t len);

vm_int_t vm_gc_sizeof(vm_gc_entry_t *ptr);
vm_obj_t vm_gc_get_index(vm_gc_entry_t *ptr, vm_int_t index);
void vm_gc_set_index(vm_gc_entry_t *ptr, vm_int_t index, vm_obj_t value);
vm_obj_t vm_gc_static_concat(vm_gc_t *gc, vm_obj_t lhs, vm_obj_t rhs);
