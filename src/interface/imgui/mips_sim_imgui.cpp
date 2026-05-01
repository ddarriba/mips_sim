#include "mips_sim_imgui.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "ImGuiFileDialog.h"
#include "../../assembler/mips_assembler.h"
#include "../../cpu/cpu_pipelined.h"
#include "../../mem.h"
#include "../../utils.h"
#include "../../exception.h"
#include "../../global_defs.h"

#include <iostream>
#include <cstring>
#include <cassert>
#include <cmath>

using namespace mips_sim;

static constexpr int MAX_COLORS = 5;
static const ImVec4 PIPELINE_COLORS[MAX_COLORS] = {
  { 1.00f, 0.75f, 0.74f, 1.0f }, /* #FFBEBC pink       */
  { 0.52f, 0.89f, 1.00f, 1.0f }, /* #85E3FF light blue */
  { 0.69f, 0.97f, 0.85f, 1.0f }, /* #AFF8D8 mint       */
  { 1.00f, 1.00f, 0.82f, 1.0f }, /* #FFFFD1 yellow     */
  { 1.00f, 0.80f, 0.98f, 1.0f }, /* #FFCCF9 lavender   */
};
static const ImVec4 STALL_COLOR = { 1.0f, 1.0f, 0.0f, 1.0f };
static const ImVec4 TEXT_DARK   = { 0.0f, 0.0f, 0.0f, 1.0f };

/* ------------------------------------------------------------------ */

MipsSimImGui::MipsSimImGui() {
  mem = std::make_shared<Memory>();
  cpu = std::make_unique<CpuPipelined>(mem);
  cpu->print_status(std::cout);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::cerr << "SDL_Init: " << SDL_GetError() << '\n';
    exit(1);
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  window = SDL_CreateWindow("MIPS Simulator",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800,
    (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
  if (!window) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << '\n'; exit(1); }

  gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, (SDL_GLContext)gl_context);
  SDL_GL_SetSwapInterval(1);

  /* Enable file drag-and-drop */
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window, (SDL_GLContext)gl_context);
#ifdef __APPLE__
  ImGui_ImplOpenGL3_Init("#version 150");
#else
  ImGui_ImplOpenGL3_Init("#version 130");
#endif
}

