# mips_sim

Academic MIPS CPU simulator. Supports multicycle and pipelined CPU, single-step execution, register inspection, and configurable hazard/forwaring/branch handling.

## Features

- 5-stage pipelined CPU (IF → ID → EX → MEM → WB)
- Forwarding unit and hazard detection unit (configurable)
- Branch prediction strategies: flush, fixed non-taken, delayed
- Pipeline diagram with stall visualization
- Integer and floating-point register views
- Data and text memory inspection
- Interactive GUI and CLI
- Batch mode for scripted use

## Dependencies

| Dependency | Purpose | Package (Debian/Ubuntu) |
|---|---|---|
| g++ ≥ 7 | C++17 compiler | `build-essential` |
| flex | Lexer generator (assembler) | `flex` |
| bison | Parser generator (assembler) | `bison` |
| SDL2 | Window/input backend | `libsdl2-dev` |
| OpenGL | GPU rendering | `libgl-dev` |

Install all at once on Debian/Ubuntu:

```sh
sudo apt install build-essential flex bison libsdl2-dev libgl-dev
```

On macOS (Homebrew):

```sh
brew install sdl2
```

## Building

Clone with submodules:

```sh
git clone --recurse-submodules <repo-url>
cd mips_sim
```

If you already cloned without `--recurse-submodules`:

```sh
git submodule update --init
```

Then build:

```sh
make
```

The binary is produced at `./mips_sim`.

### Clean

```sh
make clean
```

## Running

### GUI mode (default)

```sh
./mips_sim
```

Load an assembly file via **File -> Load File** or drag and drop a `.s` / `.asm` file onto the window.

Controls:

| Button | Action |
|---|---|
| **Reset** | Reset CPU and memory to initial state |
| **Prev** | Step back one cycle |
| **Next** | Step forward one cycle |
| **Run** | Execute until program ends |

### CLI mode

Interactive command-line interface:

```sh
./mips_sim cli
```

### Batch mode

Assemble and run a file, printing final register and memory state:

```sh
# Pipelined CPU (default)
./mips_sim run testdata/test1.s

# Multi-cycle CPU
./mips_sim run testdata/test1.s multi

# Assemble only (produces .hex file)
./mips_sim asm testdata/test1.s

# Run a pre-assembled hex file
./mips_sim runhex testdata/test1.x
```

## GUI layout

The interface uses dockable panels — drag any tab to rearrange:

- **Registers** — special registers (PC, HI, LO, EPC, …) plus tabbed views for integer, FP single, and FP double registers
- **Pipeline Diagram** — color-coded grid showing each instruction's progress through pipeline stages; stalls shown in yellow
- **Text Memory** — disassembled instructions with the current PC highlighted
- **Data Memory** — raw data memory in 16-byte rows

**Settings** (File → Settings) lets you toggle the forwarding unit, hazard detection unit, branch prediction strategy, and the pipeline stage where branches are resolved.

## Supported MIPS instructions

R-type: `add`, `addu`, `sub`, `subu`, `and`, `or`, `xor`, `nor`, `slt`, `sltu`, `sll`, `srl`, `sra`, `sllv`, `srlv`, `srav`, `jr`, `jalr`, `mult`, `multu`, `div`, `divu`, `mfhi`, `mthi`, `mflo`, `mtlo`, `syscall`

I-type: `addi`, `addiu`, `slti`, `sltiu`, `andi`, `ori`, `xori`, `lui`, `lw`, `sw`, `lb`, `lbu`, `lh`, `lhu`, `sb`, `sh`, `beq`, `bne`, `blez`, `bgtz`, `lwc1`, `swc1`

J-type: `j`, `jal`

FP (COP1): `add.s`, `sub.s`, `mul.s`, `div.s`, `mov.s`, `c.eq.s`, `c.lt.s`, `c.le.s`, `bc1t`, `bc1f`

## Third-party

- [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) — MIT license
- SDL2 — zlib license
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) — zlib license
