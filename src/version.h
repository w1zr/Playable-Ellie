#pragma once
#define MOD_NAME "Playable Ellie (WORKING)"
#define MOD_VERSION "1.0.0"

// Test builds (1): F11 cycles the 9mm overlay tables in game and Fix9mm=2-6
// are allowed (see ellie.cpp). Release builds (0): Fix9mm is 0 or 1.
#ifndef PE_TEST_9MM
#define PE_TEST_9MM 0
#endif
