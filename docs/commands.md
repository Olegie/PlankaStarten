# Commands

The graphical command field accepts:

```text
check
run <procedure> [args...]
format
save
bytecode
ir
evidence
cgen
asmgen
asm8086
```

GUI commands operate on the active selected `.plk` file in the left file list.
Selecting another file reloads its procedure table and chooses a runnable
zero-argument procedure when one is available.

`format` normalizes the editor buffer for Win32 display, removes trailing
spaces and keeps leading whitespace intact so table-style `.plk` rows are not
damaged.

The CLI accepts:

```text
plankastarten_cli check <file.plk> [more.plk...]
plankastarten_cli list <file.plk> [more.plk...]
plankastarten_cli run <file.plk> <procedure> [args...]
plankastarten_cli bytecode <file.plk> <out.pbc>
plankastarten_cli ir <file.plk> <out.ir>
plankastarten_cli evidence <file.plk> <out.json>
plankastarten_cli cgen <file.plk> <out.c>
plankastarten_cli asmgen <file.plk> <out.S>
plankastarten_cli asm8086 <file.plk> <out.asm>
```
