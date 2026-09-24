#pragma once

// On-device UI labels, grouped per language so translations swap in one place
struct Strings
{
    const char *tab_feed;
    const char *tab_playlists;
    const char *queue_title;
    const char *settings_title;
    const char *language_label;
    const char *orientation_label;
    const char *nothing_playing;
};

// Number of supported languages
#define LANG_COUNT 5

// Active language table; read through this pointer everywhere in the UI
extern const Strings *L;

// Newline-separated language names for a dropdown, in LANG_COUNT order
extern const char *LANG_NAMES;

// Sets the active language by index (0..LANG_COUNT-1)
void set_language(int index);

// Returns the active language index
int current_language();
