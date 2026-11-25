#pragma once

#include <string>

// Tiny file logger. A game launched by double-clicking has no console, so
// startup problems need to leave a readable trace next to the save files.
namespace Log {

void Init(const std::string& path);
void Write(const std::string& message);
const std::string& Path();

// Blocks with a message box on Windows (stderr elsewhere) so a fatal startup
// error does not look like the window "just closing".
void ShowFatal(const std::string& message);

} // namespace Log
