#pragma once

#include "track.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RadioSource RadioSource;

typedef enum RadioSourcePhase
{
    RADIO_SOURCE_IDLE = 0,
    RADIO_SOURCE_FINDING,
    RADIO_SOURCE_DOWNLOADING_AUDIO,
    RADIO_SOURCE_DOWNLOADING_ARTWORK,
} RadioSourcePhase;

typedef enum RadioSourceEventType
{
    RADIO_SOURCE_EVENT_TRACK_READY = 0,
    RADIO_SOURCE_EVENT_ERROR,
} RadioSourceEventType;

typedef struct RadioSourceEvent
{
    RadioSourceEventType type;
    Track track;
    wchar_t message[384];
} RadioSourceEvent;

RadioSource* RadioSourceCreate(void);
void RadioSourceDestroy(RadioSource* source);

// Одновременно выполняется один запрос; очередь результата не блокирует UI.
bool RadioSourceRequestRandom(RadioSource* source);
bool RadioSourcePollEvent(RadioSource* source, RadioSourceEvent* outEvent);
RadioSourcePhase RadioSourceGetPhase(const RadioSource* source);
int32_t RadioSourceGetProgressPermille(const RadioSource* source);
const wchar_t* RadioSourceGetCacheDirectory(const RadioSource* source);

