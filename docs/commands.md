# Commands

The graphical command field accepts:

```text
check
run <procedure> [args...]
compile
format
save
bytecode
ir
evidence
cgen
asmgen
asm8086
```

GUI commands operate on the active selected `.plk` file in the left project
tree. The tree scans subfolders. For numbered source groups such as
`00_types.plk` to `04_calculator.plk`, the GUI loads the earlier files from the
same series before running or checking the selected file.

`compile` saves the active file, detects whether the source is a console
procedure set or a GUI/cube application profile, and builds a native `.exe`
under `build\`. GUI profiles are built as Windows subsystem executables.
Console profiles are built as console subsystem executables.

`format` normalizes the editor buffer for Win32 display, removes trailing
spaces and keeps leading whitespace intact so table-style `.plk` rows are not
damaged.

The CLI accepts:

```text
plankastarten_cli check <file.plk> [more.plk...]
plankastarten_cli list <file.plk> [more.plk...]
plankastarten_cli run <file.plk> <procedure> [args...]
plankastarten_cli run <file.plk> [more.plk...] -- <procedure> [args...]
plankastarten_cli compile <file.plk>
plankastarten_cli bytecode <file.plk> <out.pbc>
plankastarten_cli ir <file.plk> <out.ir>
plankastarten_cli evidence <file.plk> <out.json>
plankastarten_cli cgen <file.plk> <out.c>
plankastarten_cli asmgen <file.plk> <out.S>
plankastarten_cli asm8086 <file.plk> <out.asm>
```
