#pragma once

// Плеер собирает выбранные подсистемы laiue статически в один небольшой
// executable, поэтому DLL export/import здесь намеренно не нужен.
#define LAIUE_WINDOW_API
#define LAIUE_INPUT_API
#define LAIUE_AUDIO_API

