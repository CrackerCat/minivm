#include "vm.h"
#include "gc.h"
#include "math.h"
#include "obj.h"
#include "libc.h"
#include "effect.h"
#include "obj/map.h"
#include "sys/sys.h"
#include "state.h"

#define VM_GLOBALS_NUM (1024)

#if defined(VM_DEBUG_OPCODE)
#define run_next_op                                                   \
    printf("(%i -> %i)\n", (int)cur_index, (int)basefunc[cur_index]); \
    goto *next_op;
#else
#define run_next_op \
    goto *next_op;
#endif

typedef struct
{
    vm_obj_t handler;
    int depth;
    int max;
} find_handler_pair_t;

static inline int find_handler_worker(void *state, vm_obj_t key, vm_obj_t val)
{
    find_handler_pair_t *pair = state;
    int ikey = vm_obj_to_int(key);
    if (ikey > pair->max)
    {
        return 0;
    }
    if (ikey >= pair->depth)
    {
        pair->handler = val;
        pair->depth = ikey;
    }
    return 0;
}

static inline find_handler_pair_t find_handler1(vm_gc_entry_t *handlers, vm_obj_t effect, size_t max_level)
{
    vm_obj_t res1 = vm_gc_get_index(handlers, effect);
    if (vm_obj_is_dead(res1))
    {
        return (find_handler_pair_t){
            .depth = -1,
        };
    }
    vm_gc_entry_map_t *map = vm_obj_to_ptr(res1);
    find_handler_pair_t pair = (find_handler_pair_t){
        .max = max_level,
        .depth = -1,
    };
    vm_map_for_pairs(map->map, &pair, &find_handler_worker);
    return pair;
}

static inline find_handler_pair_t find_handler(vm_gc_entry_t *handlers, vm_obj_t effect, size_t max_level)
{
    find_handler_pair_t pair1 = find_handler1(handlers, effect, max_level);
    if (pair1.depth >= 0)
    {
        return pair1;
    }
    find_handler_pair_t pair2 = find_handler1(handlers, vm_obj_of_int(VM_EFFECT_DEFAULT), max_level);
    if (pair2.depth >= 0)
    {
        return pair2;
    }
    return (find_handler_pair_t){
        .depth = -1,
    };
}

#define run_next_op_after_effect(outreg_, effect)                                                \
    ({                                                                                           \
        vm_reg_t copy_outreg = outreg_;                                                          \
        vm_obj_t copy_effect = effect;                                                           \
        vm_obj_t *next_locals = cur_frame->locals;                                               \
        find_handler_pair_t pair = find_handler(handlers, copy_effect, cur_frame - frames_base); \
        if (pair.depth == -1) {\
            for (const char *src = "error: handler"; *src != '\0'; src += 1)\
            {\
                state->putchar(state, *src);\
            }\
            goto do_exit;\
        }\
        vm_obj_t funcv = pair.handler;                                                           \
        int level = pair.depth;                                                                  \
        for (int i = 0; vm_obj_is_ptr(funcv); i++)                                               \
        {                                                                                        \
            next_locals[i] = funcv;                                                              \
            funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));                     \
        }                                                                                        \
        vm_loc_t next_func = vm_obj_to_fun(funcv);                                               \
        cur_locals = next_locals;                                                                \
        cur_frame->index = cur_index;                                                            \
        cur_frame->outreg = copy_outreg;                                                         \
        cur_frame++;                                                                             \
        cur_index = next_func;                                                                   \
        cur_frame->locals = cur_locals + get_word(-1);                                           \
        cur_effect->resume = cur_frame - frames_base;                                            \
        cur_effect->exit = level;                                                                \
        cur_effect++;                                                                            \
        vm_fetch;                                                                                \
    });                                                                                          \
    run_next_op;

#define gc_new(TYPE, ...) ({                            \
    vm_gc_##TYPE##_new(__VA_ARGS__);                    \
})

#define cur_bytecode_next(Type)                              \
    (                                                        \
        {                                                    \
            Type ret = *(Type *)&basefunc[cur_index];        \
            cur_index += sizeof(Type) / sizeof(vm_opcode_t); \
            ret;                                             \
        })

#define next_op (cur_index += 1, next_op_value)
#define vm_fetch (next_op_value = ptrs[basefunc[cur_index]])

