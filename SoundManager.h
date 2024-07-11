#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL.h>
#include <vector>
#include <mutex>
#include "nlohmann/json.hpp"

// This singleton class manages the Apple 2 speaker sound
// All it needs is to be sent EventReceived(bool isC03x=false) on each cycle.
// Any time the passed in param is true, the speaker is switched low<->high
// and kept in that position until the next param change.

// Internally it uses a ringbuffer to store samples to send to the audio subsystem.
// It pushes to the audio subsystem the samples when it reaches enough samples to fill SM_BUFFER_SIZE.
// If there aren't enough samples, SDL2 automatically inserts silence.
// If SDL2 isn't capable of processing samples at the speed they're pushed to the audio system,
// we drop samples once its queue is above SM_BUFFER_SIZE*2.

// TODO: 	Dynamically calculate SM_DEFAULT_CYCLES_PER_SAMPLE based on the Apple 2 region's clock cycle.
//			It is 1/(region_cycle_length*SM_SAMPLE_RATE) , i.e. 1/(usec/cycle*samples/usec)

const uint32_t SM_SAMPLE_RATE = 44100; 					// Audio sample rate
const uint32_t SM_BUFFER_SIZE = 1024;					// Default sample buffer size to send to SDL Audio
const uint32_t SM_RINGBUFFER_SIZE = 1'000'000;			// Audio buffer size - At least 4 seconds of speaker clicks (each 0xC03X is 4 cycles)
const uint32_t SM_STARTING_DELTA_READWRITE = SM_BUFFER_SIZE * 3;	// Starting difference between the 2 pointers
const uint32_t SM_DEFAULT_CYCLES_PER_SAMPLE = 231400;	// Default count of cycles per 44.1kHz sample
const uint32_t SM_CYCLE_MULTIPLIER = 10000;				// Sampling multiplier for cycles

class SoundManager {
public:
	~SoundManager();
	void Initialize();
	void Enable() { bIsEnabled = true; };
	void Disable() { bIsEnabled = false; };
	void BeginPlay();
	void StopPlay();
	bool IsPlaying();
	void EventReceived(bool isC03x=false);	// Received any event -- if isC03x then the event is a 0xC03x
	
	// ImGUI and prefs
	void DisplayImGuiChunk();
	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);
	
	// public singleton code
	static SoundManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new SoundManager(SM_SAMPLE_RATE, SM_BUFFER_SIZE);
		return s_instance;
	}
private:
	static SoundManager* s_instance;
	SoundManager(uint32_t sampleRate, uint32_t bufferSize);
	
	static void AudioCallback(void* userdata, uint8_t* stream, int len);
	void QueueAudio();
	
	std::mutex pointerMutex;
	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	uint32_t cyclesPerSample;					// Around 23 * SM_CYCLE_MULTIPLIER. Increases or decreases based on how quickly reads catch up
	uint32_t sampleRate;
	int bufferSize;
	bool bIsEnabled = true;						// Did user enable speaker through HDMI?
	bool bIsPlaying;							// Is the audio playing?
	float sampleAverage;
	uint32_t sampleCycleCount;					// Varies around 23 * SM_CYCLE_MULTIPLIER
	uint32_t cyclesHigh;							// speaker cycles that are on for each sample
	bool bIsLastHigh;								// Did the last on cycle go positive or negative?
	float soundRingbuffer[SM_RINGBUFFER_SIZE];// 5 seconds of buffer to feed the 44.1kHz audio
	uint32_t sb_index_read;						// index in soundRingbuffer where read should start
	uint32_t sb_index_write;					// index in soundRingbuffer where write should start
	
	int lastQueuedSampleCount;
};

#endif // SOUNDMANAGER_H