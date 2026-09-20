# Orbis Vault PS4
TITLE       := Orbis Vault
VERSION     := 0.21
TITLE_ID    := BREW00902
CONTENT_ID  := IV0000-BREW00902_00-ORBISVAULTSAFE01

# SAFE 0.2 links only libraries used by the official SDL sample.
# Network, AppInstUtil and SystemService are loaded dynamically after the UI boots.
EXTRAFLAGS := -include compat.hpp
LIBS := -lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSDL2

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

_unused     := $(shell mkdir -p $(INTDIR) sce_sys sce_sys/about)

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
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ PARENTAL_LEVEL --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ REMOTE_PLAY_KEY_ASSIGN --type Integer --maxsize 4 --value 0

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/orbis-vault.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/orbis-vault.elf -out=$(INTDIR)/orbis-vault.oelf --eboot eboot.bin --paid 0x3800000000000011

$(INTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: %.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 $(CONTENT_ID).pkg sce_sys/param.sfo