#define get_word(index) (*(uint32_t *)&basefunc[(cur_index) + (index)])
#define read_word (cur_bytecode_next(uint32_t))
#define read_reg (cur_bytecode_next(uint32_t))
#define read_int (cur_bytecode_next(int32_t))
#define read_loc (cur_bytecode_next(uint32_t))

void vm_run(vm_state_t *state, size_t len, const vm_opcode_t *basefunc)
{
    vm_loc_t cur_index = 0;

    vm_stack_frame_t *frames_base = vm_calloc(VM_FRAMES_UNITS * sizeof(vm_stack_frame_t));
    vm_obj_t *locals_base = vm_calloc(VM_LOCALS_UNITS * sizeof(vm_obj_t));
    vm_handler_frame_t *effects_base = vm_calloc((1 << 12) * sizeof(vm_handler_frame_t));
  
    vm_gc_t *gc = state->gc;
    gc->low = locals_base;
    gc->high = locals_base + VM_LOCALS_UNITS;

    vm_stack_frame_t *cur_frame = frames_base;
    vm_obj_t *cur_locals = locals_base;
    vm_handler_frame_t *cur_effect = effects_base;


    vm_gc_entry_t *handlers = gc_new(map, gc);
    cur_locals[0] = vm_obj_of_ptr(handlers);
    cur_locals += 1;
    cur_locals[0] = vm_obj_of_ptr(state->global);

    void *next_op_value;
    void *ptrs[VM_OPCODE_MAX2P] = {};
    ptrs[VM_OPCODE_EXIT] = &&do_exit;
    ptrs[VM_OPCODE_STORE_REG] = &&do_store_reg;
    ptrs[VM_OPCODE_STORE_NONE] = &&do_store_none;
    ptrs[VM_OPCODE_STORE_BOOL] = &&do_store_bool;
    ptrs[VM_OPCODE_STORE_INT] = &&do_store_int;
    ptrs[VM_OPCODE_STORE_FUN] = &&do_store_fun;
    ptrs[VM_OPCODE_EQUAL] = &&do_equal;
    ptrs[VM_OPCODE_EQUAL_NUM] = &&do_equal_num;
    ptrs[VM_OPCODE_NOT_EQUAL] = &&do_not_equal;
    ptrs[VM_OPCODE_NOT_EQUAL_NUM] = &&do_not_equal_num;
    ptrs[VM_OPCODE_LESS] = &&do_less;
    ptrs[VM_OPCODE_LESS_NUM] = &&do_less_num;
    ptrs[VM_OPCODE_GREATER] = &&do_greater;
    ptrs[VM_OPCODE_GREATER_NUM] = &&do_greater_num;
    ptrs[VM_OPCODE_LESS_THAN_EQUAL] = &&do_less_than_equal;
    ptrs[VM_OPCODE_LESS_THAN_EQUAL_NUM] = &&do_less_than_equal_num;
    ptrs[VM_OPCODE_GREATER_THAN_EQUAL] = &&do_greater_than_equal;
    ptrs[VM_OPCODE_GREATER_THAN_EQUAL_NUM] = &&do_greater_than_equal_num;
    ptrs[VM_OPCODE_JUMP] = &&do_jump;
    ptrs[VM_OPCODE_BRANCH_FALSE] = &&do_branch_false;
    ptrs[VM_OPCODE_BRANCH_TRUE] = &&do_branch_true;
    ptrs[VM_OPCODE_BRANCH_EQUAL] = &&do_branch_equal;
    ptrs[VM_OPCODE_BRANCH_EQUAL_NUM] = &&do_branch_equal_num;
    ptrs[VM_OPCODE_BRANCH_NOT_EQUAL] = &&do_branch_not_equal;
    ptrs[VM_OPCODE_BRANCH_NOT_EQUAL_NUM] = &&do_branch_not_equal_num;
    ptrs[VM_OPCODE_BRANCH_LESS] = &&do_branch_less;
    ptrs[VM_OPCODE_BRANCH_LESS_NUM] = &&do_branch_less_num;
    ptrs[VM_OPCODE_BRANCH_GREATER] = &&do_branch_greater;
    ptrs[VM_OPCODE_BRANCH_GREATER_NUM] = &&do_branch_greater_num;
    ptrs[VM_OPCODE_BRANCH_LESS_THAN_EQUAL] = &&do_branch_less_than_equal;
    ptrs[VM_OPCODE_BRANCH_LESS_THAN_EQUAL_NUM] = &&do_branch_less_than_equal_num;
    ptrs[VM_OPCODE_BRANCH_GREATER_THAN_EQUAL] = &&do_branch_greater_than_equal;
    ptrs[VM_OPCODE_BRANCH_GREATER_THAN_EQUAL_NUM] = &&do_branch_greater_than_equal_num;
    ptrs[VM_OPCODE_INC] = &&do_inc;
    ptrs[VM_OPCODE_INC_NUM] = &&do_inc_num;
    ptrs[VM_OPCODE_DEC] = &&do_dec;
    ptrs[VM_OPCODE_DEC_NUM] = &&do_dec_num;
    ptrs[VM_OPCODE_ADD] = &&do_add;
    ptrs[VM_OPCODE_ADD_NUM] = &&do_add_num;
    ptrs[VM_OPCODE_SUB] = &&do_sub;
    ptrs[VM_OPCODE_SUB_NUM] = &&do_sub_num;
    ptrs[VM_OPCODE_MUL] = &&do_mul;
    ptrs[VM_OPCODE_MUL_NUM] = &&do_mul_num;
    ptrs[VM_OPCODE_DIV] = &&do_div;
    ptrs[VM_OPCODE_DIV_NUM] = &&do_div_num;
    ptrs[VM_OPCODE_MOD] = &&do_mod;
    ptrs[VM_OPCODE_MOD_NUM] = &&do_mod_num;
    ptrs[VM_OPCODE_CONCAT] = &&do_concat;
    ptrs[VM_OPCODE_STATIC_CALL0] = &&do_static_call0;
    ptrs[VM_OPCODE_STATIC_CALL1] = &&do_static_call1;
    ptrs[VM_OPCODE_STATIC_CALL2] = &&do_static_call2;
    ptrs[VM_OPCODE_STATIC_CALL] = &&do_static_call;
    ptrs[VM_OPCODE_TAIL_CALL0] = &&do_tail_call0;
    ptrs[VM_OPCODE_TAIL_CALL1] = &&do_tail_call1;
    ptrs[VM_OPCODE_TAIL_CALL2] = &&do_tail_call2;
    ptrs[VM_OPCODE_TAIL_CALL] = &&do_tail_call;
    ptrs[VM_OPCODE_CALL0] = &&do_call0;
    ptrs[VM_OPCODE_CALL1] = &&do_call1;
    ptrs[VM_OPCODE_CALL2] = &&do_call2;
    ptrs[VM_OPCODE_CALL] = &&do_call;
    ptrs[VM_OPCODE_RETURN] = &&do_return;
    ptrs[VM_OPCODE_PUTCHAR] = &&do_putchar;
    ptrs[VM_OPCODE_REF_NEW] = &&do_ref_new;
    ptrs[VM_OPCODE_BOX_NEW] = &&do_box_new;
    ptrs[VM_OPCODE_STRING_NEW] = &&do_string_new;
    ptrs[VM_OPCODE_ARRAY_NEW] = &&do_array_new;
    ptrs[VM_OPCODE_MAP_NEW] = &&do_map_new;
    ptrs[VM_OPCODE_REF_GET] = &&do_ref_get;
    ptrs[VM_OPCODE_BOX_GET] = &&do_get_box;
    ptrs[VM_OPCODE_BOX_SET] = &&do_set_box;
    ptrs[VM_OPCODE_LENGTH] = &&do_length;
    ptrs[VM_OPCODE_INDEX_GET] = &&do_index_get;
    ptrs[VM_OPCODE_INDEX_SET] = &&do_index_set;
    ptrs[VM_OPCODE_SET_HANDLER] = &&do_set_handler;
    ptrs[VM_OPCODE_CALL_HANDLER] = &&do_call_handler;
    ptrs[VM_OPCODE_RETURN_HANDLER] = &&do_return_handler;
    ptrs[VM_OPCODE_EXIT_HANDLER] = &&do_exit_handler;
    ptrs[VM_OPCODE_EXEC] = &&do_exec;
    ptrs[VM_OPCODE_TYPE] = &&do_type;
    cur_frame->locals = cur_locals;
    cur_frame += 1;
    cur_frame->locals = cur_locals + VM_GLOBALS_NUM;
    vm_fetch;
    run_next_op;
do_exit:
{
    state->gc->low = NULL;
    state->gc->high = NULL;
    vm_free(frames_base);
    vm_free(locals_base);
    vm_free(effects_base);
    return;
}
do_set_handler:
{
    vm_reg_t sym = read_reg;
    vm_reg_t handler = read_reg;
    vm_fetch;
    vm_obj_t xmap = vm_gc_get_index(handlers, cur_locals[sym]);
    if (vm_obj_is_dead(xmap))
    {
        xmap = vm_obj_of_ptr(gc_new(map, gc));
        vm_gc_set_index(handlers, cur_locals[sym], xmap);
    }
    vm_obj_t my_frame = vm_obj_of_int(cur_frame - frames_base);
    vm_gc_set_index(vm_obj_to_ptr(xmap), my_frame, cur_locals[handler]);
    run_next_op;
}
do_call_handler:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t effect = read_reg;
    run_next_op_after_effect(outreg, cur_locals[effect]);
}
do_return_handler:
{
    vm_reg_t from = read_reg;
    cur_effect--;
    cur_frame = frames_base + cur_effect->resume;
    vm_obj_t val = cur_locals[from];
    cur_frame--;
    cur_locals = (cur_frame - 1)->locals;
    cur_index = cur_frame->index;
    vm_reg_t outreg = cur_frame->outreg;
    cur_locals[outreg] = val;
    vm_fetch;
    run_next_op;
}
do_exit_handler:
{
    vm_reg_t from = read_reg;
    cur_effect--;
    cur_frame = 1 + frames_base + cur_effect->exit;
    vm_obj_t val = cur_locals[from];
    cur_frame--;
    cur_locals = (cur_frame - 1)->locals;
    cur_index = cur_frame->index;
    vm_reg_t outreg = cur_frame->outreg;
    cur_locals[outreg] = val;
    vm_fetch;
    run_next_op;
}
do_exec:
{
    vm_reg_t in = read_reg;
    vm_reg_t argreg = read_reg;
    vm_gc_entry_t *ent = vm_obj_to_ptr(cur_locals[in]);
    int len = vm_obj_to_int(vm_gc_sizeof(ent));
    vm_opcode_t *xops = vm_malloc(sizeof(vm_opcode_t) * len);
    for (int i = 0; i < len; i++)
    {
        vm_obj_t obj = vm_gc_get_index(ent, vm_obj_of_int(i));
        double n = vm_obj_to_num(obj);
        xops[i] = (vm_opcode_t) n;
    }
    vm_gc_entry_t *vargs = vm_obj_to_ptr(cur_locals[argreg]);
    int nargs = vm_obj_to_int(vm_gc_sizeof(vargs));
    char **args = vm_malloc(sizeof(const char *) * nargs);
    for (int i = 0; i < nargs; i++)
    {
        vm_obj_t obj = vm_gc_get_index(vargs, vm_obj_of_int(i));
        vm_gc_entry_t *arg = vm_obj_to_ptr(obj);
        int alen = vm_obj_to_int(vm_gc_sizeof(arg));
        args[i] = vm_malloc(sizeof(char) * (alen + 1));
        for (int j = 0; j < alen; j++)
        {
            args[i][j] = vm_obj_to_int(vm_gc_get_index(arg, vm_obj_of_int(j)));
        }
        args[i][alen] = '\0';
    }
    vm_state_t *newstate = vm_state_new(nargs, (const char**) args);
    vm_run(newstate, len, xops);
    vm_state_del(newstate);
    for (int i = 0; i < nargs; i++) {
        vm_free(args[i]);
    }
    vm_free(args);
    vm_free(xops);
    vm_fetch;
    run_next_op;
}
do_return:
{
    vm_reg_t from = read_reg;
    vm_obj_t val = cur_locals[from];
    cur_frame--;
    cur_locals = (cur_frame - 1)->locals;
    cur_index = cur_frame->index;
    vm_reg_t outreg = cur_frame->outreg;
    cur_locals[outreg] = val;
    vm_fetch;
    run_next_op;
}
do_type:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t valreg = read_reg;
    vm_fetch;
    vm_obj_t obj = cur_locals[valreg];
    double num = -1;
    if (vm_obj_is_none(obj))
    {
        num = VM_TYPE_NONE;
    }
    if (vm_obj_is_bool(obj))
    {
        num = VM_TYPE_BOOL;
    }
    if (vm_obj_is_num(obj))
    {
        num = VM_TYPE_NUMBER;
    }
    if (vm_obj_is_fun(obj))
    {
        num = VM_TYPE_FUNCTION;
    }
    if (vm_obj_is_ptr(obj))
    {
        num = vm_gc_type(vm_obj_to_ptr(obj));
    }
    cur_locals[outreg] = vm_obj_of_num(num);
    run_next_op;
}
do_string_new:
{
    vm_reg_t outreg = read_reg;
    int nargs = read_word;
    vm_gc_entry_t *str = gc_new(string, gc, nargs);
    for (size_t i = 0; i < nargs; i++)
    {
        vm_gc_set_index(str, vm_obj_of_int(i), vm_obj_of_int(read_word));
    }
    vm_fetch;
    cur_locals[outreg] = vm_obj_of_ptr(str);
    run_next_op;
}
do_array_new:
{
    vm_reg_t outreg = read_reg;
    int nargs = read_word;
    vm_gc_entry_t *vec = gc_new(array, gc, nargs);
    for (int i = 0; i < nargs; i++)
    {
        vm_reg_t vreg = read_reg;
        vm_gc_set_index(vec, vm_obj_of_int(i), cur_locals[vreg]);
    }
    vm_fetch;
    cur_locals[outreg] = vm_obj_of_ptr(vec);
    run_next_op;
}
do_ref_new:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t inreg = read_reg;
    vm_fetch;
    vm_gc_entry_t *box = gc_new(ref, gc, &cur_locals[inreg]);
    cur_locals[outreg] = vm_obj_of_ptr(box);
    run_next_op;
}
do_box_new:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t inreg = read_reg;
    vm_fetch;
    vm_gc_entry_t *box = gc_new(box, gc);
    vm_gc_set_box(box, cur_locals[inreg]);
    cur_locals[outreg] = vm_obj_of_ptr(box);
    run_next_op;
}
do_map_new:
{
    vm_reg_t outreg = read_reg;
    vm_fetch;
    vm_gc_entry_t *map = gc_new(map, gc);
    cur_locals[outreg] = vm_obj_of_ptr(map);
    run_next_op;
}
do_ref_get:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t inreg = read_reg;
    vm_fetch;
    vm_obj_t obj = cur_locals[inreg];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(obj))
    {
        run_next_op_after_effect(outreg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    vm_gc_entry_t *ref = vm_obj_to_ptr(cur_locals[inreg]);
    vm_obj_t *val = vm_gc_get_ref(ref);
    cur_locals[outreg] = *val;
    run_next_op;
}
do_set_box:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t inreg = read_reg;
    vm_fetch;
    vm_obj_t obj = cur_locals[outreg];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(obj))
    {
        run_next_op_after_effect(outreg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    vm_gc_entry_t *box = vm_obj_to_ptr(obj);
    vm_gc_set_box(box, cur_locals[inreg]);
    run_next_op;
}
do_get_box:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t inreg = read_reg;
    vm_fetch;
    vm_obj_t obj = cur_locals[inreg];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(obj))
    {
        run_next_op_after_effect(outreg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    vm_gc_entry_t *box = vm_obj_to_ptr(cur_locals[inreg]);
    cur_locals[outreg] = vm_gc_get_box(box);
    run_next_op;
}
do_length:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t reg = read_reg;
    vm_fetch;
    vm_obj_t vec = cur_locals[reg];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(vec))
    {
        run_next_op_after_effect(outreg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    cur_locals[outreg] = vm_gc_sizeof(vm_obj_to_ptr(vec));
    run_next_op;
}
do_index_get:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t reg = read_reg;
    vm_reg_t ind = read_reg;
    vm_fetch;
    vm_obj_t vec = cur_locals[reg];
    vm_obj_t index = cur_locals[ind];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(vec))
    {
        run_next_op_after_effect(outreg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    cur_locals[outreg] = vm_gc_get_index(vm_obj_to_ptr(vec), index);
    if (vm_obj_is_dead(cur_locals[outreg]))
    {
        run_next_op_after_effect(outreg, vm_obj_of_num(VM_EFFECT_BOUNDS));
    }
    run_next_op;
}
do_index_set:
{
    vm_reg_t reg = read_reg;
    vm_reg_t ind = read_reg;
    vm_reg_t val = read_reg;
    vm_fetch;
    vm_obj_t vec = cur_locals[reg];
    vm_obj_t index = cur_locals[ind];
    vm_obj_t value = cur_locals[val];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(vec))
    {
        run_next_op_after_effect(reg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    vm_obj_t res = vm_gc_set_index(vm_obj_to_ptr(vec), index, value);
#if defined(VM_USE_TYPES)
    if (vm_obj_is_dead(res))
    {
        run_next_op_after_effect(reg, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    run_next_op;
}
do_tail_call0:
{
    vm_reg_t func = read_reg;
    vm_obj_t funcv = cur_locals[func];
    cur_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        cur_frame--;
        cur_locals = (cur_frame - 1)->locals;
        cur_index = cur_frame->index;
        vm_reg_t outreg = cur_frame->outreg;
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_index = next_func;
    vm_fetch;
    run_next_op;
}
do_tail_call1:
{
    vm_reg_t func = read_reg;
    cur_locals[1] = cur_locals[read_reg];
    vm_obj_t funcv = cur_locals[func];
    cur_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        cur_frame--;
        cur_locals = (cur_frame - 1)->locals;
        cur_index = cur_frame->index;
        vm_reg_t outreg = cur_frame->outreg;
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_index = next_func;
    vm_fetch;
    run_next_op;
}
do_tail_call2:
{
    vm_reg_t func = read_reg;
    cur_locals[1] = cur_locals[read_reg];
    cur_locals[2] = cur_locals[read_reg];
    vm_obj_t funcv = cur_locals[func];
    cur_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        cur_frame--;
        cur_locals = (cur_frame - 1)->locals;
        cur_index = cur_frame->index;
        vm_reg_t outreg = cur_frame->outreg;
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_index = next_func;
    vm_fetch;
    run_next_op;
}
do_tail_call:
{
    vm_reg_t func = read_reg;
    int nargs = read_word;
    vm_obj_t *next_locals = cur_frame->locals;
    for (int argno = 1; argno <= nargs; argno++)
    {
        vm_reg_t regno = read_reg;
        next_locals[argno] = cur_locals[regno];
    }
    vm_obj_t funcv = cur_locals[func];
    cur_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        cur_frame--;
        cur_locals = (cur_frame - 1)->locals;
        cur_index = cur_frame->index;
        vm_reg_t outreg = cur_frame->outreg;
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_index = next_func;
    vm_fetch;
    run_next_op;
}
do_static_call0:
{
    vm_reg_t outreg = read_reg;
    vm_loc_t next_func = read_loc;
    vm_obj_t *next_locals = cur_frame->locals;
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_static_call1:
{
    vm_reg_t outreg = read_reg;
    vm_loc_t next_func = read_loc;
    vm_obj_t *next_locals = cur_frame->locals;
    next_locals[1] = cur_locals[read_reg];
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_static_call2:
{
    vm_reg_t outreg = read_reg;
    vm_loc_t next_func = read_loc;
    vm_obj_t *next_locals = cur_frame->locals;
    next_locals[1] = cur_locals[read_reg];
    next_locals[2] = cur_locals[read_reg];
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_static_call:
{
    vm_reg_t outreg = read_reg;
    vm_loc_t next_func = read_loc;
    int nargs = read_word;
    vm_obj_t *next_locals = cur_frame->locals;
    for (int argno = 1; argno <= nargs; argno++)
    {
        vm_reg_t regno = read_reg;
        next_locals[argno] = cur_locals[regno];
    }
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_call0:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t func = read_reg;
    vm_obj_t *next_locals = cur_frame->locals;
    vm_obj_t funcv = cur_locals[func];
    next_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_call1:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t func = read_reg;
    vm_obj_t *next_locals = cur_frame->locals;
    next_locals[1] = cur_locals[read_reg];
    vm_obj_t funcv = cur_locals[func];
    next_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_call2:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t func = read_reg;
    vm_obj_t *next_locals = cur_frame->locals;
    next_locals[1] = cur_locals[read_reg];
    next_locals[2] = cur_locals[read_reg];
    vm_obj_t funcv = cur_locals[func];
    next_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_call:
{
    vm_reg_t outreg = read_reg;
    vm_reg_t func = read_reg;
    int nargs = read_word;
    vm_obj_t *next_locals = cur_frame->locals;
    for (int argno = 1; argno <= nargs; argno++)
    {
        vm_reg_t regno = read_reg;
        next_locals[argno] = cur_locals[regno];
    }
    vm_obj_t funcv = cur_locals[func];
    next_locals[0] = funcv;
    if (vm_obj_is_ptr(funcv))
    {
        funcv = vm_gc_get_index(vm_obj_to_ptr(funcv), vm_obj_of_int(0));
    }
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_fun(funcv))
    {
        run_next_op_after_effect(outreg, vm_obj_of_int(VM_EFFECT_TYPE));
    }
#endif
    vm_loc_t next_func = vm_obj_to_fun(funcv);
    cur_locals = next_locals;
    cur_frame->index = cur_index;
    cur_frame->outreg = outreg;
    cur_frame++;
    cur_index = next_func;
    cur_frame->locals = cur_locals + get_word(-1);
    vm_fetch;
    run_next_op;
}
do_store_none:
{
    vm_reg_t to = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_none();
    run_next_op;
}
do_store_bool:
{
    vm_reg_t to = read_reg;
    int from = (int)read_word;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool((bool)from);
    run_next_op;
}
do_store_reg:
{
    vm_reg_t to = read_reg;
    vm_reg_t from = read_reg;
    vm_fetch;
    cur_locals[to] = cur_locals[from];
    run_next_op;
}
do_store_int:
{
    vm_reg_t to = read_reg;
    int from = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_int(from);
    run_next_op;
}
do_store_fun:
{
    vm_reg_t to = read_reg;
    vm_loc_t func_end = read_loc;
    vm_loc_t head = cur_index;
    cur_index = func_end;
    vm_fetch;
    cur_locals[to] = vm_obj_of_fun(head + 1);
    run_next_op;
}
do_equal:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_eq(cur_locals[lhs], cur_locals[rhs]));
    run_next_op;
}
do_equal_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_ieq(cur_locals[lhs], rhs));
    run_next_op;
}
do_not_equal:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_neq(cur_locals[lhs], cur_locals[rhs]));
    run_next_op;
}
do_not_equal_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_ineq(cur_locals[lhs], rhs));
    run_next_op;
}
do_less:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_lt(cur_locals[lhs], cur_locals[rhs]));
    run_next_op;
}
do_less_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_ilt(cur_locals[lhs], rhs));
    run_next_op;
}
do_greater:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_gt(cur_locals[lhs], cur_locals[rhs]));
    run_next_op;
}
do_greater_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_igt(cur_locals[lhs], rhs));
    run_next_op;
}
do_less_than_equal:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_lte(cur_locals[lhs], cur_locals[rhs]));
    run_next_op;
}
do_less_than_equal_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_ilte(cur_locals[lhs], rhs));
    run_next_op;
}
do_greater_than_equal:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_gte(cur_locals[lhs], cur_locals[rhs]));
    run_next_op;
}
do_greater_than_equal_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_of_bool(vm_obj_igte(cur_locals[lhs], rhs));
    run_next_op;
}
do_jump:
{
    vm_loc_t to = read_loc;
    cur_index = to;
    vm_fetch;
    run_next_op;
}
do_branch_false:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t from = read_reg;
    if (!vm_obj_to_bool(cur_locals[from]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_true:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t from = read_reg;
    if (vm_obj_to_bool(cur_locals[from]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_equal:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    if (vm_obj_eq(cur_locals[lhs], cur_locals[rhs]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_equal_num:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    if (vm_obj_ieq(cur_locals[lhs], rhs))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_not_equal:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    if (vm_obj_neq(cur_locals[lhs], cur_locals[rhs]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_not_equal_num:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    if (vm_obj_ineq(cur_locals[lhs], rhs))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_less:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    if (vm_obj_lt(cur_locals[lhs], cur_locals[rhs]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_less_num:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    if (vm_obj_ilt(cur_locals[lhs], rhs))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_less_than_equal:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    if (vm_obj_lte(cur_locals[lhs], cur_locals[rhs]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_less_than_equal_num:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    if (vm_obj_ilte(cur_locals[lhs], rhs))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_greater:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    if (vm_obj_gt(cur_locals[lhs], cur_locals[rhs]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_greater_num:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    if (vm_obj_igt(cur_locals[lhs], rhs))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_greater_than_equal:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    if (vm_obj_gte(cur_locals[lhs], cur_locals[rhs]))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_branch_greater_than_equal_num:
{
    vm_loc_t to1 = read_loc;
    vm_loc_t to2 = read_loc;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    if (vm_obj_igte(cur_locals[lhs], rhs))
    {
        cur_index = to1;
    }
    else
    {
        cur_index = to2;
    }
    vm_fetch;
    run_next_op;
}
do_inc:
{
    vm_reg_t target = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[target] = vm_obj_num_add(cur_locals[target], cur_locals[rhs]);
    run_next_op;
}
do_inc_num:
{
    vm_reg_t target = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[target] = vm_obj_num_addc(cur_locals[target], rhs);
    run_next_op;
}
do_dec:
{
    vm_reg_t target = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[target] = vm_obj_num_sub(cur_locals[target], cur_locals[rhs]);
    run_next_op;
}
do_dec_num:
{
    vm_reg_t target = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[target] = vm_obj_num_subc(cur_locals[target], rhs);
    run_next_op;
}
do_add:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_num_add(cur_locals[lhs], cur_locals[rhs]);
    run_next_op;
}
do_add_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_num_addc(cur_locals[lhs], rhs);
    run_next_op;
}
do_mul:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_num_mul(cur_locals[lhs], cur_locals[rhs]);
    run_next_op;
}
do_mul_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_num_mulc(cur_locals[lhs], rhs);
    run_next_op;
}
do_sub:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    cur_locals[to] = vm_obj_num_sub(cur_locals[lhs], cur_locals[rhs]);
    run_next_op;
}
do_sub_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    cur_locals[to] = vm_obj_num_subc(cur_locals[lhs], rhs);
    run_next_op;
}
do_div:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    if (vm_obj_to_num(cur_locals[rhs]) == 0)
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_DIV0));
    }
    cur_locals[to] = vm_obj_num_div(cur_locals[lhs], cur_locals[rhs]);
    run_next_op;
}
do_div_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    if (rhs == 0)
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_DIV0));
    }
    cur_locals[to] = vm_obj_num_divc(cur_locals[lhs], rhs);
    run_next_op;
}
do_mod:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    if (vm_obj_to_num(cur_locals[rhs]) == 0)
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_MOD0));
    }
    cur_locals[to] = vm_obj_num_mod(cur_locals[lhs], cur_locals[rhs]);
    run_next_op;
}
do_mod_num:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    int rhs = read_int;
    vm_fetch;
    if (rhs == 0)
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_MOD0));
    }
    cur_locals[to] = vm_obj_num_modc(cur_locals[lhs], rhs);
    run_next_op;
}
do_concat:
{
    vm_reg_t to = read_reg;
    vm_reg_t lhs = read_reg;
    vm_reg_t rhs = read_reg;
    vm_fetch;
    vm_obj_t o1 = cur_locals[lhs];
    vm_obj_t o2 = cur_locals[rhs];
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(o1))
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
#if defined(VM_USE_TYPES)
    if (!vm_obj_is_ptr(o2))
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    cur_locals[to] = vm_gc_concat(gc, o1, o2);
#if defined(VM_USE_TYPES)
    if (vm_obj_is_dead(cur_locals[to]))
    {
        run_next_op_after_effect(to, vm_obj_of_num(VM_EFFECT_TYPE));
    }
#endif
    run_next_op;
}
do_putchar:
{
    vm_reg_t from = read_reg;
    vm_fetch;
    int val = vm_obj_to_int(cur_locals[from]);
    state->putchar(state, val);
    run_next_op;
}
}