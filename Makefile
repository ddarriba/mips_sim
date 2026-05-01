CC := g++

INSTALLDIR = $(PWD)

CPPFLAGS = -g -O3 -Wall -Wno-padded -std=c++17 -m64 -pipe

IMGUI_DIR = third_party/imgui

IMGUI_CPPFLAGS = $(CPPFLAGS) \
                 -I$(IMGUI_DIR) \
                 -I$(IMGUI_DIR)/backends \
                 -Ithird_party \
                 $(shell pkg-config --cflags sdl2)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  IMGUI_LIBS = $(shell pkg-config --libs sdl2) -framework OpenGL -framework Cocoa
else
  IMGUI_LIBS = $(shell pkg-config --libs sdl2) -lGL -ldl
endif

IMGUI_OBJ = \
  $(IMGUI_DIR)/imgui.o \
  $(IMGUI_DIR)/imgui_draw.o \
  $(IMGUI_DIR)/imgui_tables.o \
  $(IMGUI_DIR)/imgui_widgets.o \
  $(IMGUI_DIR)/backends/imgui_impl_sdl2.o \
  $(IMGUI_DIR)/backends/imgui_impl_opengl3.o \
  third_party/ImGuiFileDialog.o \
  src/interface/imgui/mips_sim_imgui.o

SIM_OBJ = \
  src/assembler/mips_parser.o \
  src/assembler/mips_scanner.o \
  src/cpu/component/alu.o \
  src/cpu/component/control_unit.o \
  src/cpu/component/registers_bank.o \
  src/cpu/cpu.o \
  src/cpu/cpu_multi.o \
  src/cpu/cpu_pipelined.o \
  src/interface/mipscli.o \
  src/mem.o \
  src/utils.o \
  src/mips_sim.o

BISONFLAGS = -Wall -d
FLEXFLAGS  =


all: $(SIM_OBJ) $(IMGUI_OBJ)
	$(CC) $(CPPFLAGS) -o mips_sim $(SIM_OBJ) $(IMGUI_OBJ) $(IMGUI_LIBS)
	@echo $(INSTALLDIR)

src/assembler/mips_parser.cpp: src/assembler/mips_assembler.y
	bison $(BISONFLAGS) -o src/assembler/mips_parser.cpp src/assembler/mips_assembler.y

src/assembler/mips_scanner.cpp: src/assembler/mips_assembler.l
	flex $(FLEXFLAGS) -o src/assembler/mips_scanner.cpp src/assembler/mips_assembler.l

src/assembler/%.o: src/assembler/%.cpp
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) -c -o $@ $<

# ImGui core files
$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CC) $(IMGUI_CPPFLAGS) -c -o $@ $<

# ImGui SDL2/OpenGL backend files
$(IMGUI_DIR)/backends/%.o: $(IMGUI_DIR)/backends/%.cpp
	$(CC) $(IMGUI_CPPFLAGS) -c -o $@ $<

# ImGuiFileDialog (lives in third_party/)
third_party/ImGuiFileDialog.o: third_party/ImGuiFileDialog.cpp
	$(CC) $(IMGUI_CPPFLAGS) -c -o $@ $<

# New ImGui GUI interface
src/interface/imgui/%.o: src/interface/imgui/%.cpp
	@mkdir -p "$(@D)"
	$(CC) -D_IMGUI $(IMGUI_CPPFLAGS) -c -o $@ $<

# Main entry point needs _IMGUI flag and ImGui includes
src/mips_sim.o: src/mips_sim.cpp
	@mkdir -p "$(@D)"
	$(CC) -D_IMGUI $(IMGUI_CPPFLAGS) -c -o $@ $<

src/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) -c -o $@ $<

clean:
	find src -name "*.o" | xargs rm -f
	find $(IMGUI_DIR) -name "*.o" 2>/dev/null | xargs rm -f
	rm -f third_party/ImGuiFileDialog.o
	rm -f src/assembler/mips_scanner.cpp src/assembler/mips_parser.cpp

parser: src/assembler/mips_parser.cpp src/assembler/mips_scanner.cpp
