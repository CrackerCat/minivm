#include "asm.h"

void vm_asm_strip(const char **src) {
    while (**src == ' ' || **src == '\t') {
        *src += 1;
    }
}

void vm_asm_stripln(const char **src) {
    while (**src == ' ' || **src == '\t' || **src == '\n' || **src == '\r' || **src == ';' || **src == '#') {
        if (**src == '#') {
            while (**src != '\n' && **src != '\r' && **src != '\0') {
                *src += 1;
            }
        } else {
            *src += 1;
        }
    }
}

int vm_asm_isdigit(char c) { return ('0' <= c && c <= '9'); }

int vm_asm_isword(char c) {
    return vm_asm_isdigit(c) || c == '_' || c == '.' || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

int vm_asm_starts(const char *in, const char *test) {
    while (*test) {
        if (*test != *in) {
            return 0;
        }
        test += 1;
        in += 1;
    }
    return 1;
}

size_t vm_asm_word(const char *src) {
    size_t n = 0;
    while (vm_asm_isword(src[n])) {
        n += 1;
    }
    return n;
}

size_t vm_asm_read_int(const char **src) {
    vm_asm_strip(src);
    vm_opcode_t n = 0;
    while ('0' <= **src && **src <= '9') {
        n *= 10;
        n += (size_t)(**src - '0');
        *src += 1;
    }
    return n;
}

size_t vm_asm_read_reg(const char **src) {
    vm_asm_strip(src);
    if (**src != 'r') {
        fprintf(stderr, "asm: bad char: expected 'r' (for reg) (got '%c')\n%.*s\n", **src, 100, *src);
        __builtin_trap();
    }
    *src += 1;
    size_t n = 0;
    while ('0' <= **src && **src <= '9') {
        n *= 10;
        n += (size_t)(**src - '0');
        *src += 1;
    }
    return n;
}

#define vm_asm_put(type_, value_)             \
    ({                                        \
        instrs[head].type = (type_);          \
        instrs[head].value = (value_);        \
        /*printf("intrs[%i] = {%i, %i}\n", (int) head, (int) type, (int) value);*/ \
        head += 1; \
    })
#define vm_asm_put_op(op_) vm_asm_put((VM_ASM_INSTR_RAW), (op_))
#define vm_asm_put_reg(reg_)                 \
    ({                                       \
        size_t r = reg_;                     \
        if (r > nregs) {                     \
            nregs = r;                       \
        }                                    \
        vm_asm_put((VM_ASM_INSTR_RAW), (r)); \
    })
#define vm_asm_put_int(int_) vm_asm_put((VM_ASM_INSTR_RAW), (int_))
#define vm_asm_put_set(name_)                            \
    ({                                                   \
        *nsets += 1;                                     \
        vm_asm_put((VM_ASM_INSTR_SET), (size_t)(name_)); \
    })
#define vm_asm_put_get(name_) vm_asm_put((VM_ASM_INSTR_GET), (size_t)(name_))
#define vm_asm_put_seti(num_) ({ vm_asm_put((VM_ASM_INSTR_SETI), (num_)); })
#define vm_asm_put_geti(num_) vm_asm_put((VM_ASM_INSTR_GETI), (num_))

vm_asm_instr_t *vm_asm_read(const char **src, size_t *nsets, size_t *nlinks) {
    size_t alloc = 32;
    vm_asm_instr_t *instrs = vm_malloc(sizeof(vm_asm_instr_t) * alloc);
    size_t head = 0;
    vm_asm_put_op(VM_OPCODE_JUMP);
    vm_asm_put_get("__entry");
    size_t where = 0;
    size_t nregs = 0;
    for (;;) {
        // printf("%i\n", instrs->type);
        if (head + 16 > alloc) {
            alloc = head * 4 + 16;
            instrs = vm_realloc(instrs, sizeof(vm_asm_instr_t) * alloc);
        }
        vm_asm_stripln(src);
        if (**src == '\0') {
            break;
        }
        if (**src == '@') {
            *src += 1;
            vm_asm_strip(src);
            vm_asm_put_set(*src);
            *src += vm_asm_word(*src);
            continue;
        } else if (**src == 'r' && '0' <= (*src)[1] && (*src)[1] <= '9') {
            size_t regn = vm_asm_word(*src);
            size_t regno = 0;
            int isreg = 1;
            for (size_t i = 1; i < regn; i++) {
                if (vm_asm_isdigit((*src)[i]) == 0) {
                    isreg = false;
                    break;
                }
                regno *= 10;
                regno += (size_t)((*src)[i] - '0');
            }
            if (isreg) {
                *src += regn;
                vm_asm_strip(src);
                if (vm_asm_starts(*src, "<-") == 0) {
                    goto err;
                }
                *src += 2;
                vm_asm_strip(src);
                const char *opname = *src;
                *src += vm_asm_word(*src);
                // printf("%s\n", opname);
                if (vm_asm_starts(opname, "int")) {
                    vm_asm_strip(src);
                    if (**src == '-') {
                        vm_asm_put_op(VM_OPCODE_NEG);
                        vm_asm_put_reg(regno);
                        *src += 1;
                    } else {
                        vm_asm_put_op(VM_OPCODE_INT);
                        vm_asm_put_reg(regno);
                    }
                    vm_asm_put_int(vm_asm_read_int(src));
                    continue;
                }
                if (vm_asm_starts(opname, "arr")) {
                    vm_asm_put_op(VM_OPCODE_ARR);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "tab")) {
                    vm_asm_put_op(VM_OPCODE_TAB);
                    vm_asm_put_reg(regno);
                    continue;
                }
                if (vm_asm_starts(opname, "nil")) {
                    vm_asm_put_op(VM_OPCODE_NIL);
                    vm_asm_put_reg(regno);
                    continue;
                }
                if (vm_asm_starts(opname, "getchar")) {
                    vm_asm_put_op(VM_OPCODE_GETCHAR);
                    vm_asm_put_reg(regno);
                    continue;
                }
                if (vm_asm_starts(opname, "true")) {
                    vm_asm_put_op(VM_OPCODE_TRUE);
                    vm_asm_put_reg(regno);
                    continue;
                }
                if (vm_asm_starts(opname, "false")) {
                    vm_asm_put_op(VM_OPCODE_FALSE);
                    vm_asm_put_reg(regno);
                    continue;
                }
                if (vm_asm_starts(opname, "get")) {
                    vm_asm_put_op(VM_OPCODE_GET);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "len")) {
                    vm_asm_put_op(VM_OPCODE_LEN);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "type")) {
                    vm_asm_put_op(VM_OPCODE_TYPE);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "addr")) {
                    vm_asm_put_op(VM_OPCODE_ADDR);
                    vm_asm_put_reg(regno);
                    vm_asm_strip(src);
                    vm_asm_put_get(*src);
                    *src += vm_asm_word(*src);
                    continue;
                }
                if (vm_asm_starts(opname, "reg")) {
                    vm_asm_put_op(VM_OPCODE_REG);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "call")) {
                    vm_asm_put_op(VM_OPCODE_CALL);
                    vm_asm_put_reg(regno);
                    vm_asm_strip(src);
                    vm_asm_put_get(*src);
                    *src += vm_asm_word(*src);
                    size_t args[8] = {0};
                    size_t nargs = 0;
                    vm_asm_strip(src);
                    while (**src != '\n' && **src != '\r') {
                        args[nargs++] = vm_asm_read_reg(src);
                        vm_asm_strip(src);
                    }
                    vm_asm_put_int(nargs);
                    for (size_t i = 0; i < nargs; i++) {
                        vm_asm_put_reg(args[i]);
                    }
                    continue;
                }
                if (vm_asm_starts(opname, "ccall")) {
                    vm_asm_put_op(VM_OPCODE_CCALL);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    size_t args[8] = {0};
                    size_t nargs = 0;
                    vm_asm_strip(src);
                    while (**src != '\n' && **src != '\r') {
                        args[nargs++] = vm_asm_read_reg(src);
                        vm_asm_strip(src);
                    }
                    vm_asm_put_int(nargs);
                    for (size_t i = 0; i < nargs; i++) {
                        vm_asm_put_reg(args[i]);
                    }
                    continue;
                }
                if (vm_asm_starts(opname, "dcall")) {
                    vm_asm_put_op(VM_OPCODE_DCALL);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    size_t args[8] = {0};
                    size_t nargs = 0;
                    vm_asm_strip(src);
                    while (**src != '\n' && **src != '\r') {
                        args[nargs++] = vm_asm_read_reg(src);
                        vm_asm_strip(src);
                    }
                    vm_asm_put_int(nargs);
                    for (size_t i = 0; i < nargs; i++) {
                        vm_asm_put_reg(args[i]);
                    }
                    continue;
                }
                if (vm_asm_starts(opname, "xcall")) {
                    vm_asm_put_op(VM_OPCODE_XCALL);
                    vm_asm_put_reg(regno);
                    vm_asm_put_int(vm_asm_read_int(src));
                    size_t args[8] = {0};
                    size_t nargs = 0;
                    vm_asm_strip(src);
                    while (**src != '\n' && **src != '\r') {
                        args[nargs++] = vm_asm_read_reg(src);
                        vm_asm_strip(src);
                    }
                    vm_asm_put_int(nargs);
                    for (size_t i = 0; i < nargs; i++) {
                        vm_asm_put_reg(args[i]);
                    }
                    continue;
                }
                if (vm_asm_starts(opname, "add")) {
                    vm_asm_put_op(VM_OPCODE_ADD);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "sub")) {
                    vm_asm_put_op(VM_OPCODE_SUB);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "mul")) {
                    vm_asm_put_op(VM_OPCODE_MUL);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "div")) {
                    vm_asm_put_op(VM_OPCODE_DIV);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "mod")) {
                    vm_asm_put_op(VM_OPCODE_MOD);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "bor")) {
                    vm_asm_put_op(VM_OPCODE_BOR);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "band")) {
                    vm_asm_put_op(VM_OPCODE_BAND);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "bxor")) {
                    vm_asm_put_op(VM_OPCODE_BXOR);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "bshl")) {
                    vm_asm_put_op(VM_OPCODE_BSHL);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
                if (vm_asm_starts(opname, "bshr")) {
                    vm_asm_put_op(VM_OPCODE_BSHR);
                    vm_asm_put_reg(regno);
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    vm_asm_put_reg(vm_asm_read_reg(src));
                    continue;
                }
            }
        } else {
            vm_asm_strip(src);
            const char *opname = *src;
            *src += vm_asm_word(*src);
            if (vm_asm_starts(opname, "func")) {
                nregs = 0;
                vm_asm_put_op(VM_OPCODE_FUNC);
                vm_asm_put_geti(*nlinks);
                vm_asm_strip(src);
                const char *fn = *src;
                *src += vm_asm_word(*src);
                vm_asm_put_int(0);
                where = head;
                vm_asm_put_int(0);
                vm_asm_put_set(fn);
                continue;
            }
            if (vm_asm_starts(opname, "end")) {
                vm_asm_put_seti((*nlinks)++);
                instrs[where].value = nregs + 1;
                nregs = 0;
                vm_asm_stripln(src);
                continue;
            }
            if (vm_asm_starts(opname, "exit")) {
                vm_asm_put_op(VM_OPCODE_EXIT);
                continue;
            }
            if (vm_asm_starts(opname, "ret")) {
                vm_asm_put_op(VM_OPCODE_RET);
                vm_asm_put_reg(vm_asm_read_reg(src));
                continue;
            }
            if (vm_asm_starts(opname, "putchar")) {
                vm_asm_put_op(VM_OPCODE_PUTCHAR);
                vm_asm_put_reg(vm_asm_read_reg(src));
                continue;
            }
            if (vm_asm_starts(opname, "jump")) {
                vm_asm_put_op(VM_OPCODE_JUMP);
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                continue;
            }
            if (vm_asm_starts(opname, "bb")) {
                vm_asm_put_op(VM_OPCODE_BB);
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                continue;
            }
            if (vm_asm_starts(opname, "blt")) {
                vm_asm_put_op(VM_OPCODE_BLT);
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                continue;
            }
            if (vm_asm_starts(opname, "beq")) {
                vm_asm_put_op(VM_OPCODE_BEQ);
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                vm_asm_strip(src);
                vm_asm_put_get(*src);
                *src += vm_asm_word(*src);
                continue;
            }
            if (vm_asm_starts(opname, "set")) {
                vm_asm_put_op(VM_OPCODE_SET);
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_put_reg(vm_asm_read_reg(src));
                vm_asm_put_reg(vm_asm_read_reg(src));
                continue;
            }
        }
        goto err;
    }
    instrs[head].type = VM_ASM_INSTR_END;
    return instrs;
