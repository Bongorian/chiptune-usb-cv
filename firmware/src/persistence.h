#pragma once
#include "engine.h"
typedef enum { SAVE_PATCH, SAVE_TRACK, SAVE_PROJECT, SAVE_SYSTEM } SaveKind;
bool settings_save(Settings *s, Engine *e, SaveKind kind, unsigned track, unsigned slot,
                   bool (*commit)(void *), void *ctx);
bool engine_load_project(Engine *e, const Project *p);
