// Native (modding tools): writes the player's own game assets into a folder, in the formats and
// names that mods use, so creators can start from the originals without anyone sharing them.
#pragma once
#include <string>

namespace ExportAssets {
// Returns the number of files written (0 on failure). `folder` ends with a separator.
int Run(const std::string& folder);
}