err:
    fprintf(stderr, "%.100s", *src);
    fprintf(stderr, "asm error: source can not be parsed\n");
    vm_free(instrs);
    return NULL;
}

typedef struct {
    const char *name;
    vm_opcode_t where;
} vm_asm_link_t;

static size_t vm_asm_hash(const char *str, size_t n) {
    size_t hash = 5381;
    for (size_t i = 0; i < n; i++) {
        hash = ((hash << 5) + hash) + (size_t)str[i];
    }
    return hash;
}

vm_bc_buf_t vm_asm_link(vm_asm_instr_t *instrs, size_t ns, size_t ni) {
    size_t links_alloc = ns * 2;
    size_t *nums = vm_alloc0(sizeof(size_t) * ni);
    vm_asm_link_t *links = vm_alloc0(sizeof(vm_asm_link_t) * links_alloc);
    size_t cur_loc = 0;
    // printf("%i != %i -> %i\n", instrs->type, VM_ASM_INSTR_END, instrs->type != VM_ASM_INSTR_END);
    for (vm_asm_instr_t *cur = instrs; cur->type != VM_ASM_INSTR_END; cur++) {
        // printf("BEGIN\n");
        switch (cur->type) {
            case VM_ASM_INSTR_RAW: {
                cur_loc += 1;
                break;
            }
            case VM_ASM_INSTR_GETI: {
                cur_loc += 1;
                break;
            }
            case VM_ASM_INSTR_GET: {
                cur_loc += 1;
                break;
            }
            case VM_ASM_INSTR_SETI: {
                nums[cur->value] = cur_loc;
                break;
            }
            case VM_ASM_INSTR_SET: {
                const char *str = (const char *)cur->value;
                size_t put = vm_asm_hash(str, vm_asm_word(str));
                while (links[put % links_alloc].name != NULL) {
                    put += 1;
                }
                links[put % links_alloc].name = str;
                links[put % links_alloc].where = (vm_opcode_t)cur_loc;
                break;
            }
        }
        // printf("END\n");
    }
    size_t ops_alloc = 256;
    vm_opcode_t *ops = vm_malloc(sizeof(vm_opcode_t) * ops_alloc);
    size_t nops = 0;
    for (vm_asm_instr_t *cur = instrs; cur->type != VM_ASM_INSTR_END; cur++) {
        if (nops + 2 > ops_alloc) {
            ops_alloc = nops * 4 + 2;
            ops = vm_realloc(ops, sizeof(vm_opcode_t) * ops_alloc);
        }
        switch (cur->type) {
            case VM_ASM_INSTR_RAW: {
                ops[nops++] = (vm_opcode_t)cur->value;
                break;
            }
            case VM_ASM_INSTR_GETI: {
                ops[nops++] = (vm_opcode_t)nums[cur->value];
                break;
            }
            case VM_ASM_INSTR_GET: {
                size_t val = 0;
                const char *find = (const char *)cur->value;
                size_t findlen = vm_asm_word(find);
                size_t get = vm_asm_hash(find, findlen);
                size_t start = get;
                while (get != start + links_alloc) {
                    const char *name = links[get % links_alloc].name;
                    if (name == NULL) {
                        get += 1;
                        continue;
                    }
                    size_t linklen = vm_asm_word(name);
                    if (linklen == findlen) {
                        size_t head = 0;
                        while (head < findlen) {
                            if (find[head] != name[head]) {
                                break;
                            }
                            head += 1;
                            if (head == findlen) {
                                val = links[get % links_alloc].where;
                                goto done;
                            }
                        }
                    }
                    get += 1;
                }
                if (findlen == 0) {
                    fprintf(stderr, "linker error: empty symbol\n");
                }
                fprintf(stderr, "linker error: undefined: %.*s\n", (int)vm_asm_word(find), find);
                goto err;
            done:
                ops[nops++] = (vm_opcode_t)val;
                break;
            }
            case VM_ASM_INSTR_SETI: {
                break;
            }
            case VM_ASM_INSTR_SET: {
                break;
            }
        }
    }
    vm_free(links);
    vm_free(nums);
    return (vm_bc_buf_t){
        .ops = ops,
        .nops = nops,
    };
err:
    vm_free(links);
    vm_free(nums);
    vm_free(ops);
    return (vm_bc_buf_t){
        .nops = 0,
    };
}

vm_bc_buf_t vm_asm(const char *src) {
    size_t ns = 0;
    size_t ni = 0;
    vm_asm_instr_t *instrs = vm_asm_read(&src, &ns, &ni);
    // printf("instrs = %i\n", (int) instrs);
    if (instrs == NULL) {
        return (vm_bc_buf_t){
            .nops = 0,
            .ops = NULL,
        };
    }
    vm_bc_buf_t buf = vm_asm_link(instrs, ns, ni);
    vm_free(instrs);
    return buf;
}
