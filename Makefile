DEBUG = 0

TARGET = apollo-ps2.elf
TARGET_RELEASE = apollo.elf

ifeq ($(DEBUG), 1)
   OPTIMIZE_LV	:= -O0 -g
   RARCH_DEFINES += -DDEBUG
else
   OPTIMIZE_LV	:= -O3
   LDFLAGS :=  -s
endif

INCDIR = -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/freetype2 -I./include
CFLAGS = $(OPTIMIZE_LV) -ffast-math -fsingle-precision-constant
ASFLAGS = $(CFLAGS)

CFLAGS += -D__PS2__ -Dmain=SDL_main -DHAVE_SDL2

LDFLAGS += -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -L.
LIBS += -lSDL2main -lSDL2 -lnetman -lcurl -lps2ip -lpatches -lpad -lgskit -ldmakit -lps2_drivers -lpolarssl -lfreetype -lpng -lz -lzip -lapollo -ldbglogger

SRC = $(wildcard ./source/*.c)
#EXCLUDE = 
SRC := $(filter-out $(EXCLUDE),$(SRC))
OBJS = $(SRC:.c=.o)
OBJS += $(wildcard ./data/*.o)

EE_OBJS += $(OBJS)
EE_CFLAGS = $(CFLAGS)
EE_CXXFLAGS = $(CFLAGS)
EE_LDFLAGS = $(LDFLAGS)
EE_LIBS = $(LIBS)
EE_ASFLAGS = $(ASFLAGS)
EE_INCS = $(INCDIR)
EE_BIN = $(TARGET)
EE_GPVAL = $(GPVAL)

all: $(EE_BIN)

modules:
	@$(EE_LD) -r -b binary -o	data/bgimg.png.o	data/bgimg.png
	@$(EE_LD) -r -b binary -o	data/buk_scr.png.o	data/buk_scr.png
	@$(EE_LD) -r -b binary -o	data/cat_about.png.o	data/cat_about.png
	@$(EE_LD) -r -b binary -o	data/cat_bup.png.o	data/cat_bup.png
	@$(EE_LD) -r -b binary -o	data/cat_cheats.png.o	data/cat_cheats.png
	@$(EE_LD) -r -b binary -o	data/cat_db.png.o	data/cat_db.png
	@$(EE_LD) -r -b binary -o	data/cat_empty.png.o	data/cat_empty.png
	@$(EE_LD) -r -b binary -o	data/cat_hdd.png.o	data/cat_hdd.png
	@$(EE_LD) -r -b binary -o	data/cat_opt.png.o	data/cat_opt.png
	@$(EE_LD) -r -b binary -o	data/cat_sav.png.o	data/cat_sav.png
	@$(EE_LD) -r -b binary -o	data/cat_usb.png.o	data/cat_usb.png
	@$(EE_LD) -r -b binary -o	data/cat_warning.png.o	data/cat_warning.png
	@$(EE_LD) -r -b binary -o	data/cheat.png.o	data/cheat.png
	@$(EE_LD) -r -b binary -o	data/circle_loading_bg.png.o	data/circle_loading_bg.png
	@$(EE_LD) -r -b binary -o	data/circle_loading_seek.png.o	data/circle_loading_seek.png
	@$(EE_LD) -r -b binary -o	data/column_1.png.o	data/column_1.png
	@$(EE_LD) -r -b binary -o	data/column_2.png.o	data/column_2.png
	@$(EE_LD) -r -b binary -o	data/column_3.png.o	data/column_3.png
	@$(EE_LD) -r -b binary -o	data/column_4.png.o	data/column_4.png
	@$(EE_LD) -r -b binary -o	data/column_5.png.o	data/column_5.png
	@$(EE_LD) -r -b binary -o	data/column_6.png.o	data/column_6.png
	@$(EE_LD) -r -b binary -o	data/column_7.png.o	data/column_7.png
	@$(EE_LD) -r -b binary -o	data/edit_shadow.png.o	data/edit_shadow.png
	@$(EE_LD) -r -b binary -o	data/footer_ico_circle.png.o	data/footer_ico_circle.png
	@$(EE_LD) -r -b binary -o	data/footer_ico_cross.png.o	data/footer_ico_cross.png
	@$(EE_LD) -r -b binary -o	data/footer_ico_square.png.o	data/footer_ico_square.png
	@$(EE_LD) -r -b binary -o	data/footer_ico_triangle.png.o	data/footer_ico_triangle.png
	@$(EE_LD) -r -b binary -o	data/header_dot.png.o	data/header_dot.png
	@$(EE_LD) -r -b binary -o	data/header_line.png.o	data/header_line.png
	@$(EE_LD) -r -b binary -o	data/help.png.o	data/help.png
	@$(EE_LD) -r -b binary -o	data/jar_about.png.o	data/jar_about.png
	@$(EE_LD) -r -b binary -o	data/jar_about_hover.png.o	data/jar_about_hover.png
	@$(EE_LD) -r -b binary -o	data/jar_bup.png.o	data/jar_bup.png
	@$(EE_LD) -r -b binary -o	data/jar_bup_hover.png.o	data/jar_bup_hover.png
	@$(EE_LD) -r -b binary -o	data/jar_db.png.o	data/jar_db.png
	@$(EE_LD) -r -b binary -o	data/jar_db_hover.png.o	data/jar_db_hover.png
	@$(EE_LD) -r -b binary -o	data/jar_empty.png.o	data/jar_empty.png
	@$(EE_LD) -r -b binary -o	data/jar_hdd.png.o	data/jar_hdd.png
	@$(EE_LD) -r -b binary -o	data/jar_hdd_hover.png.o	data/jar_hdd_hover.png
	@$(EE_LD) -r -b binary -o	data/jar_opt.png.o	data/jar_opt.png
	@$(EE_LD) -r -b binary -o	data/jar_opt_hover.png.o	data/jar_opt_hover.png
	@$(EE_LD) -r -b binary -o	data/jar_usb.png.o	data/jar_usb.png
	@$(EE_LD) -r -b binary -o	data/jar_usb_hover.png.o	data/jar_usb_hover.png
	@$(EE_LD) -r -b binary -o	data/logo.png.o	data/logo.png
	@$(EE_LD) -r -b binary -o	data/logo_text.png.o	data/logo_text.png
	@$(EE_LD) -r -b binary -o	data/mark_arrow.png.o	data/mark_arrow.png
	@$(EE_LD) -r -b binary -o	data/mark_line.png.o	data/mark_line.png
	@$(EE_LD) -r -b binary -o	data/opt_off.png.o	data/opt_off.png
	@$(EE_LD) -r -b binary -o	data/opt_on.png.o	data/opt_on.png
	@$(EE_LD) -r -b binary -o	data/scroll_bg.png.o	data/scroll_bg.png
	@$(EE_LD) -r -b binary -o	data/scroll_lock.png.o	data/scroll_lock.png
	@$(EE_LD) -r -b binary -o	data/tag_apply.png.o	data/tag_apply.png
	@$(EE_LD) -r -b binary -o	data/tag_lock.png.o	data/tag_lock.png
	@$(EE_LD) -r -b binary -o	data/tag_net.png.o	data/tag_net.png
	@$(EE_LD) -r -b binary -o	data/tag_own.png.o	data/tag_own.png
	@$(EE_LD) -r -b binary -o	data/tag_pce.png.o	data/tag_pce.png
	@$(EE_LD) -r -b binary -o	data/tag_ps1.png.o	data/tag_ps1.png
	@$(EE_LD) -r -b binary -o	data/tag_ps2.png.o	data/tag_ps2.png
	@$(EE_LD) -r -b binary -o	data/tag_psp.png.o	data/tag_psp.png
	@$(EE_LD) -r -b binary -o	data/tag_transfer.png.o	data/tag_transfer.png
	@$(EE_LD) -r -b binary -o	data/tag_warning.png.o	data/tag_warning.png
	@$(EE_LD) -r -b binary -o	data/tag_zip.png.o	data/tag_zip.png
	@$(EE_LD) -r -b binary -o	data/trp_gold.png.o	data/trp_gold.png
	@$(EE_LD) -r -b binary -o	data/trp_sync.png.o	data/trp_sync.png

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

release: all
	ps2-packer-lite $(EE_BIN) $(TARGET_RELEASE)

#Include preferences
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
