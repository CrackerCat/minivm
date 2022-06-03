#pragma once

#if !defined(VM_GC_MIN)
#define VM_GC_MIN (1 << 16)
#endif

#if !defined(VM_CONFIG_BIGINT)
#define VM_CONFIG_BIGINT 0
#endif

#if !defined(VM_CONFIG_NUM_FRAMES)
#define VM_CONFIG_NUM_FRAMES 1000
#endif

#if !defined(VM_CONFIG_NUM_REGS)
#define VM_CONFIG_NUM_REGS (VM_CONFIG_NUM_FRAMES * 16)
#endif

#if !defined(VM_GROW_STACK)
#define VM_GROW_STACK (0)
#endif

#if !defined(VM_CONFIG_GC)
#define VM_CONFIG_GC 0
#endif
