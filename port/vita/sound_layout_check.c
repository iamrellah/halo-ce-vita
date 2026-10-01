/* Compile-only contracts for offline retail sound inspection. */
#include "cseries.h"
#include "sound/sound_definitions.h"
#include <stddef.h>
#define CHECK(name, condition) typedef char name[(condition) ? 1 : -1]
CHECK(sound_size, sizeof(struct sound_definition) == 164);
CHECK(sound_reference, offsetof(struct sound_definition, promotion_sound) == 112);
CHECK(sound_ranges, offsetof(struct sound_definition, pitch_ranges) == 152);
CHECK(range_size, sizeof(struct sound_pitch_range) == 72);
CHECK(range_permutations, offsetof(struct sound_pitch_range, permutations) == 60);
CHECK(permutation_size, sizeof(struct sound_permutation) == 124);
CHECK(permutation_samples, offsetof(struct sound_permutation, samples) == 64);
CHECK(permutation_mouth, offsetof(struct sound_permutation, mouth_data) == 84);
CHECK(permutation_subtitle, offsetof(struct sound_permutation, subtitle_data) == 104);
