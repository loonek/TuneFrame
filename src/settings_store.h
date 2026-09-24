#pragma once

// Loads the saved language index, or def when nothing is stored
int settings_load_language(int def);

// Persists the language index to non-volatile storage
void settings_save_language(int index);

// Loads the saved orientation (0 landscape, 1 portrait), or def when nothing is stored
int settings_load_orientation(int def);

// Persists the orientation to non-volatile storage
void settings_save_orientation(int value);
