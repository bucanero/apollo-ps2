# mcimage.irx - raw memory card page access (see mcimage.c)
IOP_BIN = mcimage.irx
IOP_OBJS = mcimage.o imports.o

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
