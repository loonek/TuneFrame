#pragma once

// On-device UI labels, grouped per language so translations swap in one place
struct Strings
{
    const char *tab_feed;
    const char *tab_playlists;
    const char *queue_title;
    const char *nothing_playing;
};

// Active language; swap this pointer to translate every on-device label at once
extern const Strings *L;
