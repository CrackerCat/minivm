#include <vm/backend/back.h>

void vm_backend_js(opcode_t *basefunc)
{
	int n = 0;
	int cur_index = 0;
	int *nregs = alloca(sizeof(int) * 256);
	int *rec = alloca(sizeof(int) * 256);
	int *base = rec;
	*rec = 0;
	*nregs = 256;
	printf("let o=0,r=[],a,d=0,s=[0];b:while(1){switch(o|0){");
	while (true)
	{
		printf("case %i:", cur_index);
	nocase:;
		opcode_t op = read_instr;
		switch (op)
		{
		case OPCODE_STORE_REG:
		{
			reg_t reg = read_reg;
			reg_t from = read_reg;
			printf("r[%i+d]=r[%i+d];", reg, from);
			break;
		}
		case OPCODE_STORE_INT:
		{
			reg_t reg = read_reg;
			int n = read_int;
			printf("r[%i+d]=%i;", reg, n);
			break;
		}
		case OPCODE_STORE_FUN:
		{
			reg_t to = read_reg;
			int end = read_int;
			if (rec - base > 250)
			{
				fprintf(stderr, "too much nesting\n");
				return;
			}
			*(++rec) = cur_index;
			printf("r[%i+d]=%i;", to, cur_index);
			printf("o=%i;continue b;", end);
			printf("case %i:", cur_index);
			*(++nregs) = read_int;
			goto nocase;
		}
		case OPCODE_FUN_DONE:
		{
			rec--;
			nregs--;
			break;
		}
		case OPCODE_RETURN:
		{
			reg_t reg = read_reg;
			printf("a=r[%i+d];", reg);
			printf("d=s.pop();");
			printf("r[s.pop()+d]=a;");
			printf("o=s.pop();continue b;");
			break;
		}
		case OPCODE_CALL0:
		{
			reg_t outreg = read_reg;
			reg_t func = read_reg;
			int next_func = read_loc;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("o=r[%i+d];s.push(d);d+=%i;continue b;", func, *nregs);
			break;
		}
		case OPCODE_CALL1:
		{
			reg_t outreg = read_reg;
			reg_t func = read_loc;
			reg_t r1arg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("r[d+%i]=r[%i+d];", *nregs, r1arg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=r[%i+d];s.push(d);d+=%i;continue b;", func, *nregs);
			break;
		}
		case OPCODE_CALL2:
		{
			reg_t outreg = read_reg;
			reg_t func = read_loc;
			reg_t r1arg = read_reg;
			reg_t r2arg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("r[d+%i]=r[%i+d];", *nregs, r1arg);
			printf("r[d+%i]=r[%i+d];", 1 + *nregs, r2arg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=r[%i+d];s.push(d);d+=%i;continue b;", func, *nregs);
			break;
		}
		case OPCODE_CALL:
		{
			reg_t outreg = read_reg;
			reg_t func = read_loc;
			int nargs = read_int;
			printf("s.push(%i,%i);", cur_index, outreg);
			for (int i = 0; i < nargs; i++)
			{
				if (i != 0)
				{
					printf(",");
				}
				printf("r[d+%i]=r[d+%i];", i + *nregs, read_reg);
			}
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=r[%i+d];s.push(d);d+=%i;continue b;", func, *nregs);
			break;
		}
		case OPCODE_STATIC_CALL0:
		{
			reg_t outreg = read_reg;
			int next_func = read_loc;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", next_func);
			break;
		}
		case OPCODE_STATIC_CALL1:
		{
			reg_t outreg = read_reg;
			int next_func = read_loc;
			reg_t r1arg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("r[d+%i]=r[%i+d];", *nregs, r1arg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", next_func);
			break;
		}
		case OPCODE_STATIC_CALL2:
		{
			reg_t outreg = read_reg;
			int next_func = read_loc;
			reg_t r1arg = read_reg;
			reg_t r2arg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("r[%i+d]=r[%i+d];", *nregs, r1arg);
			printf("r[%i+d]=r[%i+d];", 1 + *nregs, r2arg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", next_func);
			break;
		}
		case OPCODE_STATIC_CALL:
		{
			reg_t outreg = read_reg;
			int next_func = read_loc;
			int nargs = read_int;
			printf("s.push(%i,%i);", cur_index, outreg);
			for (int i = 0; i < nargs; i++)
			{
				if (i != 0)
				{
					printf(",");
				}
				printf("r[d+%i]=r[d+%i];", i + *nregs, read_reg);
			}
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", next_func);
			break;
		}
		case OPCODE_REC0:
		{
			reg_t outreg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", *rec);
			break;
		}
		case OPCODE_REC1:
		{
			reg_t outreg = read_reg;
			reg_t r1arg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("r[d+%i]=r[%i+d];", *nregs, r1arg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", *rec);
			break;
		}
		case OPCODE_REC2:
		{
			reg_t outreg = read_reg;
			reg_t r1arg = read_reg;
			reg_t r2arg = read_reg;
			printf("s.push(%i,%i);", cur_index, outreg);
			printf("r[%i+d]=r[%i+d];", *nregs, r1arg);
			printf("r[%i+d]=r[%i+d];", 1 + *nregs, r2arg);
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", *rec);
			break;
		}
		case OPCODE_REC:
		{
			reg_t outreg = read_reg;
			int nargs = read_int;
			printf("s.push(%i,%i);", cur_index, outreg);
			for (int i = 0; i < nargs; i++)
			{
				if (i != 0)
				{
					printf(",");
				}
				printf("r[d+%i]=r[d+%i];", i + *nregs, read_reg);
			}
			printf("s.push(d);d+=%i;", *nregs);
			printf("o=%i;continue b;", *rec);
			break;
		}
		case OPCODE_EQUAL:
		{
			reg_t to = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=+(r[%i+d]==r[%i+d]);", to, lhs, rhs);
			break;
		}
		case OPCODE_EQUAL_NUM:
		{
			reg_t to = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("r[%i+d]=+(r[%i+d]<%i);", to, lhs, rhs);
			break;
		}
		case OPCODE_NOT_EQUAL:
		{
			reg_t to = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=+(r[%i+d]<r[%i+d]);", to, lhs, rhs);
			break;
		}
		case OPCODE_NOT_EQUAL_NUM:
		{
			reg_t to = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("r[%i+d]=+(r[%i+d]<%i);", to, lhs, rhs);
			break;
		}
		case OPCODE_LESS:
		{
			reg_t to = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=+(r[%i+d]<r[%i+d]);", to, lhs, rhs);
			break;
		}
		case OPCODE_LESS_NUM:
		{
			reg_t to = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("r[%i+d]=+(r[%i+d]<%i);", to, lhs, rhs);
			break;
		}
		case OPCODE_GREATER:
		{
			reg_t to = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=+(r[%i+d]>r[%i+d]);", to, lhs, rhs);
			break;
		}
		case OPCODE_GREATER_NUM:
		{
			reg_t to = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("r[%i+d]=+(r[%i+d]>%i);", to, lhs, rhs);
			break;
		}
		case OPCODE_LESS_THAN_EQUAL:
		{
			reg_t to = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=+(r[%i+d]<=r[%i+d]);", to, lhs, rhs);
			break;
		}
		case OPCODE_LESS_THAN_EQUAL_NUM:
		{
			reg_t to = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("r[%i+d]=+(r[%i+d]<=%i);", to, lhs, rhs);
			break;
		}
		case OPCODE_GREATER_THAN_EQUAL:
		{
			reg_t to = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=+(r[%i+d]>=r[%i+d]);", to, lhs, rhs);
			break;
		}
		case OPCODE_GREATER_THAN_EQUAL_NUM:
		{
			reg_t to = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("r[%i+d]=+(r[%i+d]>=%i);", to, lhs, rhs);
			break;
		}
		case OPCODE_JUMP_ALWAYS:
		{
			int loc = read_loc;
			printf("o=%i;continue b;", loc);
			break;
		}
		case OPCODE_JUMP_IF_FALSE:
		{
			int loc = read_loc;
			reg_t reg = read_reg;
			printf("if(!r[%i+d]){o=%i;continue b;}", reg, loc);
			break;
		}
		case OPCODE_JUMP_IF_TRUE:
		{
			int loc = read_loc;
			reg_t reg = read_reg;
			printf("if(r[%i+d]){o=%i;continue b;}", reg, loc);
			break;
		}
		case OPCODE_JUMP_IF_EQUAL:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("if(r[%i+d]===r[%i+d]){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_EQUAL_NUM:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("if(r[%i+d]===%i){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_NOT_EQUAL:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("if(r[%i+d]!==r[%i+d]){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_NOT_EQUAL_NUM:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("if(r[%i+d]!==%i){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_LESS:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("if(r[%i+d]<r[%i+d]){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_LESS_NUM:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("if(r[%i+d]<%i){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_GREATER:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("if(r[%i+d]>r[%i+d]){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_GREATER_NUM:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("if(r[%i+d]>%i){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_LESS_THAN_EQUAL:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("if(r[%i+d]<=r[%i+d]){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_LESS_THAN_EQUAL_NUM:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("if(r[%i+d]<=%i){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_GREATER_THAN_EQUAL:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("if(r[%i+d]>=r[%i+d]){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_JUMP_IF_GREATER_THAN_EQUAL_NUM:
		{
			int loc = read_loc;
			reg_t lhs = read_reg;
			int rhs = read_int;
			printf("if(r[%i+d]>=%i){o=%i;continue b;}", lhs, rhs, loc);
			break;
		}
		case OPCODE_INC:
		{
			reg_t reg = read_reg;
			reg_t from = read_reg;
			printf("r[%i+d]+=r[%i+d];", reg, from);
			break;
		}
		case OPCODE_INC_NUM:
		{
			reg_t reg = read_reg;
			int n = read_int;
			printf("r[%i+d]+=%i;", reg, n);
			break;
		}
		case OPCODE_DEC:
		{
			reg_t reg = read_reg;
			reg_t from = read_reg;
			printf("r[%i+d]-=r[%i+d];", reg, from);
			break;
		}
		case OPCODE_DEC_NUM:
		{
			reg_t reg = read_reg;
			int n = read_int;
			printf("r[%i+d]-=%i;", reg, n);
			break;
		}
		case OPCODE_ADD:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]+r[%i+d];", reg, lhs, rhs);
			break;
		}
		case OPCODE_ADD_NUM:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]+%i;", reg, lhs, rhs);
			break;
		}
		case OPCODE_SUB:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]-r[%i+d];", reg, lhs, rhs);
			break;
		}
		case OPCODE_SUB_NUM:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]-%i;", reg, lhs, rhs);
			break;
		}
		case OPCODE_MUL:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]*r[%i+d];", reg, lhs, rhs);
			break;
		}
		case OPCODE_MUL_NUM:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]*%i;", reg, lhs, rhs);
			break;
		}
		case OPCODE_DIV:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]/r[%i+d];", reg, lhs, rhs);
			break;
		}
		case OPCODE_DIV_NUM:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]/%i;", reg, lhs, rhs);
			break;
		}
		case OPCODE_MOD:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]%%r[%i+d];", reg, lhs, rhs);
			break;
		}
		case OPCODE_MOD_NUM:
		{
			reg_t reg = read_reg;
			reg_t lhs = read_reg;
			reg_t rhs = read_reg;
			printf("r[%i+d]=r[%i+d]%%%i;", reg, lhs, rhs);
			break;
		}
		case OPCODE_ARRAY:
		{
			reg_t outreg = read_reg;
			int nargs = read_int;
			printf("r[%i+d]=[", outreg);
			for (int i = 0; i < nargs; i++)
			{
				reg_t reg = read_reg;
				if (i != 0)
				{
					printf(",");
				}
				printf("r[%i+d]", reg);
			}
			printf("];");
			break;
		}
		case OPCODE_LENGTH:
		{
			reg_t outreg = read_reg;
			reg_t reg = read_reg;
			printf("r[%i+d]=r[%i+d].length;", outreg, reg);
			break;
		}
		case OPCODE_INDEX:
		{
			reg_t outreg = read_reg;
			reg_t reg = read_reg;
			reg_t ind = read_reg;
			printf("r[%i+d]=r[%i+d][r[%i+d]];", outreg, reg, ind);
			break;
		}
		case OPCODE_INDEX_NUM:
		{
			reg_t outreg = read_reg;
			reg_t reg = read_reg;
			int index = read_reg;
			printf("r[%i+d]=r[%i+d][%i];", outreg, reg, index);
			break;
		}
		case OPCODE_PRINTLN:
		{
			reg_t reg = read_reg;
			printf("console.log(r[%i+d]);", reg);
			break;
		}
		case OPCODE_EXIT:
		{
			printf("break b;}}");
			goto done;
		}
		default:
		{
			fprintf(stderr, "unhandle opcode: %i\n", (int)op);
			goto done;
		}
		}
	}
done:
	return;
}
