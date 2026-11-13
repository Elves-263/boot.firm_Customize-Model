#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

# Default 3DSX TitleID for hb:ldr (note: also defined in top-level Makefile)
export HBLDR_DEFAULT_3DSX_TID ?= 000400000D921E00

# What to call the title corresponding to HBLDR_DEFAULT_3DSX_TID (note: also defined in top-level Makefile)
export HBLDR_DEFAULT_3DSX_TITLE_NAME ?= "hblauncher_loader"

# 起動時に新しい版を確かめる GitHub のリポジトリ（所有者/名前）。空なら確かめない（build.bat で設定する。
# note: also defined in top-level Makefile）
export UPDATE_REPO ?=

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
# 改変版が新しい方と v9.1 で共通に使うコード（リポジトリ直下の common/。BearSSL は起動時の更新確認の TLS）
COMMON		:=	../../../common
SOURCES		:=	source source/gdb source/menus source/plugin source/redshift \
				$(COMMON)/rosalina/source $(COMMON)/rosalina/source/menus $(COMMON)/bearssl
DATA		:=	source/gdb/xml data
INCLUDES	:=	include include/gdb include/menus include/redshift \
				$(COMMON)/rosalina/include $(COMMON)/bearssl

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
# 上流の版（13.5 など）。Luma3DS_new/UPSTREAM_VERSION に書いてある（build.bat も同じものを渡す）
export LUMA_NEW_VERSION ?= $(patsubst v%,%,$(strip $(shell cat "$(CURDIR)/../../UPSTREAM_VERSION")))

DEFINES :=	-D__3DS__ -DHBLDR_DEFAULT_3DSX_TID="0x$(HBLDR_DEFAULT_3DSX_TID)ULL" -DHBLDR_DEFAULT_3DSX_TITLE_NAME="\"$(HBLDR_DEFAULT_3DSX_TITLE_NAME)\"" \
			-DUPDATE_REPO="\"$(UPDATE_REPO)\"" -DLUMA_NEW_VERSION="\"$(LUMA_NEW_VERSION)\""

FALSEPOSITIVES := -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
# --- 追加: -fpermissive の代わりの、的を絞った診断設定 -----------------------
# GCC 14 以降、以下はすべて既定でエラー(permerror)になった。
# -fpermissive はこれを全部まとめて警告に格下げしてしまうため使わない。
#
#   実行時に本当に壊れるもの → エラーのまま維持する
#     implicit-function-declaration : 戻り値が int と仮定され u64/float/構造体で破綻する
#     implicit-int / return-type    : 未定義の値が返る
#
#   ARM32 では実害が出にくいもの → 警告に落として目視で確認する
#     int-conversion / incompatible-pointer-types
#     （ポインタも整数も 32bit で同じレジスタに乗るため生成コードは変わらない。
#       2018 年の v9.1 ツリーには元から大量にあり、止めるとビルドが進まない）
HARDERRORS := -Werror=implicit-function-declaration -Werror=implicit-int \
              -Werror=return-type \
              -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
ifeq ($(strip $(STRICT)),1)
    WERROR := -Werror
else
    WERROR :=
endif
# ---------------------------------------------------------------------------

CFLAGS	:=	-g -std=gnu11 -Wall -Wextra $(WERROR) $(HARDERRORS) -Wno-unused-value -Os -mword-relocations \
			-fomit-frame-pointer -ffunction-sections -fdata-sections -fno-math-errno \
			$(FALSEPOSITIVES) $(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map),-wrap,exit,--section-start,.text=0x14000000

LIBS	:=	-lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(CTRULIB)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 		:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).cxi $(TARGET).elf


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all	:	$(OUTPUT).cxi

$(OUTPUT).cxi	:	$(OUTPUT).elf $(OUTPUT).rsf
	@makerom -f ncch -rsf $(word 2,$^) -nocodepadding -o $@ -elf $<
	@echo built ... $(notdir $@)

$(OUTPUT).elf	:	$(OFILES)

%.elf: $(OFILES)
	@echo linking $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	@$(NM) -CSn $@ > $(notdir $*.lst)

draw.o: CFLAGS += -O3

$(OFILES_SRC)	: $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
#---------------------------------------------------------------------------------
%.xml.o	%_xml.h:	%.xml
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
#---------------------------------------------------------------------------------
%.ini.o	%_ini.h:	%.ini
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
