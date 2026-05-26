<p align="center">
  <img src="docs/logo.svg" alt="PlankaStarten logo" width="560">
</p>

# PlankaStarten

PlankaStarten ist eine eigenstaendige C-Anwendung zum Laden, Pruefen,
Ausfuehren und Uebersetzen von `.plk`-Dateien ueber die oeffentliche PlankaC
API.

Der zugehoerige Sprachkern liegt in
[Olegie/PlankaC](https://github.com/Olegie/PlankaC). PlankaStarten ist die
Arbeitsoberflaeche und das Einbettungsbeispiel fuer diese API.

Das Programm ist kein zweiter Interpreter und kein Wrapper um eine externe
Kommandozeile. Es bindet `libplankac.a` ein, verwendet `plankac.h`, erzeugt
einen `PLANKAC_CONTEXT` und arbeitet dann direkt mit den API-Funktionen von
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
- Ausgaben in einer einfachen Konsolenflaeche anzeigen
- native EXE-Dateien bauen: Konsolenprofile werden Konsolenprogramme,
  GUI- und Cube-Profile werden Windows-GUI-Programme
- XP-nahe Befehlszeile fuer Quelltextbefehle und Backend-Ausgaben
- Quelltext formatieren, ohne fuehrende Tabellenabstaende zu zerstoeren:
  Leerraum am Zeilenende wird entfernt, nach `END` wird sauber getrennt

Die grafische Oberflaeche bleibt bewusst direkt und technisch. Sie ist als
Arbeitsfenster gedacht: Projektbaum links, Editor in der Mitte, Prozeduren und
Argumente rechts, unten eine Eingabezeile und die Ausgabe.

## Aufbau

```text
src/plankastarten_gui.c   Win32-Arbeitsfenster in C
src/plankastarten_cli.c   Konsolenprogramm fuer Tests und Skripte
examples/max3.plk         kleines Beispielprogramm
docs/api_connection.md    API-Grenze zwischen PlankaStarten und PlankaC
docs/commands.md          Befehle der GUI und CLI
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
werden Ausfuehrung und Backend-Ausgaben geprueft.

## Windows

Die grafische Oberflaeche ist eine direkte Win32-C-Anwendung. Der Build bleibt
nah an klassischen Windows-Controls und kann mit einem passenden Toolchain auch
fuer aeltere Windows-Ziele gebaut werden. Hinweise fuer XP-orientierte Builds
stehen in [docs/windows_xp.md](docs/windows_xp.md).

## Grafische Anwendung

```bat
build\PlankaStarten.exe
```

Der Startordner ist `examples\`. Ueber **Open** kann ein anderer Ordner mit
`.plk`-Dateien ausgewaehlt werden. Links steht ein Projektbaum mit
Unterordnern. Der aktive `.plk`-Eintrag ist das Quellprofil fuer **Check**,
**Run**, **Compile** und die Backend-Ausgaben.

Bei nummerierten Quellgruppen wie `00_types.plk` bis `04_calculator.plk`
laedt PlankaStarten die vorhergehenden Dateien derselben Serie mit. Eine
spaetere Datei kann dadurch Prozeduren aus frueheren Dateien verwenden, ohne
unabhaengige Skizzen oder Testdateien in den Kontext zu ziehen.

Nuetzliche Eingaben im Befehlsfeld:

```text
check
run start
run max3 4 9 7
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

Die erzeugten Dateien werden in `build\` abgelegt.

## Kommandozeile

```bat
build\plankastarten_cli.exe check examples\max3.plk
build\plankastarten_cli.exe list examples\max3.plk
build\plankastarten_cli.exe run examples\max3.plk start
build\plankastarten_cli.exe run a.plk b.plk -- start
build\plankastarten_cli.exe compile examples\max3.plk
build\plankastarten_cli.exe evidence examples\max3.plk build\max3.evidence.json
```

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
PlankaC bleibt Sprachkern, Loader, Runtime und Backend-Schicht.

Der native Compile-Befehl bleibt an derselben Grenze: PlankaC erzeugt die
Sprachartefakte, PlankaStarten waehlt nur das passende Host-Profil und ruft den
lokalen C-Compiler auf.
