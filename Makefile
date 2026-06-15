CC := gcc
PKG_CONFIG ?= pkg-config
CFLAGS := -Wall -Wextra -Wno-format-truncation -std=c99 -g -Iinclude $(shell $(PKG_CONFIG) --cflags libpng)
DEPFLAGS := -MMD -MP
LDLIBS := $(shell $(PKG_CONFIG) --libs libpng) -lm
RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib)
RAYLIB_LDLIBS := $(shell $(PKG_CONFIG) --libs raylib)

BUILD_DIR := build
TARGET := AnimaEngine
GUI_TARGET := AnimaEngineGUI
TEST_TARGET := $(BUILD_DIR)/anima_tests

COMMON_SRC := $(filter-out source/main.c source/gui_main.c source/gui_app.c source/pokemon_catalog.c source/gui_raylib.c source/gui_state.c source/gui_widgets.c source/gui_view_rom.c source/gui_view_browser.c,$(wildcard source/*.c))
CLI_SRC := $(COMMON_SRC) source/pokemon_catalog.c source/main.c
GUI_SRC := $(COMMON_SRC) source/pokemon_catalog.c source/gui_raylib.c source/gui_state.c source/gui_widgets.c source/gui_view_rom.c source/gui_view_browser.c source/gui_app.c source/gui_main.c

TEST_SRC := tests/test_main.c tests/test_lz.c tests/test_pokemon_catalog.c
TEST_OBJ := $(patsubst tests/%.c,$(BUILD_DIR)/%.test.o,$(TEST_SRC))
TEST_DEPS := $(TEST_OBJ:.o=.d)
TEST_LIB_SRC := $(COMMON_SRC) source/pokemon_catalog.c
TEST_LIB_OBJ := $(patsubst source/%.c,$(BUILD_DIR)/%.test.o,$(TEST_LIB_SRC))

CLI_OBJ := $(patsubst source/%.c,$(BUILD_DIR)/%.o,$(CLI_SRC))
GUI_OBJ := $(patsubst source/%.c,$(BUILD_DIR)/%.gui.o,$(GUI_SRC))
CLI_DEPS := $(CLI_OBJ:.o=.d)
GUI_DEPS := $(GUI_OBJ:.o=.d)

.PHONY: all gui test docs clean release-clean

all: $(TARGET)

$(TARGET): $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJ) $(LDLIBS)

gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_OBJ)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o $@ $(GUI_OBJ) $(LDLIBS) $(RAYLIB_LDLIBS)

test: $(TEST_TARGET)
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_LIB_OBJ) $(TEST_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TEST_LIB_OBJ) $(TEST_OBJ) $(LDLIBS)

$(BUILD_DIR)/%.o: source/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.gui.o: source/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.test.o: source/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.test.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude -Itests $(DEPFLAGS) -c $< -o $@

docs:
	doxygen docs/Doxyfile

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(GUI_TARGET) $(TEST_TARGET)

release-clean: clean
	rm -rf release docs/build

-include $(CLI_DEPS) $(GUI_DEPS) $(TEST_DEPS)