MipsSimImGui::~MipsSimImGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext((SDL_GLContext)gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::run() {
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window))
        running = false;
      if (event.type == SDL_DROPFILE) {
        load_file(std::string(event.drop.file));
        SDL_free(event.drop.file);
      }
      if (event.type == SDL_MOUSEWHEEL) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
          io.FontGlobalScale = ImClamp(io.FontGlobalScale + event.wheel.y * 0.05f, 0.5f, 3.0f);
        }
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    render();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::setup_default_layout(unsigned int dockspace_id) {
  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, vp->Size);

  ImGuiID left, right;
  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.28f, &left, &right);

  ImGuiID right_top, right_bot;
  ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.60f, &right_top, &right_bot);

  ImGuiID bot_left, bot_right;
  ImGui::DockBuilderSplitNode(right_bot, ImGuiDir_Left, 0.50f, &bot_left, &bot_right);

  ImGui::DockBuilderDockWindow("Registers",        left);
  ImGui::DockBuilderDockWindow("Pipeline Diagram", right_top);
  ImGui::DockBuilderDockWindow("Datapath",         right_top);
  ImGui::DockBuilderDockWindow("Text Memory",      bot_left);
  ImGui::DockBuilderDockWindow("Data Memory",      bot_right);
  ImGui::DockBuilderFinish(dockspace_id);
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render() {
  if (ImGuiFileDialog::Instance()->Display("LoadFileDlg",
      ImGuiWindowFlags_NoCollapse, ImVec2(700, 450))) {
    if (ImGuiFileDialog::Instance()->IsOk())
      load_file(ImGuiFileDialog::Instance()->GetFilePathName());
    ImGuiFileDialog::Instance()->Close();
  }

  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  ImGui::SetNextWindowViewport(vp->ID);

  constexpr ImGuiWindowFlags host_flags =
    ImGuiWindowFlags_MenuBar         |
    ImGuiWindowFlags_NoDocking       |
    ImGuiWindowFlags_NoTitleBar      |
    ImGuiWindowFlags_NoCollapse      |
    ImGuiWindowFlags_NoResize        |
    ImGuiWindowFlags_NoMove          |
    ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0, 0));
  ImGui::Begin("##DockHost", nullptr, host_flags);
  ImGui::PopStyleVar(3);

  ImGuiID dsid = ImGui::GetID("MainDockSpace");
  if (first_frame) {
    setup_default_layout(dsid);
    first_frame = false;
  }
  ImGui::DockSpace(dsid, ImVec2(0, 0));

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Load File...")) {
        IGFD::FileDialogConfig cfg;
        cfg.path  = ".";
        cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(
          "LoadFileDlg", "Open Assembly File", ".s,.asm", cfg);
      }
      ImGui::Separator();
      ImGui::BeginDisabled(!file_loaded);
      if (ImGui::MenuItem("Settings")) {
        pending_settings = cpu->get_status();
        show_settings = true;
      }
      ImGui::EndDisabled();
      ImGui::Separator();
      if (ImGui::MenuItem("Exit")) {
        SDL_Event e; e.type = SDL_QUIT;
        SDL_PushEvent(&e);
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!file_loaded);

    if (ImGui::SmallButton("Reset")) do_reset();
    ImGui::SameLine();
    ImGui::BeginDisabled(!file_loaded || cpu->get_cycle() == 0);
    if (ImGui::SmallButton("Prev"))  do_prev();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!file_loaded || !cpu->is_ready());
    if (ImGui::SmallButton("Next"))  do_next();
    ImGui::SameLine();
    if (ImGui::SmallButton("Run"))   do_run();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Text("Cycle: %u", cpu->get_cycle());

    ImGui::EndDisabled();
    ImGui::EndMenuBar();
  }

  ImGui::End();

  render_registers();
  render_pipeline();
  render_datapath();
  render_text_memory();
  render_data_memory();

  if (show_settings)  render_settings();
  if (show_exception) render_exception_popup();
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_registers() {
  if (!ImGui::Begin("Registers")) { ImGui::End(); return; }

  if (!file_loaded) {
    ImGui::TextDisabled("Load a file to see registers");
    ImGui::End();
    return;
  }

  constexpr ImGuiTableFlags tflags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV;

  float pad    = ImGui::GetStyle().CellPadding.x * 2.0f;
  float sname_w = ImGui::CalcTextSize("BadVAddr").x + pad;  /* widest special reg name */
  float val_w   = ImGui::CalcTextSize("0x00000000").x + pad;
  float ireg_w  = ImGui::CalcTextSize("$zero").x + pad;
  float fpreg_w = ImGui::CalcTextSize("$f00").x + pad;
  float fp_w    = ImGui::CalcTextSize("-000.00").x + pad;

  ImGui::SeparatorText("Special Registers");

  if (ImGui::BeginTable("##sregs", 4, tflags)) {
    ImGui::TableSetupColumn("##sn0", ImGuiTableColumnFlags_WidthFixed, sname_w);
    ImGui::TableSetupColumn("##sv0", ImGuiTableColumnFlags_WidthFixed, val_w);
    ImGui::TableSetupColumn("##sn1", ImGuiTableColumnFlags_WidthFixed, sname_w);
    ImGui::TableSetupColumn("##sv1", ImGuiTableColumnFlags_WidthFixed, val_w);

    auto sreg = [&](const char* n0, const char* k0, const char* n1, const char* k1) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", n0);
      ImGui::TableSetColumnIndex(1); ImGui::Text("0x%s", Utils::hex32(cpu->read_special_register(k0)).c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("%s", n1);
      ImGui::TableSetColumnIndex(3); ImGui::Text("0x%s", Utils::hex32(cpu->read_special_register(k1)).c_str());
    };

    sreg("PC",       SPECIAL_PC,       "HI",    SPECIAL_HI);
    sreg("LO",       SPECIAL_LO,       "EPC",   SPECIAL_EPC);
    sreg("BadVAddr", SPECIAL_BADVADDR, "Cause", SPECIAL_CAUSE);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Status");
    ImGui::TableSetColumnIndex(1); ImGui::Text("0x%s", Utils::hex32(cpu->read_special_register(SPECIAL_STATUS)).c_str());

    ImGui::EndTable();
  }

  ImGui::Separator();

  if (ImGui::BeginTabBar("##regtabs")) {

    if (ImGui::BeginTabItem("Integer")) {
      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (ImGui::BeginTable("##iregs", 8, tflags | ImGuiTableFlags_ScrollY, avail)) {
        for (int i = 0; i < 4; i++) {
          ImGui::TableSetupColumn("##n", ImGuiTableColumnFlags_WidthFixed, ireg_w);
          ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthFixed, val_w);
        }
        for (int row = 0; row < 8; row++) {
          ImGui::TableNextRow();
          for (int col = 0; col < 4; col++) {
            int idx = row + col * 8;
            ImGui::TableSetColumnIndex(col * 2);
            ImGui::TextDisabled("%s", Utils::get_register_name(idx).c_str());
            ImGui::TableSetColumnIndex(col * 2 + 1);
            ImGui::Text("0x%s", Utils::hex32(cpu->read_register(idx)).c_str());
          }
        }
        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("FP Single")) {
      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (ImGui::BeginTable("##fregs", 8, tflags | ImGuiTableFlags_ScrollY, avail)) {
        for (int i = 0; i < 4; i++) {
          ImGui::TableSetupColumn("##n", ImGuiTableColumnFlags_WidthFixed, fpreg_w);
          ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthFixed, fp_w);
        }
        for (int row = 0; row < 8; row++) {
          ImGui::TableNextRow();
          for (int col = 0; col < 4; col++) {
            int idx = row + col * 8;
            ImGui::TableSetColumnIndex(col * 2);
            ImGui::TextDisabled("%s", Utils::get_fp_register_name(idx).c_str());
            ImGui::TableSetColumnIndex(col * 2 + 1);
            ImGui::Text("%.2f", cpu->read_register_f(idx));
          }
        }
        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("FP Double")) {
      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (ImGui::BeginTable("##dregs", 8, tflags | ImGuiTableFlags_ScrollY, avail)) {
        for (int i = 0; i < 4; i++) {
          ImGui::TableSetupColumn("##n", ImGuiTableColumnFlags_WidthFixed, fpreg_w);
          ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthFixed, fp_w);
        }
        for (int row = 0; row < 4; row++) {
          ImGui::TableNextRow();
          for (int col = 0; col < 4; col++) {
            int idx = (row + col * 4) * 2;
            ImGui::TableSetColumnIndex(col * 2);
            ImGui::TextDisabled("%s", Utils::get_fp_register_name(idx).c_str());
            ImGui::TableSetColumnIndex(col * 2 + 1);
            ImGui::Text("%.2f", cpu->read_register_d(idx));
          }
        }
        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_pipeline() {
  if (!ImGui::Begin("Pipeline Diagram")) { ImGui::End(); return; }

  if (!file_loaded || cpu->get_cycle() == 0) {
    ImGui::TextDisabled("Step through the program to see the pipeline diagram");
    ImGui::End();
    return;
  }

  CpuPipelined& cpupipe = dynamic_cast<CpuPipelined&>(*cpu);
  const uint32_t* const* diagram = cpupipe.get_diagram();
  uint32_t cur = cpu->get_cycle();
  const auto& pcs = cpu->get_loaded_instructions();

  std::vector<size_t> vis;
  uint32_t min_cycle = cur;

  for (size_t i = 1; i <= pcs.size(); i++) {
    if (diagram[i][cur] > 0) {
      vis.push_back(i);
      uint32_t lmin = cur;
      for (size_t j = cur; j > 0 && diagram[i][j] > 0; j--)
        lmin--;
      if (lmin < min_cycle) min_cycle = lmin;
    }
  }

  if (vis.empty()) {
    ImGui::TextDisabled("No instructions currently in pipeline");
    ImGui::End();
    return;
  }

  uint32_t num_cycles = cur - min_cycle;
  int num_cols = 1 + (int)num_cycles;

  constexpr ImGuiTableFlags tflags =
    ImGuiTableFlags_Borders     |
    ImGuiTableFlags_ScrollX     |
    ImGuiTableFlags_ScrollY     |
    ImGuiTableFlags_RowBg;

  ImVec2 avail = ImGui::GetContentRegionAvail();
  if (ImGui::BeginTable("##pipeline", num_cols, tflags, avail)) {
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthFixed, 270.0f);
    for (uint32_t c = 0; c < num_cycles; c++) {
      char lbl[8];
      snprintf(lbl, sizeof(lbl), "%u", min_cycle + 1 + c);
      ImGui::TableSetupColumn(lbl, ImGuiTableColumnFlags_WidthFixed, 42.0f);
    }
    ImGui::TableHeadersRow();

    for (size_t row = 0; row < vis.size(); row++) {
      size_t iid = vis[row];
      ImVec4 color   = PIPELINE_COLORS[iid % MAX_COLORS];
      ImU32  color32 = ImGui::ColorConvertFloat4ToU32(color);
      ImU32  stall32 = ImGui::ColorConvertFloat4ToU32(STALL_COLOR);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color32);

      uint32_t pc = pcs[iid], word = 0;
      try { word = mem->mem_read_32(pc); } catch (int) {}
      std::string iname = "[" + Utils::hex32(pc) + "] " + Utils::decode_instruction(word);
      ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DARK);
      ImGui::TextUnformatted(iname.c_str());
      ImGui::PopStyleColor();

      for (uint32_t c = 0; c < num_cycles; c++) {
        uint32_t j = min_cycle + 1 + c;
        ImGui::TableSetColumnIndex((int)(c + 1));
        if (diagram[iid][j] == 0) continue;

        bool is_stall = (j > 0 && diagram[iid][j] == diagram[iid][j - 1]);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, is_stall ? stall32 : color32);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DARK);
        ImGui::TextUnformatted(is_stall ? "--" : stage_names[diagram[iid][j] - 1].c_str());
        ImGui::PopStyleColor();
      }
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_text_memory() {
  if (!ImGui::Begin("Text Memory")) { ImGui::End(); return; }

  if (!file_loaded || instructions.empty()) {
    ImGui::TextDisabled("Load a file to see instructions");
    ImGui::End();
    return;
  }

  /* build PC → pipeline colour map for instructions currently in-flight */
  std::map<uint32_t, ImU32> pipe_color;
  if (cpu->get_cycle() > 0) {
    CpuPipelined& cpupipe = dynamic_cast<CpuPipelined&>(*cpu);
    const uint32_t* const* diag = cpupipe.get_diagram();
    uint32_t cur = cpu->get_cycle();
    const auto& pcs = cpu->get_loaded_instructions();
    for (size_t i = 1; i < pcs.size(); i++) {
      uint32_t s = diag[i][cur];
      if (s == 0) continue;
      bool stall = (cur > 0 && diag[i][cur-1] == s);
      pipe_color[pcs[i]] = stall
        ? ImGui::ColorConvertFloat4ToU32(STALL_COLOR)
        : ImGui::ColorConvertFloat4ToU32(PIPELINE_COLORS[i % MAX_COLORS]);
    }
  }

  ImVec2 avail = ImGui::GetContentRegionAvail();
  if (ImGui::BeginTable("##tmem", 1,
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, avail)) {
    ImGui::TableSetupColumn("Instruction");

    for (const auto& instr : instructions) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);

      auto it = pipe_color.find(instr.pc);
      if (it != pipe_color.end()) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, it->second);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DARK);
        ImGui::TextUnformatted(instr.text.c_str());
        ImGui::PopStyleColor();
      } else {
        ImGui::TextUnformatted(instr.text.c_str());
      }
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_data_memory() {
  if (!ImGui::Begin("Data Memory")) { ImGui::End(); return; }

  if (!file_loaded) {
    ImGui::TextDisabled("Load a file to see data memory");
    ImGui::End();
    return;
  }

  ImVec2 avail = ImGui::GetContentRegionAvail();
  if (ImGui::BeginTable("##dmem", 1,
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, avail)) {
    ImGui::TableSetupColumn("Address / Values");

    for (uint32_t addr = MEM_DATA_START; addr < MEM_DATA_START + MEM_DATA_SIZE; addr += 16) {
      try {
        char buf[80];
        snprintf(buf, sizeof(buf), "[0x%08X] %08X %08X %08X %08X",
                 addr,
                 mem->mem_read_32(addr),
                 mem->mem_read_32(addr +  4),
                 mem->mem_read_32(addr +  8),
                 mem->mem_read_32(addr + 12));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(buf);
      } catch (int) { break; }
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_settings() {
  ImGui::SetNextWindowSize(ImVec2(360, 310), ImGuiCond_Always);
  ImGui::OpenPopup("Settings");

  if (ImGui::BeginPopupModal("Settings", &show_settings, ImGuiWindowFlags_NoResize)) {
    bool fw = (bool)pending_settings[KEY_FORWARDING_UNIT];
    bool hd = (bool)pending_settings[KEY_HAZARD_DETECTION_UNIT];
    int  bt = pending_settings[KEY_BRANCH_TYPE];
    int  bs = pending_settings[KEY_BRANCH_STAGE];

    if (ImGui::Checkbox("Forwarding Unit",       &fw)) pending_settings[KEY_FORWARDING_UNIT]       = fw;
    if (ImGui::Checkbox("Hazard Detection Unit", &hd)) pending_settings[KEY_HAZARD_DETECTION_UNIT] = hd;

    ImGui::SeparatorText("Branch Type");
    if (ImGui::RadioButton("Flush Pipeline",  bt == BRANCH_FLUSH))     pending_settings[KEY_BRANCH_TYPE] = BRANCH_FLUSH;
    if (ImGui::RadioButton("Fixed Non-Taken", bt == BRANCH_NON_TAKEN)) pending_settings[KEY_BRANCH_TYPE] = BRANCH_NON_TAKEN;
    if (ImGui::RadioButton("Delayed Branch",  bt == BRANCH_DELAYED))   pending_settings[KEY_BRANCH_TYPE] = BRANCH_DELAYED;

    ImGui::SeparatorText("Branch Evaluation Stage");
    if (ImGui::RadioButton("ID Stage",  bs == STAGE_ID))  pending_settings[KEY_BRANCH_STAGE] = STAGE_ID;
    if (ImGui::RadioButton("MEM Stage", bs == STAGE_MEM)) pending_settings[KEY_BRANCH_STAGE] = STAGE_MEM;

    ImGui::Separator();
    if (ImGui::Button("Apply", ImVec2(100, 0))) {
      if (cpu->set_status(pending_settings, true))
        cpu->reset(true, true);
      show_settings = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      show_settings = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_exception_popup() {
  ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
  ImGui::OpenPopup("Exception");

  if (ImGui::BeginPopupModal("Exception", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", exception_msg.c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      show_exception = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (!show_exception)
    ImGui::ClosePopupsOverWindow(ImGui::GetCurrentWindow(), false);
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::render_datapath() {
  if (!ImGui::Begin("Datapath")) { ImGui::End(); return; }

  if (!file_loaded || cpu->get_cycle() == 0) {
    ImGui::TextDisabled("Step through the program to see the datapath");
    ImGui::End();
    return;
  }

  CpuPipelined& cpupipe = dynamic_cast<CpuPipelined&>(*cpu);
  const uint32_t* const* diag = cpupipe.get_diagram();
  uint32_t cur = cpu->get_cycle();
  const auto& pcs = cpu->get_loaded_instructions();

  size_t stage_iid[5]   = {};
  bool   stage_stall[5] = {};
  for (size_t i = 1; i <= pcs.size(); i++) {
    uint32_t s = diag[i][cur];
    if (s >= 1 && s <= 5) {
      stage_iid[s-1]   = i;
      stage_stall[s-1] = (cur > 0 && diag[i][cur-1] == s);
    }
  }

  /* exact same fill as pipeline diagram cells */
  auto stage_bg = [&](int s) -> ImU32 {
    if (!stage_iid[s])  return IM_COL32(42,42,52,255);
    if (stage_stall[s]) return ImGui::ColorConvertFloat4ToU32(STALL_COLOR);
    return ImGui::ColorConvertFloat4ToU32(PIPELINE_COLORS[stage_iid[s] % MAX_COLORS]);
  };
  /* bright pipeline color — for wires drawn above/outside stage regions */
  auto stage_accent = [&](int s) -> ImU32 {
    if (!stage_iid[s])  return IM_COL32(100,100,110,255);
    if (stage_stall[s]) return IM_COL32(220,220,  0,255);
    return ImGui::ColorConvertFloat4ToU32(PIPELINE_COLORS[stage_iid[s] % MAX_COLORS]);
  };
  /* text / arrows drawn directly ON the stage background */
  auto on_bg = [&](int s) -> ImU32 {
    return stage_iid[s] ? IM_COL32(15,15,20,255) : IM_COL32(160,160,170,255);
  };
  /* component box / ellipse borders — must contrast the stage bg */
  auto comp_bdr = [&](int s) -> ImU32 {
    return stage_iid[s] ? IM_COL32(55,55,75,255) : IM_COL32(100,100,110,255);
  };
  auto get_instr = [&](size_t iid) -> std::string {
    if (!iid || iid > pcs.size()) return "";
    uint32_t w = 0;
    try { w = mem->mem_read_32(pcs[iid]); } catch (int) {}
    return Utils::decode_instruction(w);
  };

  ImGui::BeginChild("##dp", ImVec2(0,0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  ImVec2 O = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  constexpr float CW=1100.f, CH=490.f;
  ImGui::Dummy(ImVec2(CW, CH));

  auto P = [&](float x, float y) -> ImVec2 { return {O.x+x, O.y+y}; };

  constexpr ImU32 C_TEXT = IM_COL32(228,228,228,255);
  constexpr ImU32 C_PREG = IM_COL32(140, 80,220,255);
  constexpr float LW = 1.5f;

  auto draw_box = [&](float x0,float y0,float x1,float y1,
                      ImU32 fill, ImU32 bdr, const char* lbl) {
    dl->AddRectFilled(P(x0,y0), P(x1,y1), fill, 4.f);
    dl->AddRect(P(x0,y0), P(x1,y1), bdr, 4.f, 0, LW);
    if (lbl && *lbl) {
      ImVec2 ts = ImGui::CalcTextSize(lbl);
      dl->AddText(P((x0+x1)*.5f-ts.x*.5f,(y0+y1)*.5f-ts.y*.5f), C_TEXT, lbl);
    }
  };
  auto draw_ellipse = [&](float cx,float cy,float rx,float ry,
                          ImU32 fill, ImU32 bdr, const char* lbl) {
    dl->AddEllipseFilled(P(cx,cy), ImVec2(rx,ry), fill);
    dl->AddEllipse(P(cx,cy), ImVec2(rx,ry), bdr, 0.f, 0, LW);
    if (lbl && *lbl) {
      ImVec2 ts = ImGui::CalcTextSize(lbl);
      dl->AddText(P(cx-ts.x*.5f,cy-ts.y*.5f), C_TEXT, lbl);
    }
  };
  /* MUX: trapezoid — wide side = inputs (left), narrow = output (right) */
  auto draw_mux = [&](float cx, float cy, ImU32 fill, ImU32 bdr,
                      const char* lbl="M") {
    constexpr float W=10.f, HI=18.f, HO=10.f;
    ImVec2 pts[4] = {P(cx-W,cy-HI),P(cx+W,cy-HO),P(cx+W,cy+HO),P(cx-W,cy+HI)};
    dl->AddConvexPolyFilled(pts, 4, fill);
    dl->AddPolyline(pts, 4, bdr, ImDrawFlags_Closed, LW);
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText(P(cx-ts.x*.5f,cy-ts.y*.5f), C_TEXT, lbl);
  };
  /* ALU: wide trapezoid — input = left, output = right */
  auto draw_alu = [&](float cx, float cy, ImU32 fill, ImU32 bdr) {
    constexpr float W=38.f, HI=58.f, HO=30.f;
    ImVec2 pts[4] = {P(cx-W,cy-HI),P(cx+W,cy-HO),P(cx+W,cy+HO),P(cx-W,cy+HI)};
    dl->AddConvexPolyFilled(pts, 4, fill);
    dl->AddPolyline(pts, 4, bdr, ImDrawFlags_Closed, LW);
    ImVec2 ts = ImGui::CalcTextSize("ALU");
    dl->AddText(P(cx-ts.x*.5f,cy-ts.y*.5f), C_TEXT, "ALU");
  };
  auto draw_arrow = [&](ImVec2 p1, ImVec2 p2, ImU32 col) {
    dl->AddLine(p1, p2, col, LW);
    float dx=p2.x-p1.x, dy=p2.y-p1.y;
    float len=sqrtf(dx*dx+dy*dy);
    if (len < 1.f) return;
    dx/=len; dy/=len;
    constexpr float AL=7.f, AW=3.5f;
    dl->AddTriangleFilled(p2,
      {p2.x-AL*dx-AW*dy, p2.y-AL*dy+AW*dx},
      {p2.x-AL*dx+AW*dy, p2.y-AL*dy-AW*dx}, col);
  };
  auto draw_preg = [&](float x, float y0, float y1, const char* lbl) {
    constexpr float HW=9.f;
    dl->AddRectFilled(P(x-HW,y0), P(x+HW,y1), IM_COL32(28,18,58,255), 3.f);
    dl->AddRect(P(x-HW,y0), P(x+HW,y1), C_PREG, 3.f, 0, 1.5f);
    ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText(P(x-ts.x*.5f, y1+3.f), C_PREG, lbl);
  };

  /* layout constants */
  constexpr float YT=30.f, YB=445.f, YM=235.f;
  constexpr float SX0[5] = {  8.f, 215.f, 460.f, 675.f, 875.f };
  constexpr float SX1[5] = {195.f, 440.f, 655.f, 855.f,1080.f };
  constexpr float PR[4]  = {205.f, 450.f, 665.f, 865.f };
  const char* SNAME[5]   = {"IF","ID","EX","MEM","WB"};

  /* stage backgrounds */
  for (int s=0; s<5; s++) {
    dl->AddRectFilled(P(SX0[s],YT), P(SX1[s],YB), stage_bg(s), 6.f);
    dl->AddRect(P(SX0[s],YT), P(SX1[s],YB), IM_COL32(90,90,100,180), 6.f, 0, 1.f);
    ImVec2 ts = ImGui::CalcTextSize(SNAME[s]);
    dl->AddText(P((SX0[s]+SX1[s])*.5f-ts.x*.5f, YT+4.f), on_bg(s), SNAME[s]);
  }

  /* pipeline register bars + labels below */
  const char* PRNAME[4] = {"IF/ID","ID/EX","EX/MEM","MEM/WB"};
  for (int i=0; i<4; i++)
    draw_preg(PR[i], YT+2, YB-18, PRNAME[i]);

  /* ── IF ──────────────────────────────────────────────────────────── */
  draw_box(18, YM-22, 72, YM+22, IM_COL32(22,22,42,255), comp_bdr(0), "PC");
  if (ImGui::IsMouseHoveringRect(P(18, YM-22), P(72, YM+22))) {
    ImGui::SetTooltip("PC = 0x%s", Utils::hex32(cpu->read_special_register(SPECIAL_PC)).c_str());
  }
  draw_arrow(P(72,YM), P(92,YM), on_bg(0));
  draw_box(92, YM-56, 186, YM+56, IM_COL32(22,32,52,255), comp_bdr(0), "Inst. Mem");

  /* ── ID ──────────────────────────────────────────────────────────── */
  draw_ellipse(328,  68, 60, 25, IM_COL32(22,22,42,255), comp_bdr(1), "Hazard Det.");
  draw_ellipse(328, 124, 54, 23, IM_COL32(22,22,42,255), comp_bdr(1), "Control");
  draw_box(228, YM-56, 415, YM+56, IM_COL32(22,32,52,255), comp_bdr(1), "Reg. File");
  draw_box(242, YM+73, 398, YM+104, IM_COL32(22,22,42,255), comp_bdr(1), "Sign Ext.");

  /* ── EX ──────────────────────────────────────────────────────────── */
  draw_mux(484, YM-45, IM_COL32(32,32,22,255), comp_bdr(2), "FA"); /* ForwardA */
  draw_mux(484, YM+28, IM_COL32(32,32,22,255), comp_bdr(2), "FB"); /* ForwardB */
  draw_mux(542, YM+28, IM_COL32(32,32,22,255), comp_bdr(2), "AS"); /* ALUSrc   */
  draw_alu(608, YM,    IM_COL32(22,38,22,255), comp_bdr(2));
  draw_ellipse(562, 400, 68, 25, IM_COL32(22,22,42,255), comp_bdr(2), "Fwd. Unit");

  /* ── MEM ─────────────────────────────────────────────────────────── */
  draw_box(688, YM-56, 842, YM+56, IM_COL32(22,32,52,255), comp_bdr(3), "Data Mem.");

  /* ── WB ──────────────────────────────────────────────────────────── */
  draw_mux(928, YM, IM_COL32(32,22,32,255), comp_bdr(4), "WB"); /* MemToReg */

  /* write-back wire — routed above the stage region, uses accent color */
  {
    ImU32 wbc = stage_accent(4);
    float wx=940.f, wyt=13.f;
    dl->AddLine(P(wx, YM),   P(wx, wyt),    wbc, LW);
    dl->AddLine(P(wx, wyt),  P(322.f, wyt), wbc, LW);
    draw_arrow(P(322.f, wyt), P(322.f, YM-56), wbc);
  }

  /* instruction text at bottom of each stage */
  for (int s=0; s<5; s++) {
    if (!stage_iid[s]) continue;
    std::string txt = get_instr(stage_iid[s]);
    if (txt.empty()) continue;
    ImVec2 ts = ImGui::CalcTextSize(txt.c_str());
    float cx = (SX0[s]+SX1[s])*.5f;
    float tx = ImClamp(cx-ts.x*.5f, SX0[s]+2.f, SX1[s]-ts.x-2.f);
    dl->AddText(P(tx, YB-22.f), on_bg(s), txt.c_str());
  }

  ImGui::EndChild();
  ImGui::End();
}

/* ------------------------------------------------------------------ */

bool MipsSimImGui::load_file(const std::string& path) {
  if (!Utils::file_exists(path)) {
    exception_msg = "File not found: " + path;
    show_exception = true;
    return false;
  }

  try {
    if (assemble_file(path.c_str(), mem) != 0) {
      exception_msg = "Error parsing assembly file";
      show_exception = true;
      return false;
    }
    mem->snapshot(MEM_TEXT_REGION);
    mem->snapshot(MEM_DATA_REGION);
  } catch (int) {
    exception_msg = err_msg;
    if (err_v) exception_msg += " [0x" + Utils::hex32(err_v) + "]";
    show_exception = true;
    return false;
  }

  asm_filename = path;
  file_loaded  = true;
  refresh_instructions();
  cpu->reset(true, true);
  std::cout << "File loaded: " << path << '\n';
  return true;
}

void MipsSimImGui::refresh_instructions() {
  instructions.clear();
  for (uint32_t addr = MEM_TEXT_START; addr < MEM_TEXT_START + MEM_TEXT_SIZE; addr += 4) {
    try {
      uint32_t word = mem->mem_read_32(addr);
      instructions.push_back({addr,
        "[" + Utils::hex32(addr) + "] " + Utils::hex32(word) + "  " +
        Utils::decode_instruction(word)});
    } catch (int) { break; }
  }
}

/* ------------------------------------------------------------------ */

void MipsSimImGui::do_next() {
  try {
    cpu->next_cycle(std::cout);
  } catch (int) {
    exception_msg = err_msg;
    if (err_v) exception_msg += " [0x" + Utils::hex32(err_v) + "]";
    show_exception = true;
    if (cpu->get_cycle() > 0)
      cpu->run_to_cycle(cpu->get_cycle() - 1, std::cout);
  }
}

void MipsSimImGui::do_prev() {
  if (cpu->get_cycle() == 0) return;
  try {
    cpu->run_to_cycle(cpu->get_cycle() - 1, std::cout);
  } catch (int) {
    exception_msg = err_msg;
    show_exception = true;
  }
}

void MipsSimImGui::do_run() {
  try {
    while (cpu->is_ready()) cpu->next_cycle(std::cout);
  } catch (int) {
    exception_msg = err_msg;
    if (err_v) exception_msg += " [0x" + Utils::hex32(err_v) + "]";
    show_exception = true;
    if (cpu->get_cycle() > 0)
      cpu->run_to_cycle(cpu->get_cycle() - 1, std::cout);
  }
}

void MipsSimImGui::do_reset() {
  cpu->reset(true, true);
}
