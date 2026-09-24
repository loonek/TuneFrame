#include "i18n.h"

// One row per language, ASCII-only to stay within the font's glyph set
static const Strings LANGS[LANG_COUNT] = {
    { "Feed", "Playlists", "Queue",          "Settings",      "Language", "Vertical mode",   "Nothing playing" },        // English
    { "Feed", "Playlisty", "Kolejka",        "Ustawienia",    "Jezyk",    "Tryb wertykalny", "Nic nie gra" },            // Polski
    { "Feed", "Listas",    "Cola",           "Ajustes",       "Idioma",   "Modo vertical",   "Nada en reproduccion" },   // Espanol
    { "Feed", "Playlists", "Warteschlange",  "Einstellungen", "Sprache",  "Vertikaler Modus","Nichts wird abgespielt" }, // Deutsch
    { "Feed", "Playlists", "File d'attente", "Parametres",    "Langue",   "Mode vertical",   "Rien en lecture" },        // Francais
};

const char *LANG_NAMES = "English\nPolski\nEspanol\nDeutsch\nFrancais";

static int lang_idx = 0;

// Selects the active UI language
const Strings *L = &LANGS[0];

// Sets the active language by index
void set_language(int index)
{
    if (index < 0 || index >= LANG_COUNT) return;
    lang_idx = index;
    L = &LANGS[index];
}

// Returns the active language index
int current_language()
{
    return lang_idx;
}
