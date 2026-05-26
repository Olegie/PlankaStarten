# Commands

The graphical command field accepts:

```text
check
run <procedure> [args...]
save
bytecode
ir
evidence
cgen
asmgen
asm8086
```

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
