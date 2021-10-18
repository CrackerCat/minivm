HOST=v8

CWD:=$(shell pwd)
BIN:=$(CWD)/bin
LIB:=$(CWD)/lib

CLANG=clang
LLC=llc
WASMLD=wasm-ld
WASMOPT=wasm-opt

SRCS:=vm/vm.c vm/gc.c vm/wasm.c

OBJS=$(patsubst %.c,$(LIB)/%.o,$(SRCS))

CFLAGS=--target=wasm32 -Ofast -ffast-math -emit-llvm -c -S -I. -DVM_NO_STD -nostdlib -g

default: all

all: $(BIN)/minivm.js

$(BIN)/minivm.js: $(BIN)/minivm.wasm
	@cat main/$(HOST).js | sed "s:__VM_WASM_FILE_PATH__:'$(BIN)/minivm.wasm':" > $@
	chmod +x $@

$(BIN)/minivm.wasm: $(OBJS)
	@mkdir -p $(dir $@)
	$(WASMLD) --export=vm_xrun --export=vm_xadd --export=vm_xset_putchar --allow-undefined -s $(OBJS) -o $@.unopt
	$(WASMOPT) -O4 $@.unopt -o $@

$(OBJS): $(patsubst $(LIB)/%.ll,%.c,$@)
	@mkdir -p $(dir $@)
	$(CLANG) $(CFLAGS) $(patsubst $(LIB)/%.o,%.c,$@) -o $(patsubst $(LIB)/%.o,$(LIB)/%.bc,$@)
	$(LLC) -march=wasm32 -filetype=obj $(patsubst $(LIB)/%.o,$(LIB)/%.bc,$@) -o $@
	