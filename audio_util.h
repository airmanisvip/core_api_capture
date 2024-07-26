#pragma once

class AudioUtil
{
public:
	/*
	 sample_rate : 采样率
	 frames : 帧数量
	 返回值 : frames对应的纳秒
	*/
	static unsigned long long AudioFramesToNS(size_t sample_rate, unsigned long long frames);
	static unsigned long long NsToAudioFrames(size_t sample_rate, unsigned long long frames);
};