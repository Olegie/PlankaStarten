#include "plankastarten_gui_i18n.h"

static const char *const PST_TEXT_EN[PST_T_COUNT] = {
    "PLK Workbench | PlankaC runner, compiler and app host",
    "Workspace",
    " Source ",
    " Backends ",
    "Project",
    "Editor",
    "Procedures",
    "Command",
    "Output",
    "Procedure",
    "Arguments",
    "Result",
    "Open",
    "Save",
    "Format",
    "Check",
    "Run",
    "Compile",
    "Clear",
    "Exec",
    "PlankaStarten ready",
    "Select folder with .plk files",
    "About PlankaStarten",
    "PlankaStarten is a small C workbench for PLK files. It uses the PlankaC API to load, inspect, run, compile, and export artifacts from Plankalkuel-oriented source files.",
    "PlankaStarten Commands",
    "check\nrun <procedure> [args]\napp / compile\nformat\nsave\nbytecode\nir\nevidence\ncgen\nasmgen\nasm8086",
    "Find Procedure",
    "Text or procedure name",
    "Find",
    "Procedure list",
    "English",
    "German"
};

static const char *const PST_TEXT_DE[PST_T_COUNT] = {
    "PLK-Arbeitsplatz | PlankaC-Runner, Compiler und App-Host",
    "Arbeitsordner",
    " Quelle ",
    " Backends ",
    "Projekt",
    "Editor",
    "Prozeduren",
    "Befehl",
    "Ausgabe",
    "Prozedur",
    "Argumente",
    "Ergebnis",
    "Oeffnen",
    "Speichern",
    "Format",
    "Pruefen",
    "Start",
    "Bauen",
    "Leeren",
    "Ausf.",
    "PlankaStarten bereit",
    "Ordner mit .plk-Dateien auswaehlen",
    "Ueber PlankaStarten",
    "PlankaStarten ist ein kleiner C-Arbeitsplatz fuer PLK-Dateien. Er nutzt die PlankaC-API zum Laden, Pruefen, Ausfuehren, Kompilieren und Exportieren von Artefakten aus Plankalkuel-nahen Quellen.",
    "PlankaStarten Befehle",
    "check\nrun <Prozedur> [Argumente]\napp / compile\nformat\nsave\nbytecode\nir\nevidence\ncgen\nasmgen\nasm8086",
    "Prozedur suchen",
    "Text oder Prozedurname",
    "Suchen",
    "Prozedurliste",
    "Englisch",
    "Deutsch"
};

const char *pst_text(PST_LANG lang, PST_TEXT text)
{
    if (text < 0 || text >= PST_T_COUNT) {
        return "";
    }
    return lang == PST_LANG_DE ? PST_TEXT_DE[text] : PST_TEXT_EN[text];
}

const char *pst_language_name(PST_LANG lang)
{
    return lang == PST_LANG_DE ? "Deutsch" : "English";
}
