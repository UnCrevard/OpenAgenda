# MinGW Makefile for AgendaWin32 (pure C Win32 Unicode, zero external dependencies)

CC       := gcc
WINDRES  := windres

SRC_DIR  := src
OBJ_DIR  := obj
TARGET   := OpenAgenda.exe

CFLAGS   := -Wall -Wextra -Wshadow -O2 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DCOBJMACROS -I$(SRC_DIR)
LDFLAGS  := -s -municode -mwindows
LIBS     := -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lwinmm -lshlwapi

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/calendar_view.c \
	$(SRC_DIR)/dialogs.c \
	$(SRC_DIR)/themed_ctrls.c \
	$(SRC_DIR)/date_utils.c \
	$(SRC_DIR)/recurrence.c \
	$(SRC_DIR)/settings.c \
	$(SRC_DIR)/storage.c \
	$(SRC_DIR)/theme.c \
	$(SRC_DIR)/notify.c

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
RES_OBJ := $(OBJ_DIR)/resource.o

.PHONY: all clean

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
else
	@mkdir -p "$(OBJ_DIR)"
endif

$(TARGET): $(OBJS) $(RES_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

HDRS := $(wildcard $(SRC_DIR)/*.h)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(RES_OBJ): $(SRC_DIR)/resource.rc $(SRC_DIR)/app.manifest $(SRC_DIR)/agenda.ico | $(OBJ_DIR)
	$(WINDRES) -I $(SRC_DIR) -i $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(OBJ_DIR)\*.o" del /Q "$(OBJ_DIR)\*.o"
	@if exist "$(TARGET)" del /Q "$(TARGET)"
else
	@rm -f $(OBJ_DIR)/*.o $(TARGET)
endif
