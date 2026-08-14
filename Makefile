#!/usr/bin/make -f

ifneq (,)
This makefile requires GNU Make.
endif

CC ?= cc
RM ?= rm -f
INSTALL ?= install
LN ?= ln -sfn

CFLAGS ?= -O3 -Wall -Wextra -Wpedantic
LDFLAGS ?=
PREFIX ?= /usr/local
BUILD_DIR ?= build

SRCS = $(wildcard *.c)
CMD_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.cmd.o,$(SRCS))
COMMAND_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.COMMAND.o,$(SRCS))

.PHONY: all
all: $(BUILD_DIR)/cmd.exe $(BUILD_DIR)/COMMAND.COM

$(BUILD_DIR)/%.cmd.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cmd.exe : $(CMD_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/%.COMMAND.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ -DENABLE_AUTOEXEC=1

$(BUILD_DIR)/COMMAND.COM: $(COMMAND_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)

.PHONY: install
install: all
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 755 $(BUILD_DIR)/cmd.exe $(PREFIX)/bin/
	$(INSTALL) -m 755 $(BUILD_DIR)/COMMAND.COM $(PREFIX)/bin/
	$(LN) $(PREFIX)/bin/cmd.exe $(PREFIX)/bin/cmd
	@echo "Successfully installed to $(PREFIX)"
	@echo "You may want to copy AUTOEXEC.BAT to your root directory so that COMMAND.COM is automatically executed when you start COMMAND.COM."
