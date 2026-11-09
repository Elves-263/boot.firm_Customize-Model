# Luma3DS_New の更新アプリ（.cia）
#
#   make CTRULIB=<libctru> APP_VERSION=v1.5 UPDATE_REPO=所有者/名前 LUMA_NEW_VERSION=13.5 \
#        FIRM_NEW=<新しい方の boot.firm> FIRM_91=<v9.1 の boot.firm>
#
# 普通は build.bat から呼ぶ（libctru は build.bat が .libctru に用意したものを使う）。
# 作るもの: Luma3DS_New_Updater.cia（と .elf）。makerom が要る。
# FIRM_NEW・FIRM_91 の boot.firm（後ろにフォントを付けたもの）をアプリの中（romfs）に入れる。
# bannertool が PATH にあれば、HOME メニューの上の画面に出す絵（banner.png と banner.wav）も入れる。
# 無ければ絵なしで作る（動きは同じ）。

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(strip $(CTRULIB)),)
$(error "CTRULIB を指定してください（build.bat から呼ぶと .libctru の libctru を使う）")
endif
ifeq ($(and $(strip $(FIRM_NEW)),$(strip $(FIRM_91))),)
$(error "FIRM_NEW と FIRM_91 を指定してください（build.bat から呼ぶと、新しい方と v9.1 の boot.firm を使う）")
endif
endif

include $(DEVKITARM)/3ds_rules

TARGET      := Luma3DS_New_Updater
BUILD       := build
SOURCES     := source source/bearssl source/qrcodegen
FONT        := data/ui_font.bin

APP_VERSION ?= v0
UPDATE_REPO ?=
# 新しい方の Luma の上流の版（画面に出すだけ）。Luma3DS_new/UPSTREAM_VERSION に書いてある
LUMA_NEW_VERSION ?= $(patsubst v%,%,$(strip $(shell cat "$(CURDIR)/../Luma3DS_new/UPSTREAM_VERSION")))

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -std=gnu11 -Wall -Wextra -O2 -mword-relocations -ffunction-sections -fdata-sections $(ARCH) \
               -D__3DS__ -DAPP_VERSION="\"$(APP_VERSION)\"" -DUPDATE_REPO="\"$(UPDATE_REPO)\"" -DLUMA_NEW_VERSION="\"$(LUMA_NEW_VERSION)\"" \
               -I$(CTRULIB)/include $(foreach dir,$(SOURCES),-I$(dir)) -I$(BUILD)
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS        := -L$(CTRULIB)/lib -lctru -lm

CFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES      := $(patsubst %.c,$(BUILD)/%.o,$(CFILES)) $(BUILD)/ui_font.bin.o

BANNERTOOL  := $(shell command -v bannertool 2>/dev/null)
ifneq ($(BANNERTOOL),)
BANNER      := $(BUILD)/banner.bnr
BANNERFLAGS := -banner $(BANNER)
endif

.PHONY: all clean

all: $(TARGET).cia

# 画面の字（tools/mkfont_ui.py で Noto Sans CJK JP から作ったもの）
$(BUILD)/ui_font.bin.o $(BUILD)/ui_font_bin.h: $(FONT)
	@mkdir -p $(BUILD)
	@cp $(FONT) $(BUILD)/ui_font.bin
	@cd $(BUILD) && bin2s -a 4 -H ui_font_bin.h ui_font.bin | $(AS) -o ui_font.bin.o

$(BUILD)/source/screen.o: $(BUILD)/ui_font_bin.h

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	@echo $(notdir $<)
	@$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	@$(CC) $(LDFLAGS) $(OFILES) $(LIBS) -o $@
	@echo built ... $(notdir $@)

$(BUILD)/banner.bnr: banner.png banner.wav
	@mkdir -p $(BUILD)
	@bannertool makebanner -i banner.png -a banner.wav -o $@

# アプリに入れる boot.firm
$(BUILD)/romfs/new.firm: $(FIRM_NEW)
	@mkdir -p $(@D)
	@cp $< $@

$(BUILD)/romfs/9.1.firm: $(FIRM_91)
	@mkdir -p $(@D)
	@cp $< $@

$(TARGET).cia: $(TARGET).elf updater.rsf icon.smdh $(BANNER) $(BUILD)/romfs/new.firm $(BUILD)/romfs/9.1.firm
	@makerom -f cia -o $@ -elf $(TARGET).elf -rsf updater.rsf -icon icon.smdh $(BANNERFLAGS) -exefslogo -target t
	@echo built ... $(notdir $@)

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).cia

-include $(OFILES:.o=.d)
