#pragma once

class AudioUtil
{
public:
	/*
	 sample_rate : ������
	 frames : ֡����
	 ����ֵ : frames��Ӧ������
	*/
	static unsigned long long AudioFramesToNS(size_t sample_rate, unsigned long long frames);
	static unsigned long long NsToAudioFrames(size_t sample_rate, unsigned long long frames);
};