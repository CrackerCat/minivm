#pragma once

#include "obj.h"
#include "gc.h"

vm_obj_t vm_sys_call(void *sys, vm_obj_t arg);
void vm_sys_mark(void *sys);
void *vm_sys_init(vm_gc_t *gc);
void vm_sys_deinit(void *obj);
