# API Connection

PlankaStarten is connected to
[Olegie/PlankaC](https://github.com/Olegie/PlankaC) through
`c/include/plankac.h`.

The host does not parse `.plk` itself. It passes source paths to
`plankac_context_load_sources`, then uses the loaded context for inspection,
execution and backend output.

```text
.plk files
  -> PlankaStarten project tree
  -> source-set selection
  -> plankac_context_load_sources
  -> PLANKAC_CONTEXT
  -> run / evidence / bytecode / C / ASM / 8086 output
  -> optional native compile step
```

The GUI and CLI both use the same API boundary. The GUI adds a source editor,
project tree, procedure list and console-style command input; it does not add
another language implementation. The project tree can select files in nested
folders. When a selected source belongs to a numbered series, the GUI passes
the earlier files in that series to PlankaC as the active source set.

For native compile, PlankaStarten still enters through the same public API. For
console sources it asks PlankaC for generated C and links that file with
`libplankac.a`. For GUI/cube profiles it builds a Windows host executable that
loads the selected `.plk` application profile.
