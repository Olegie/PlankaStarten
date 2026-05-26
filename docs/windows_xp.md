# Windows XP Build Notes

PlankaStarten keeps the GUI source close to the Win32 API on purpose. The current
window uses ANSI Win32 controls, grouped command panels, and a Common Controls
v6 manifest. `build.bat` sets the Windows GUI subsystem to
`5.01`, which is the Windows XP subsystem version.

That is only one half of XP support. The other half is the C runtime selected by
the compiler. A modern UCRT MinGW can still emit imports such as
`api-ms-win-crt-runtime-l1-1-0.dll`. Those DLLs are not part of a clean Windows
XP install, so such a binary should not be published as an XP-ready build.

For an XP package, use one of these toolchains:

- MinGW-w64 built against `msvcrt`, not UCRT.
- An older MinGW.org or TDM-GCC toolchain that targets XP.
- Visual Studio with an XP platform toolset.

After building, verify the executable:

```bat
objdump -p build\PlankaStarten.exe | findstr /i "Subsystem MajorSubsystemVersion MinorSubsystemVersion DLL"
```

Expected shape for the GUI executable:

```text
MajorSubsystemVersion 5
MinorSubsystemVersion 1
Subsystem              (Windows GUI)
DLL Name: GDI32.dll
DLL Name: KERNEL32.dll
DLL Name: SHELL32.dll
DLL Name: USER32.dll
```

The important absence is:

```text
api-ms-win-crt-*
```

If those imports are present, the source is still XP-oriented, but the generated
binary is tied to a newer runtime stack.
