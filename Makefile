PLANKAC_ROOT ?= ../PlankaMath
CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99
CPPFLAGS += -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -I$(PLANKAC_ROOT)/c/include
LDLIBS ?= -lm
BUILD := build

.PHONY: all check clean

all: $(BUILD)/plankastarten_cli

$(BUILD):
	mkdir -p $(BUILD)

$(PLANKAC_ROOT)/build/libplankac.a:
	$(MAKE) -C $(PLANKAC_ROOT) all

$(BUILD)/plankastarten_cli: src/plankastarten_cli.c src/plankastarten_compile.c $(PLANKAC_ROOT)/build/libplankac.a | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/plankastarten_cli.c src/plankastarten_compile.c $(PLANKAC_ROOT)/build/libplankac.a -o $@ $(LDLIBS)

check: all
	$(BUILD)/plankastarten_cli check examples/max3.plk
	$(BUILD)/plankastarten_cli run examples/max3.plk start
	$(BUILD)/plankastarten_cli evidence examples/max3.plk $(BUILD)/max3.evidence.json
	grep -q "plankac-evidence-v1" $(BUILD)/max3.evidence.json

clean:
	rm -rf $(BUILD)
