# PlankaStarten application examples

These files are small console applications written as `.plk` procedure sets.
They are meant to show PlankaStarten as an application host, not only as a
procedure tester.

## Run without compiling

```bat
build\plankastarten_cli.exe app examples\apps\loan_estimator.plk
build\plankastarten_cli.exe app examples\apps\bmi_guard.plk
build\plankastarten_cli.exe app examples\apps\vector_length.plk
```

The `app` command chooses the `start` procedure, asks for `V0`, `V1`, ... and
prints the `R` results.

## Compile and run

```bat
build\plankastarten_cli.exe compile examples\apps\loan_estimator.plk
build\loan_estimator_console.exe
```

Running the generated executable without arguments starts the interactive
prompt. It still accepts direct procedure calls:

```bat
build\loan_estimator_console.exe start 12000 0.06 60
```

## Examples

`loan_estimator.plk`

Inputs:

```text
V0 principal
V1 interest rate as fraction, for example 0.06
V2 months
```

`bmi_guard.plk`

Inputs:

```text
V0 weight in kg
V1 height in cm
```

`vector_length.plk`

Inputs:

```text
V0 x
V1 y
```
