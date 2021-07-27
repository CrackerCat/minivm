#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct vec_s;

typedef struct vec_s *vec_t;

struct vec_s
{
  int length;
  int allocated;
  int size;
  char values[0];
};

static vec_t vec_new(int elem_size)
{
  int allocated = 4;
  vec_t ret = malloc(sizeof(struct vec_s) + elem_size * allocated);
  ret->length = 0;
  ret->size = elem_size;
  ret->allocated = elem_size * allocated;
  return ret;
}

static void vec_resize(vec_t *vecp, int ngrow)
{
  if ((*vecp)->length + (*vecp)->size * ngrow >= (*vecp)->allocated)
  {
    int nallocated = ((*vecp)->allocated + ngrow) * 4;
    *vecp = realloc(*vecp, nallocated);
    (*vecp)->allocated = nallocated;
  }
}

#define vec_new(t) vec_new(sizeof(t))
#define vec_push(vec, value)                                     \
  (                                                              \
      {                                                          \
        vec_resize(&vec, 1);                                     \
        *(typeof(value) *)&(vec)->values[(vec)->length] = value; \
        (vec)->length += (vec)->size;                            \
      })

#define vec_pop(vec) ((void))(vec)->length-=(vec)->size)
#define vec_del(vec) ((void)free(vec))
#define vec_get(vec, index) ((vec)->values + index * (vec)->size)
#define vec_foreach(item, vec)                        \
  for (                                               \
      void *item = (void *)(vec)->values;             \
      item < (void *)((vec)->values + (vec)->length); \
      item += (vec)->size)
