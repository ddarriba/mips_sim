#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct SDL_Window;

namespace mips_sim {
  class Memory;
  class Cpu;
}

class MipsSimImGui {
public:
  MipsSimImGui();
  ~MipsSimImGui();
  void run();

private:
  void render();
  void setup_default_layout(unsigned int dockspace_id);
  void render_registers();
  void render_pipeline();
  void render_datapath();
  void render_text_memory();
  void render_data_memory();
  void render_settings();
  void render_exception_popup();

  bool load_file(const std::string& path);
  void refresh_instructions();
  void do_next();
  void do_prev();
  void do_run();
  void do_reset();

  SDL_Window* window;
  void*       gl_context; /* SDL_GLContext is void* */

  std::shared_ptr<mips_sim::Memory> mem;
  std::unique_ptr<mips_sim::Cpu>    cpu;

  struct InstrInfo {
    uint32_t    pc;
    std::string text;
  };
  std::vector<InstrInfo> instructions;

  bool file_loaded    = false;
  bool show_settings  = false;
  bool show_exception = false;
  bool first_frame    = true;

  std::string exception_msg;
  std::string asm_filename;

  std::map<std::string, int> pending_settings;
};
