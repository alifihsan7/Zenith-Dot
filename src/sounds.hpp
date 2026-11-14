#pragma once

#include "resource.h"

#include <windows.h>
#include <mmsystem.h>

namespace zenith {

// Plays one of Zenula's chimes from the exe's resources. Async and one-shot: nothing stays
// loaded or running after it ends.
inline void playChime(int sound) {
    PlaySoundW(MAKEINTRESOURCEW(sound), GetModuleHandleW(nullptr), SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
}

}
