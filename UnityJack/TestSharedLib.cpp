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
	
    int SetData(int idx, std::vector<float> buffer) {
		outputBuffer.push_back(buffer);
        
        // Increase the index until the mixed buffer is filled.
        track++;

        // if filled send to ringbuffer, restart index
		auto JackOutputChannelCount = GetJackOutputChannelCount();
        if (track == JackOutputChannelCount) {
			if (!initialized) {
				auto success = createClient(0, JackOutputChannelCount);
				if (!success) {
					return 0;
				}
			}

			float* ptrMixedBuffer = mixedBuffer;
			for (size_t i = 0; i < outputBuffer.size(); i++) {
				auto elementSize = outputBuffer[i].size();
				for (size_t j = 0; j < elementSize; j++) {
					mixedBuffer[i+(j * outputBuffer.size())] = outputBuffer[i][j];
				}
			}
			outputBuffer.clear();

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
    
    
private:
    
    JackClient() {
        std::cout << "Trying to create" << std::endl;
    }
    

public:
    JackClient(JackClient const&) = delete;
    void operator=(JackClient const&)  = delete;
	// Only call this from the Unity jack send plugin. Keeps track of how many plugins are instantiated and with what channel count
	void RegisterJackOutputChannelFromMixerPlugin(int channels) {
		std::cout << "JACK: Input channel number is " << channels << std::endl;
		jackOutputChannels.push_back(channels);
		std::cout << "JACK: channels pushed back!" << std::endl;
	}
	int IncreaseJackPluginInstanceIndex() {
		return jackPluginInstanceIndex++;
	}
	int GetJackOutputChannelCount() {
		int channels = 0;
		for (size_t i = 0; i < jackOutputChannels.size(); i++) {
			channels += jackOutputChannels[i];
		}
		std::cout << "Channels: " << channels << std::endl;
		if (channels < jackPluginInstanceIndex) {
			std::cout << "Returning channels: " << jackPluginInstanceIndex << std::endl;
			return jackPluginInstanceIndex;
		} else {
			std::cout << "Returning Channels: " << channels << std::endl;
			return channels;
		}
	}
	int GetJackOutputTracks() {
		return jackOutputChannels.size();
	}
	void ResetOutputTrackCount() {
		jackPluginInstanceIndex = 0;
		jackOutputChannels.clear();
	}

private:

    // TODO: use better this? http://stackoverflow.com/questions/35008089/elegantly-define-multi-dimensional-array-in-modern-c
    std::unique_ptr<InternalJackClient> client;
	// The buffer containing the audio of all channels/jack plugins (interleaved)
    float *mixedBuffer;
    float *mixedBufferIn;
	std::vector<std::vector<float>> outputBuffer;

    int foo = 5;
    int track;
    bool initialized;
    int _inputs, _outputs;
	std::vector<int> jackOutputChannels;
	int jackPluginInstanceIndex = 0;
};
    
} // !namespace TestSharedStack


