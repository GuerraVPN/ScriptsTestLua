# Orbis Vault PS4
TITLE       := Orbis Vault
VERSION     := 0.10
TITLE_ID    := BREW00901
CONTENT_ID  := IV0000-BREW00901_00-ORBISVAULTPS4001

# 0 = transport/catalog scaffold only
# 1 = link SceAppInstUtil adapter for authorized local packages
NATIVE_INSTALL ?= 0

# libc++ threading support in the OpenOrbis SDK expects nanosleep to be declared
# before standard C++ headers are parsed.
EXTRAFLAGS := -include compat.hpp

LIBS := -lc -lkernel -lc++ -lSceNet -lSceSsl -lSceHttp -lSceSysmodule

ifeq ($(NATIVE_INSTALL),1)
    EXTRAFLAGS += -DORBIS_VAULT_NATIVE_INSTALL
    LIBS += -lSceAppInstUtil
endif

ASSETS      := $(wildcard assets/**/*)
LIBMODULES  := $(wildcard sce_module/*)

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := .
COMMONDIR   := $(TOOLCHAIN)/samples/_common
INTDIR      := x64/Debug

CFILES      := $(wildcard *.c)
CPPFILES    := $(wildcard *.cpp)
OBJS        := $(patsubst %.c,$(INTDIR)/%.o,$(CFILES)) $(patsubst %.cpp,$(INTDIR)/%.o,$(CPPFILES))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -std=c++17 -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

_unused     := $(shell mkdir -p $(INTDIR) sce_sys)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CC   := clang
    CCX  := clang++
    LD   := ld.lld
    CDIR := linux
endif
ifeq ($(UNAME_S),Darwin)
    CC   := /usr/local/opt/llvm/bin/clang
    CCX  := /usr/local/opt/llvm/bin/clang++
    LD   := /usr/local/opt/llvm/bin/ld.lld
    CDIR := macos
endif

all: eboot.bin

pkg: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	PkgTool.Core sfo_new $@
	PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/orbis-vault.elf $(LDFLAGS)
	create-fself -in=$(INTDIR)/orbis-vault.elf -out=$(INTDIR)/orbis-vault.oelf --eboot eboot.bin --paid 0x3800000000000011

$(INTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: %.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 $(CONTENT_ID).pkg sce_sys/param.sfo
