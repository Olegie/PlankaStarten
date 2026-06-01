<p align="center">
  <img src="docs/logo.svg" alt="PlankaStarten logo" width="560">
</p>

# PlankaStarten

PlankaStarten ist eine eigenstaendige C-Anwendung zum Laden, Pruefen,
Ausfuehren und Uebersetzen von `.plk`-Dateien ueber die oeffentliche PlankaC
API.

Der zugehoerige Compiler und Sprachkern liegt in
[Olegie/PlankaC](https://github.com/Olegie/PlankaC). PlankaStarten ist die
Arbeitsoberflaeche und das Einbettungsbeispiel fuer diese API.

Das Programm ist kein zweiter Interpreter und kein Wrapper um eine externe
Kommandozeile. Es bindet `libplankac.a` ein, verwendet `plankac.h`, erzeugt
einen `PLANKAC_CONTEXT` und arbeitet direkt mit den API-Funktionen von
PlankaC.

## Zweck

PlankaStarten soll zeigen, wie ein normales C-Programm PlankaC als Bibliothek
einbetten kann:

- `.plk`-Dateien aus einem Arbeitsordner und dessen Unterordnern laden
- Prozeduren anzeigen
- Quelltext mit Zeilennummern, Cursorposition und sauberer
  Zeilenumbruch-Normalisierung bearbeiten und speichern
- Prozeduren mit Argumenten ausfuehren
- Bytecode, IR, Evidence JSON, C, x86-64 ASM und 8086 ASM erzeugen
- native EXE-Dateien bauen: Konsolenprofile werden Konsolenprogramme,
  GUI- und Cube-Profile werden Windows-GUI-Programme
- Ausgaben in einer einfachen Konsolenflaeche anzeigen
- Quelltext formatieren, ohne fuehrende Tabellenabstaende zu zerstoeren

Die grafische Oberflaeche bleibt bewusst direkt und technisch. Sie ist als
Arbeitsfenster gedacht: Projektbaum links, Editor in der Mitte, Prozeduren und
Argumente rechts, unten eine Eingabezeile und die Ausgabe.

## Aufbau

```text
src/plankastarten_gui.c       Win32-Arbeitsfenster in C
src/plankastarten_cli.c       Konsolenprogramm fuer Tests und Skripte
src/plankastarten_compile.c   native Compile-Schicht fuer Konsole und GUI
examples/max3.plk             kleines Beispielprogramm
examples/apps/                interaktive Konsolenbeispiele
docs/api_connection.md        API-Grenze zwischen PlankaStarten und PlankaC
docs/commands.md              Befehle der GUI und CLI
```

## Bauen

PlankaStarten erwartet eine lokale PlankaC-Arbeitskopie. Wenn sie nicht unter
`..\PlankaMath` liegt, wird der Pfad mit `PLANKAC_ROOT` gesetzt:

```bat
set PLANKAC_ROOT=C:\Users\Admin\Downloads\PlankaMath
build.bat
```

Der Build erzeugt:

```text
build\PlankaStarten.exe
build\plankastarten_cli.exe
```

`build.bat` fuehrt danach einen API-Test mit `examples\max3.plk` aus. Dabei
werden Ausfuehrung, native Kompilierung und Backend-Ausgaben geprueft.

## Grafische Anwendung

```bat
build\PlankaStarten.exe
```

Der Startordner ist `examples\`. Ueber **Open** kann ein anderer Ordner mit
`.plk`-Dateien ausgewaehlt werden. Links steht ein Projektbaum mit
Unterordnern. Der aktive `.plk`-Eintrag ist die Quelle fuer **Check**, **Run**,
**Compile** und die Backend-Ausgaben.

Bei nummerierten Quellgruppen wie `00_types.plk` bis `04_calculator.plk`
laedt PlankaStarten die vorhergehenden Dateien derselben Serie mit. So kann
eine spaetere Datei Prozeduren aus den frueheren Dateien verwenden, ohne dass
unabhaengige Skizzen oder Testdateien in den Kontext gezogen werden.
Liegt eine aktive Datei in einem `examples`-Ordner und existiert daneben ein
`src`-Ordner, werden die nummerierten Quellen aus diesem `src`-Ordner als
Projektbibliothek geladen. Ein Beispiel wie `session_guarded.plk` kann dadurch
`divide_checked` aus `src/01_arithmetic.plk` verwenden, ohne dass die
Bibliotheksdateien von Hand ausgewaehlt werden muessen.

Nuetzliche Eingaben im Befehlsfeld:

```text
check
run start
run max3 4 9 7
app
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

`compile` erkennt das Profil der aktiven `.plk`-Datei. Normale Prozedurdateien
werden als Konsolenprogramme gebaut. GUI- und Cube-Profile werden als
Windows-GUI-Programme gebaut und aus der grafischen Anwendung direkt gestartet.
Konsolenprogramme koennen ohne Argumente gestartet werden; dann fragen sie
ihre `V`-Argumente interaktiv ab, geben einen klaren `Result`-Block aus und
warten vor dem Schliessen des Fensters.

## Anwendungsbeispiele

```bat
build\plankastarten_cli.exe app examples\apps\loan_estimator.plk
build\plankastarten_cli.exe app examples\apps\bmi_guard.plk
build\plankastarten_cli.exe app examples\apps\vector_length.plk
```

Dies sind kleine `.plk`-Programme mit Eingabeaufforderung. Sie zeigen den
Unterschied zwischen einem nackten Prozedurtest und einem gehosteten
PlankaC-Programm.

## Kommandozeile

```bat
build\plankastarten_cli.exe check examples\max3.plk
build\plankastarten_cli.exe list examples\max3.plk
build\plankastarten_cli.exe run examples\max3.plk start
build\plankastarten_cli.exe app examples\apps\loan_estimator.plk
build\plankastarten_cli.exe run a.plk b.plk -- start
build\plankastarten_cli.exe compile examples\max3.plk
build\plankastarten_cli.exe evidence examples\max3.plk build\max3.evidence.json
```

## Windows

Die grafische Oberflaeche ist eine direkte Win32-C-Anwendung. Der Build bleibt
nah an klassischen Windows-Controls und kann mit einem passenden Toolchain auch
fuer aeltere Windows-Ziele gebaut werden. Hinweise fuer XP-orientierte Builds
stehen in [docs/windows_xp.md](docs/windows_xp.md).

## API-Grenze

PlankaStarten nutzt unter anderem:

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

Damit bleibt die Grenze sauber: PlankaStarten ist Host und Werkzeugoberflaeche;
PlankaC bleibt Compiler, Loader, Runtime und Backend-Schicht. Der native
Compile-Befehl bleibt an derselben Grenze: PlankaC erzeugt die
Sprachartefakte, PlankaStarten waehlt das passende Host-Profil und ruft den
lokalen C-Compiler auf.

---

# English

PlankaStarten is a standalone C application for loading, checking, running and
compiling `.plk` files through the public PlankaC API.

The compiler and language core are maintained in
[Olegie/PlankaC](https://github.com/Olegie/PlankaC). PlankaStarten is the
workbench and embedding example built around that API.

It is not a second interpreter and not a command-line wrapper. It links
`libplankac.a`, includes `plankac.h`, creates a `PLANKAC_CONTEXT`, and calls the
PlankaC API directly.

## Purpose

PlankaStarten demonstrates how a normal C program can embed PlankaC as a
library:

- load `.plk` files from a workspace folder and its subfolders
- inspect procedures
- edit and save source with line numbers and cursor position
- run procedures with arguments
- run `.plk` files as interactive console applications
- write bytecode, IR, Evidence JSON, C, x86-64 ASM and 8086 ASM
- build native executables
- show output in a simple console panel

The graphical application is intentionally direct and technical: project tree
on the left, editor in the center, procedures and arguments on the right,
command input and output at the bottom.

## Build

Set `PLANKAC_ROOT` if PlankaC is not located at `..\PlankaMath`:

```bat
set PLANKAC_ROOT=C:\Users\Admin\Downloads\PlankaMath
build.bat
```

The build creates:

```text
build\PlankaStarten.exe
build\plankastarten_cli.exe
```

## GUI

```bat
build\PlankaStarten.exe
```

The default workspace is `examples\`. Use **Open** to choose another folder
with `.plk` files. The left panel is a project tree with subfolders. The
selected `.plk` file is used by **Check**, **Run**, **Compile**, and backend
output commands.

For numbered source series, PlankaStarten loads the earlier files from the same
series. For files inside an `examples` directory, it also loads numbered
sources from a sibling `src` directory as the project library.

Useful command input:

```text
check
run start
run max3 4 9 7
app
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

`compile` detects the active source profile. Console procedure sets become
console executables. GUI and cube profiles become Windows GUI executables.
Generated console executables can be launched without arguments. In that mode
they choose an entry procedure, ask for `V` inputs, print a `Result` block and
wait before closing.

## Application Examples

```bat
build\plankastarten_cli.exe app examples\apps\loan_estimator.plk
build\plankastarten_cli.exe app examples\apps\bmi_guard.plk
build\plankastarten_cli.exe app examples\apps\vector_length.plk
```

## CLI

```bat
build\plankastarten_cli.exe check examples\max3.plk
build\plankastarten_cli.exe list examples\max3.plk
build\plankastarten_cli.exe run examples\max3.plk start
build\plankastarten_cli.exe app examples\apps\loan_estimator.plk
build\plankastarten_cli.exe run a.plk b.plk -- start
build\plankastarten_cli.exe compile examples\max3.plk
```

## Boundary

PlankaStarten is the host and tool surface. PlankaC remains the compiler,
loader, runtime, type/checking layer and backend layer.
