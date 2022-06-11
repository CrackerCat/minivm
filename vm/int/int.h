
#if !defined(VM_HEADER_INT_INT)
#define VM_HEADER_INT_INT

#include "gc.h"
#include "../lib.h"
#include "../opcode.h"

enum vm_int_op_t
{
  VM_INT_OP_EXIT,
  VM_INT_OP_PUT,
  VM_INT_OP_MOV,
  VM_INT_OP_MOVC,
  VM_INT_OP_MOVF,
  VM_INT_OP_ADD,
  VM_INT_OP_SUB,
  VM_INT_OP_RET,
  VM_INT_OP_BEQ,
  VM_INT_OP_BEQI,
  VM_INT_OP_CALL0,
  VM_INT_OP_CALL1,
  VM_INT_OP_CALL2,
  VM_INT_OP_CALL3,
  VM_INT_OP_CALL4,
  VM_INT_OP_CALL5,
  VM_INT_OP_CALL6,
  VM_INT_OP_CALL7,
  VM_INT_OP_CALL8,
  VM_INT_OP_DCALL0,
  VM_INT_OP_DCALL1,
  VM_INT_OP_DCALL2,
  VM_INT_OP_DCALL3,
  VM_INT_OP_DCALL4,
  VM_INT_OP_DCALL5,
  VM_INT_OP_DCALL6,
  VM_INT_OP_DCALL7,
  VM_INT_OP_DCALL8,
  VM_INT_OP_JUMP,
  VM_INT_OP_DJUMP,
  VM_INT_OP_ADDI,
  VM_INT_OP_SUBI,
  VM_INT_OP_ISUB,
  VM_INT_OP_RETC,
  VM_INT_OP_MUL,
  VM_INT_OP_MULI,
  VM_INT_OP_DIV,
  VM_INT_OP_DIVI,
  VM_INT_OP_IDIV,
  VM_INT_OP_MOD,
  VM_INT_OP_MODI,
  VM_INT_OP_IMOD,
  VM_INT_OP_BB,
  VM_INT_OP_BLT,
  VM_INT_OP_BLTI,
  VM_INT_OP_IBLT,
  VM_INT_OP_ARR,
  VM_INT_OP_ARRI,
  VM_INT_OP_MAP,
  VM_INT_OP_GET,
  VM_INT_OP_GETI,
  VM_INT_OP_SET,
  VM_INT_OP_SETI,
  VM_INT_OP_ISET,
  VM_INT_OP_ISETI,
  VM_INT_OP_LEN,
  VM_INT_OP_ADD2,
  VM_INT_OP_ADD2I,
  VM_INT_OP_SUB2,
  VM_INT_OP_SUB2I,
  VM_INT_OP_2SUB,
  VM_INT_OP_I2SUB,
  VM_INT_OP_TYPE,
  VM_INT_MAX_OP,
};

uint8_t *vm_int_comp(size_t nops, const vm_opcode_t *ops, uint8_t *jumps, void **ptrs, vm_gc_t *restrict gc);
int vm_int_run(size_t nops, const vm_opcode_t *iops, vm_gc_t *restrict gc);

#endif
