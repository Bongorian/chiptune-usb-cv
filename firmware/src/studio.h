#pragma once
#include "engine.h"
extern const char *const st_model_names[ST_MODELS];
extern const char *const st_param_names[ST_PARAMS];
void studio_default(StScene *scene);
void studio_sound_default(StSound *sound, unsigned model);
void studio_factory(unsigned index, StSound *sound, char name[9]);
bool studio_sound_load(Engine *engine, unsigned part, const StSound *sound, const char name[9]);
bool studio_single(Engine *engine, unsigned part, unsigned channel);
bool studio_is_single(const Engine *engine, unsigned part);
bool studio_valid(const StScene *scene);
bool studio_sound_valid(const StSound *sound);
bool studio_plan(const StScene *scene, uint16_t outputs[ST_PARTS], uint16_t resources[ST_PARTS],
                 unsigned *conflict);
void studio_activate(Engine *engine);
void studio_reset(Engine *engine);
bool studio_apply(Engine *engine, const StScene *scene);
void studio_message(Engine *engine, unsigned source, const uint8_t *message, unsigned length);
void studio_tick(Engine *engine, uint32_t elapsed);
void studio_panic(Engine *engine, int source);
void studio_cv(Engine *engine, unsigned input, float volts, bool gate, bool fresh);
void studio_test(Engine *engine, unsigned part, unsigned note, unsigned mode);
void studio_run(Engine *engine, bool run);
unsigned studio_note_count(const Engine *engine, unsigned part);
unsigned studio_owner(const Engine *engine, unsigned voice);
unsigned studio_limit(unsigned model);
float studio_env_clock(float hz, unsigned shape, unsigned steps);
void studio_realtime(Engine *engine, uint8_t status, unsigned source);
void studio_program(Engine *engine, unsigned source, unsigned channel, unsigned program);
void studio_control(Engine *engine, unsigned source, unsigned channel, unsigned cc, unsigned value);
