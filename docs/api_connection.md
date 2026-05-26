# API Connection

PlankaStarten is connected to PlankaC through `c/include/plankac.h`.

The host does not parse `.plk` itself. It passes source paths to
`plankac_context_load_sources`, then uses the loaded context for inspection,
execution and backend output.

```text
.plk files
  -> PlankaStarten file list
  -> plankac_context_load_sources
  -> PLANKAC_CONTEXT
  -> run / evidence / bytecode / C / ASM / 8086 output
```

The GUI and CLI both use the same API boundary. The GUI adds a source editor,
procedure list and console-style command input; it does not add another
language implementation.
