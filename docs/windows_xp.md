# Windows XP-Oriented Builds

PlankaStarten keeps the GUI close to the Win32 API: ANSI window procedures,
standard controls, no framework dependency, and a Common Controls manifest. The
Windows build script also requests subsystem version `5.01`.

That makes the source suitable for XP-oriented builds, but the final binary is
defined by the compiler and C runtime used for packaging.

For an XP package, build with a toolchain that targets the classic Windows C
runtime, for example:

- MinGW-w64 configured for `msvcrt`
- an older MinGW.org or TDM-GCC toolchain
- Visual Studio with an XP platform toolset

After building, verify the executable before publishing it as an XP package:

```bat
dumpbin /headers build\PlankaStarten.exe
dumpbin /imports build\PlankaStarten.exe
```

Expected subsystem:

```text
MajorSubsystemVersion 5
MinorSubsystemVersion 1
```

The import table should not depend on runtime DLLs that are absent on a clean
Windows XP installation. If such imports are present, rebuild with an
XP-capable toolchain before labeling the binary as an XP build.
