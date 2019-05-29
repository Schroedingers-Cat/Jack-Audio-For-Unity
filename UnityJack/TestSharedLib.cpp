// Copyright (C) 2016  Rodrigo Diaz
// 
// This file is part of JackAudioUnity.
// 
// JackAudioUnity is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// JackAudioUnity is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with JackAudioUnity.  If not, see <http://www.gnu.org/licenses/>.
// 

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
#define UNITY_WIN 1
#elif defined(__MACH__) || defined(__APPLE__)
#define UNITY_OSX 1
#elif defined(__ANDROID__)
#define UNITY_ANDROID 1
#elif defined(__linux__)
#define UNITY_LINUX 1
#endif

#include <stdio.h>
#include <memory> //for std::unique_ptr
#include <map>

#if UNITY_OSX | UNITY_LINUX
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <unistd.h>
    #include <string.h>
#elif UNITY_WIN
    #include <windows.h>
	#define _STDINT_H //for jack int definition
#endif

#if defined(__GNUC__) || defined(__SNC__)
#define TESTSHARED_ALIGN(val) __attribute__((aligned(val))) __attribute__((packed))
#elif defined(_MSC_VER)
#define TESTSHARED_ALIGN(val) __declspec(align(val))
#else
#define TESTSHARED_ALIGN(val)
#endif

#include "InternalJackClient.h"
#include <array>

// #define TRACKS 16
#define BUFSIZE 1024
template <typename T, int M, int N> using array2d = std::array<std::array<T, N>, M>;

namespace TestSharedStack
{

class JackClient 
{
    
public:
    static JackClient& getInstance()
    {
        static JackClient instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    int SetAllData(float* buffer) {
        
        if (!initialized) return 0;

        client->setAudioBuffer(buffer);
        return 0;
    }
	
    int SetData(int idx, float* buffer) {
		auto JackOutputChannelCount = GetJackOutputChannelCount();

		if (!initialized) {
			delayedInitOutputBuffer.push_back(std::vector<float>(buffer[0], buffer[BUFSIZE]));
		} else {
			for (size_t i = 0; i < BUFSIZE; i++) {
				mixedBuffer[(i * JackOutputChannelCount) + idx] = buffer[i];
			}
		}
		
        
        // Increase the index until the mixed buffer is filled.
        track++;

        // if filled send to ringbuffer, restart index
        if (track == JackOutputChannelCount) {
			if (!initialized) {
				auto success = createClient(0, JackOutputChannelCount);
				if (!success) {
					return 0;
				}

				for (size_t channelIdx = 0; channelIdx < delayedInitOutputBuffer.size(); channelIdx++) {
					for (size_t sampleFrameIdx = 0; sampleFrameIdx < delayedInitOutputBuffer[channelIdx].size(); sampleFrameIdx++) {
						mixedBuffer[channelIdx + (sampleFrameIdx * delayedInitOutputBuffer.size())] = delayedInitOutputBuffer[channelIdx][sampleFrameIdx];
					}
				}
			}

            client->setAudioBuffer(mixedBuffer);
            track = 0;
        }
        
        return 0;
    }
    
    void GetAllData(float* buffer) {
        if (!initialized) return;
        client->getAudioBuffer(buffer);
    }
    
    int GetData(int idx, float* buffer) {
    
        if (!initialized) return 0;

        client->getAudioBuffer(mixedBufferIn);
        
        for (int i = 0; i < BUFSIZE; i++) {
            buffer[i] = mixedBufferIn[(i * _inputs) + idx];
        }
        
        return 0;
    }
    
    bool createClient(int inputs, int outputs)
    {
        if (!initialized){
            std::cout << "Creating Client " << inputs << " " << outputs << std::endl;
            _inputs = inputs;
            _outputs = outputs;
            client.reset(new InternalJackClient("Unity3D",inputs,outputs));

            mixedBuffer = (float*)malloc(_outputs * BUFSIZE * sizeof(float));
            mixedBufferIn = (float*)malloc(_inputs * BUFSIZE * sizeof(float));
            
            initialized = true;

        }
        return initialized;
    }
    
    bool destroyClient()
    {
        std::cout << "Destroying" << std::endl;
        if (initialized) {
            initialized = false; // important: initialized flag must be false before resetting the client.
            client.reset();
            free(mixedBuffer);
            free(mixedBufferIn);
        }
        return initialized;
    }

	bool ChangePortNumber() {
		destroyClient();
		return createClient(0, GetJackOutputChannelCount());
	}
    
    
private:
    
    JackClient() {
        std::cout << "Trying to create" << std::endl;
    }
    

public:
    JackClient(JackClient const&) = delete;
    void operator=(JackClient const&)  = delete;
	// Only call this from the Unity jack send plugin. Keeps track of how many plugins are instantiated and with what channel count
	void RegisterJackOutputChannelFromMixerPlugin(int instanceCount, int channels) {
		jackPluginOutputChannels.insert(std::make_pair(instanceCount, channels));
		ChangePortNumber();
	}
	int IncreaseJackPluginInstanceCount() {
		ChangePortNumber();
		return jackPluginInstanceCount++;
	}
	int GetJackOutputChannelCount() {
		int channels = 0;
		for (auto i : jackPluginOutputChannels)
		{
			channels += i.second;
		}

		if (channels < jackPluginInstanceCount) {
			return jackPluginInstanceCount;
		} else {
			return channels;
		}
	}
	int GetJackOutputTracks() {
		return jackPluginOutputChannels.size();
	}
	void ResetOutputTrackCount() {
		jackPluginInstanceCount = 0;
		jackPluginOutputChannels.clear();
	}

private:

    // TODO: use better this? http://stackoverflow.com/questions/35008089/elegantly-define-multi-dimensional-array-in-modern-c
    std::unique_ptr<InternalJackClient> client;
	// The buffer containing the audio of all channels/jack plugins (interleaved)
    float *mixedBuffer;
    float *mixedBufferIn;
	// The first mixed buffer will be temporarily assigned into this until the channel count is known
	std::vector<std::vector<float>> delayedInitOutputBuffer;

    int foo = 5;
    int track;
    bool initialized;
    int _inputs, _outputs;
	// The number of output channels (value) of each jack plugin instance (key)
	std::map<int, int> jackPluginOutputChannels;
	int jackPluginInstanceCount = 0;
};
    
} // !namespace TestSharedStack


