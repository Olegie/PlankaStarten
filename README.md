# PlankaStarten

PlankaStarten is a small C workbench for running and compiling `.plk` files
through the public PlankaC API.

It is intentionally separate from PlankaC. PlankaC owns the language loader,
runtime, type checks, bytecode writer and backend emitters. PlankaStarten is a
host program: it links against `libplankac.a`, includes `plankac.h`, creates a
`PLANKAC_CONTEXT`, and calls the API directly.

## What It Provides

- Win32 graphical workbench written in C.
- File list for `.plk` sources in a selected folder.
- Editable source pane.
- Procedure list.
- Procedure name and argument fields.
- Console-style command input.
- Output console.
- API-backed commands: check, run, bytecode, IR, evidence, generated C,
  generated x86-64 ASM and generated 8086 ASM.
- Console runner for scripts and smoke tests.

No command shells are used to execute `.plk` files. The shell is used only by
`build.bat` to compile the host program.

## Build

Set `PLANKAC_ROOT` if PlankaC is not in `..\PlankaMath`:

```bat
set PLANKAC_ROOT=C:\Users\Admin\Downloads\PlankaMath
build.bat
```

The build creates:

```text
build\PlankaStarten.exe
build\plankastarten_cli.exe
```

`build.bat` also runs API smoke tests against `examples\max3.plk`.

## GUI

Run:

```bat
build\PlankaStarten.exe
```

The default workspace is `examples\`. Use **Open** to select another folder
with `.plk` files. The workbench loads all `.plk` files in the folder as one
source set.

Useful command input examples:

```text
check
run start
run max3 4 9 7
bytecode
ir
evidence
cgen
asmgen
asm8086
save
```

Generated files are written under `build\`.

## CLI

```bat
build\plankastarten_cli.exe check examples\max3.plk
build\plankastarten_cli.exe list examples\max3.plk
build\plankastarten_cli.exe run examples\max3.plk start
build\plankastarten_cli.exe evidence examples\max3.plk build\max3.evidence.json
```

## API Boundary

PlankaStarten uses these public functions:

```text
plankac_create
plankac_destroy
plankac_context_load_sources
plankac_context_summary
plankac_context_proc_count
plankac_context_get_proc
plankac_context_run
plankac_context_write_bytecode
plankac_context_write_ir
plankac_context_write_evidence
plankac_context_write_c_backend
plankac_context_write_asm_runtime
plankac_context_write_asm8086_runtime
plankac_format
```

That keeps this repository useful as an embedding example: it shows how a C
program can host PlankaC without depending on PlankaC internal headers.
