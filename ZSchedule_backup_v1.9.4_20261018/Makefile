# ZSchedule 构建脚本 (GNU Make + MinGW-w64)
#
#   make            编译
#   make clean      清理
#   make run        编译并运行
#
# 若使用 MSVC，请改用 build.bat

CC      := gcc
WINDRES := windres

SRC     := main.c model.c image.c richtext.c weekview.c imgview.c stickies.c noteview.c paper.c ui_main.c ui_edit.c csvimport.c sound.c
OBJ     := $(SRC:.c=.o)
RES     := zsched_res.o
TARGET  := 笺.exe

CFLAGS  := -O2 -Wall -Wextra -DUNICODE -D_UNICODE
LDFLAGS := -mwindows
LIBS    := -lcomctl32 -lgdi32 -lcomdlg32 -lshell32 -lmsimg32 -lgdiplus -lole32 -lshlwapi -luser32 -lwinmm -lmfplat -lmf -lmfuuid

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ) $(RES)
	$(CC) $(OBJ) $(RES) -o $@ $(LDFLAGS) $(LIBS)

%.o: %.c zsched.h
	$(CC) $(CFLAGS) -c $< -o $@

$(RES): zsched.rc zsched.manifest
	$(WINDRES) -i zsched.rc -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(RES) $(TARGET)
