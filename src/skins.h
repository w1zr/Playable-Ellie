// Every Extras > Skins skin unlocked and owned (see skins.cpp).
#pragma once

namespace skins {

// Scans the game image and installs every patch, or none: on any mismatch it
// logs the reason, writes nothing and returns false.
bool Install();

} // namespace skins
