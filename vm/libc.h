#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VM_FRAMES_UNITS (1 << 16)
#define VM_LOCALS_UNITS (VM_FRAMES_UNITS * 16)

#if !defined(NULL)
#define NULL ((void*)0)
#endif

int putchar(int chr);
double fmod(double a, double b);

#define vm_putchar(chr) (putchar(chr))
#define vm_fmod(lhs, rhs) (fmod(lhs, rhs))

#if defined(VM_USE_MIMALLOC)
void *mi_malloc(size_t size);
void *mi_calloc(size_t n, size_t size);
void mi_free(void *ptr);
#define vm_malloc(size) (mi_malloc((size)))
#define vm_calloc(size) (mi_calloc((1),(size)))
#define vm_free(ptr) (mi_free((ptr)))
#else
void *malloc(size_t size);
void *calloc(size_t n, size_t size);
void free(void *ptr);
#define vm_malloc(size) (malloc((size)))
#define vm_calloc(size) (calloc((1),(size)))
#define vm_free(ptr) (free((ptr)))
#endif