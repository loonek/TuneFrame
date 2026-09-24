#include <Preferences.h>

#include "settings_store.h"

static Preferences prefs;

// Loads the saved language index from NVS, or def when nothing is stored
int settings_load_language(int def)
{
    prefs.begin("ytmc", true);
    int v = prefs.getInt("lang", def);
    prefs.end();
    return v;
}

// Persists the language index to NVS
void settings_save_language(int index)
{
    prefs.begin("ytmc", false);
    prefs.putInt("lang", index);
    prefs.end();
}

// Loads the saved orientation from NVS, or def when nothing is stored
int settings_load_orientation(int def)
{
    prefs.begin("ytmc", true);
    int v = prefs.getInt("orient", def);
    prefs.end();
    return v;
}

// Persists the orientation to NVS
void settings_save_orientation(int value)
{
    prefs.begin("ytmc", false);
    prefs.putInt("orient", value);
    prefs.end();
}
