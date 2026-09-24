#include "settings_store.h"

// The PC simulator has no persistent store, so settings reset each run
int settings_load_language(int def) { return def; }
void settings_save_language(int index) { (void)index; }
int settings_load_orientation(int def) { return def; }
void settings_save_orientation(int value) { (void)value; }
