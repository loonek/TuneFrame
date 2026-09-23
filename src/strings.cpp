#include "strings.h"

static const Strings EN = { "Feed", "Playlists", "Queue", "Nothing playing" };
static const Strings PL = { "Feed", "Playlisty", "Kolejka", "Nic nie gra" };

// Selects the active UI language
const Strings *L = &EN;
