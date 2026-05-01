#include "assembler/mips_assembler.h"
#include "cpu/cpu_multi.h"
#include "cpu/cpu_pipelined.h"
#include "mem.h"
#include "utils.h"
#include "exception.h"
#include "interface/mipscli.h"

#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdio>
//#include <readline/readline.h>
//#include <readline/history.h>
#include <iomanip>
#include <bitset>
#include <memory>
#include <vector>

#ifdef _IMGUI
#include "interface/imgui/mips_sim_imgui.h"
#endif

#define MODE_RUN_BATCH       1
#define MODE_RUN_HEX         2
#define MODE_ASSEMBLE        3
#define MODE_RUN_CLI         4
#define MODE_RUN_GUI         5

#define CPU_MULTICYCLE 1
#define CPU_PIPELINED  2

using namespace mips_sim;
using namespace std;

namespace mips_sim
{
uint32_t err_v;
int err_no;
string err_msg;
}

uint32_t load_program(string program_filename, Memory & memory);
int run_batch(string input_file, int run_mode, int cpu_mode, ostream & outstream);

void print_help(const char * cmd)
{
#ifdef _IMGUI
  cerr << "GUI mode:" << endl;
  cerr << "  Call " << cmd << endl;
#endif
  cerr << "Command Line Interface:" << endl;
  cerr << "  Call " << cmd << " cli" << endl;
  cerr << "Batch mode:" << endl;
  cerr << "  Call " << cmd << " {run|runhex|asm} filename [multi|pipe]" << endl;
}

int main(int argc, char * argv[])
{
  string input_file, output_file;
  int run_mode = 0, cpu_mode = 0;

  if (argc > 1)
  {
    /* help */
    if (!strcmp(argv[1], "help"))
    {
      print_help(argv[0]);
      return 0;
    }

    if (!strcmp(argv[1], "asm"))
      run_mode = MODE_ASSEMBLE;
    else if (!strcmp(argv[1], "run"))
      run_mode = MODE_RUN_BATCH;
    else if (!strcmp(argv[1], "runhex"))
      run_mode = MODE_RUN_HEX;
    else if (!strcmp(argv[1], "cli"))
      run_mode = MODE_RUN_CLI;
    else
    {
      cerr << "Undefined run mode: " << argv[1] << endl;
      print_help(argv[0]);
      return EXIT_FAILURE;
    }
  }
  else
  {
#ifdef _IMGUI
    run_mode = MODE_RUN_GUI;
#else
    run_mode = MODE_RUN_CLI;
#endif
  }

  if (run_mode == MODE_ASSEMBLE || run_mode == MODE_RUN_BATCH || run_mode == MODE_RUN_HEX)
  {
    cpu_mode = CPU_PIPELINED; /* default mode */
    if (argc >= 4)
    {
      if (!strcmp(argv[3], "multi"))
        cpu_mode = CPU_MULTICYCLE;
      else if (!strcmp(argv[3], "pipe"))
        cpu_mode = CPU_PIPELINED;
    }

    input_file = argv[2];

    return run_batch(input_file, run_mode, cpu_mode, cout);
  }
  else
  {
#ifdef _IMGUI
    if (run_mode == MODE_RUN_GUI)
    {
      MipsSimImGui gui;
      gui.run();
      return EXIT_SUCCESS;
    }
    else
#endif
    if (run_mode == MODE_RUN_CLI)
    {
      /* Command Line Interface */
      MipsCli mips_cli;
      mips_cli.launch();
    }
    else
    {
      cerr << "Undefined run mode: " << argv[1] << endl;
      print_help(argv[0]);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

/* load program in memory from file */
uint32_t load_program(string program_filename, Memory & memory)
{
  ifstream prog(program_filename);
  size_t i = 0;
  uint32_t word;
  uint32_t words_read = 0;

  /* Open program file. */
  if (!prog) {
    cerr << "Error: Can't open program file " << program_filename << endl;
    exit(-1);
  }

  /* Read in the program. */
  i = 0;
  while (prog >> hex >> word) { //fscanf(prog, "%x\n", &word) != EOF) {
    cout << "Write " << Utils::hex32(word) << " to " << i << endl;
    memory.mem_write_32(static_cast<uint32_t>(MEM_TEXT_START + i), word);
    i += 4;
    words_read++;
  }

  return words_read;
}

int run_batch(string input_file, int run_mode, int cpu_mode, ostream & outstream)
{
  shared_ptr<Memory> mem = shared_ptr<Memory>(new Memory());
  unique_ptr<Cpu> cpu;
  string output_file = input_file + ".hex";

  switch(run_mode)
  {
    case MODE_RUN_BATCH:
      {
        try
        {
          if (assemble_file(input_file.c_str(), mem) != 0)
          {
            cerr << "Error parsing the input file" << endl;
            exit(EXIT_FAILURE);
          }
        }
        catch(int e)
        {
          cerr << "Error " << e << ": " << err_msg;
          if (err_v)
            cerr << " [0x" << Utils::hex32(err_v) << "]";
          cerr << endl;
          exit(EXIT_FAILURE);
        }
      }
      break;
    case MODE_RUN_HEX:
      {
        uint32_t words_read = load_program(input_file, *mem);
        if (!words_read)
        {
          cerr << "No valid instructions in file " << input_file << endl;
          return EXIT_FAILURE;
        }
      }
      break;
    case MODE_ASSEMBLE:
      {
        try
        {
          assemble_file(input_file.c_str(), mem);
          print_file(output_file);
          return EXIT_SUCCESS;
        }
        catch(int e)
        {
          cerr << "Error " << e << ": " << err_msg;
          if (err_v)
            cerr << " [0x" << Utils::hex32(err_v) << "]";
          cerr << endl;
          return EXIT_FAILURE;
        }
      }
    default:
      /* this should never happen */
      assert(0);
      break;
  }

  mem->snapshot(MEM_TEXT_REGION);
  mem->snapshot(MEM_DATA_REGION);

  switch (cpu_mode)
  {
    case CPU_MULTICYCLE:
      cpu = unique_ptr<Cpu>(new CpuMulti(mem));
      break;
    case CPU_PIPELINED:
      cpu = unique_ptr<Cpu>(new CpuPipelined(mem));
      break;
    default:
      assert(0);
  }

  try
  {
    while (cpu->is_ready())
    {
      cpu->next_cycle(outstream);
    }
  }
  catch(int e)
  {
    cerr << "EXCEPTION " << e << ": " << err_msg;
    if (err_v)
      cerr << " [0x" << Utils::hex32(err_v) << "]";
    cerr << endl;
  }

  cout << endl << "Registers:" << endl;
  cpu->print_registers(cout);

  cout << endl << "Data Memory:" << endl;
  mem->print_memory(MEM_DATA_START, MEM_DATA_SIZE, cout);

  cout << endl << "Instructions Memory:" << endl;
  mem->print_memory(MEM_TEXT_START, MEM_TEXT_SIZE, cout);

  cout << endl << endl;
  cpu->print_diagram();

  return EXIT_SUCCESS;
}
