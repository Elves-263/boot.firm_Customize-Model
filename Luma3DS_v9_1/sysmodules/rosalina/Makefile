rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

name := $(shell basename $(CURDIR))

dir_source := source
dir_include := include
dir_build := build
dir_out := ../../$(dir_build)

LIBS := -lctru
LIBDIRS	:= $(CTRULIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

INCLUDE := -I$(CURDIR)/shim $(foreach dir,$(LIBDIRS),-I$(dir)/include)

ARCH := -mcpu=mpcore -mfloat-abi=hard
ASFLAGS := -g $(ARCH)
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

# 起動時に新しい版を確かめる GitHub のリポジトリ（所有者/名前）。空なら確かめない（build.bat で設定する）
UPDATE_REPO ?=
# 新しい方の Luma の版（13.5 など）。Luma3DS_new/UPSTREAM_VERSION に書いてある（build.bat も同じものを渡す）
LUMA_NEW_VERSION ?= $(patsubst v%,%,$(strip $(shell cat "$(CURDIR)/../../../Luma3DS_new/UPSTREAM_VERSION")))

# TR()（include/menus/lang.h）は日本語と英語を \0 でつないだ 1 つの文字列なので、書式の検査は日本語のほうだけになる
# （-Wno-format-contains-nul）。英語のほうの書式は font_jp/chk_tr.py で日本語のほうと同じか確かめる
CFLAGS := -Wall -Wextra $(WERROR) $(HARDERRORS) -MMD -MP -marm $(ASFLAGS) -mtp=soft -fno-builtin -std=c11 -O2 -ffast-math -mword-relocations \
		-fomit-frame-pointer -ffunction-sections -fdata-sections $(INCLUDE) -I$(dir_include) -I$(dir_source)/bearssl -DARM11 -D_3DS \
		-DUPDATE_REPO="\"$(UPDATE_REPO)\"" -DLUMA_NEW_VERSION="\"$(LUMA_NEW_VERSION)\"" -Wno-format-contains-nul
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -mtp=soft -Wl,--section-start,.text=0x14000000 -Wl,--gc-sections

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
		$(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
		$(call rwildcard, $(dir_source), *.s *.c)))

xml_files = $(call rwildcard, $(dir_source), *.xml)

.PHONY: all
all: $(dir_out)/$(name).cxi

.PHONY: clean
clean:
	@rm -rf $(dir_build)

$(dir_out)/$(name).cxi: $(dir_build)/$(name).elf
	@makerom -f ncch -rsf rosalina.rsf -nocodepadding -o $@ -elf $<

$(dir_build)/$(name).elf: $(objects)
	$(LINK.o) $(OUTPUT_OPTION) $^ $(LIBPATHS) $(LIBS)

$(dir_build)/xml_data.h: $(xml_files)
	@ echo "" > $(@)
	@$(foreach f, $(xml_files),\
	echo "static const char" `(echo $(notdir $(f)) | sed -e 's/^\([0-9]\)/_\1/g' | tr . _)`"[] = " >> $(@);\
	sed -e 's/\\/\\\\/g;s/"/\\"/g;s/^/"/g;s/[ \t\r\n]*$$/\\n"/g' $(f) >> $(@);\
	echo ';' >> $@;\
	)
$(dir_build)/gdb/xfer.o: $(dir_build)/xml_data.h
$(dir_build)/memory.o : CFLAGS += -O3
# BearSSL（TLS）は大きいので、速さより大きさを優先する
$(dir_build)/bearssl/%.o : CFLAGS += -Os

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
