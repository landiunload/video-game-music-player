#pragma once

#include "track.h"

#include <stdbool.h>
#include <stddef.h>

#define PLAYER_HISTORY_CAPACITY 50

typedef struct PlayerState
{
    float volume;
    bool muted;
    Track history[PLAYER_HISTORY_CAPACITY];
    size_t historyCount;
    int32_t historyCursor;
} PlayerState;

void PlayerStateDefaults(PlayerState* state);
bool PlayerStateLoad(PlayerState* state);
bool PlayerStateSave(const PlayerState* state);
void PlayerStateRecord(PlayerState* state, const Track* track);

