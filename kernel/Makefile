TARGET = kernel.elf
OBJS = main.o graphics.o font.o cp1251/cp1251.o newlib_support.o console.o pci.o asmfunc.o
DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

DEVENV_DIR = $(HOME)/make-os/devenv
EDK_DIR = $(HOME)/edk2

INCS += -I$(DEVENV_DIR)/x86_64-elf/include/c++/v1 \
	-I$(DEVENV_DIR)/x86_64-elf/include \
	-I$(DEVENV_DIR)/x86_64-elf/include/freetype2 \
	-I$(EDK_DIR)/MdePkg/Include \
	-I$(EDK_DIR)/MdePkg/Include/X64

LIBS += -L$(DEVENV_DIR)/x86_64-elf/lib

CFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone

CXXFLAGS += -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS \
	-DEFIAPI='__attribute__((ms_abi))' \
	-O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17

LDFLAGS += --entry KernelMain -z norelro --image-base 0x100000 --static

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o
	rm -rf *.d
	rm -rf .*.d

kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) $(LIBS) -o kernel.elf $(OBJS) -lc

%.o: %.cpp Makefile
	clang++ $(CXXFLAGS) $(INCS) -c $<

.%.d: %.cpp
	clang++ $(CXXFLAGS) $(INCS) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.c Makefile
	clang $(CFLAGS) $(INCS) -c $<

.%.d: %.c
	clang $(CFLAGS) $(INCS) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<

cp1251.bin: cp1251/cp1251.txt
	../tools/makefont.py -o $@ $<

cp1251.o: cp1251/cp1251.o
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

.$.d: %.bin
	touch $@

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

-include $(DEPENDS)