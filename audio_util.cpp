#include "audio_util.h"

#include "util_uint64.h"

unsigned long long AudioUtil::AudioFramesToNS(size_t sample_rate, unsigned long long frames)
{
	// 1000000000ULL - ƒ…√Î
	// 1000000ULL - Œ¢√Î
	// 1000ULL - ∫¡√Î
	// 1ULL - √Î
	return util_mul_div64(frames, 1000000000ULL, sample_rate);
}
unsigned long long AudioUtil::NsToAudioFrames(size_t sample_rate, unsigned long long frames)
{
	return util_mul_div64(frames, sample_rate, 1000000000ULL);
}