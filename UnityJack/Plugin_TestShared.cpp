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

#include "AudioPluginUtil.h"
#include "TestSharedLib.cpp"

#define DEBUG_OUT

namespace TestSharedStack
{

enum Param
{
	P_JACK_CHANNEL_INDEX,
	P_OBJECT_MODE,
    P_MIXER_OUT_VOLUME,
    P_NUM
};

struct EffectData
{
    float p[P_NUM];
    float tmpbuffer_in[BUFSIZE];
    float tmpbuffer_out[BUFSIZE];

};

int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
{
    int numparams = P_NUM;
    definition.paramdefs = new UnityAudioParameterDefinition[numparams];
    RegisterParameter(definition, "Jack Channel", "", 0.0f, 64.0f, 0.0f, 1.0f, 1.0f, P_JACK_CHANNEL_INDEX, "The jack channel input number");
	RegisterParameter(definition, "Object/Mono", "", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, P_OBJECT_MODE, "Object mode/downmix to mono (>0.5f) or not (<= 0.5f)");
    RegisterParameter(definition, "Output Volume", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_MIXER_OUT_VOLUME, "Volume output to Unity mixer");

    return numparams;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
{
    EffectData* data = new EffectData;
    memset(data, 0, sizeof(EffectData));
    state->effectdata = data;
    InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);

    // TODO: this should not be neccesary since we are calling
    // client creation from C#
    // Get unique index on start
//    data->p[P_INDEX] = (float)JackClient::getInstance().GenerateIndex();
    return UNITY_AUDIODSP_OK;
}


UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
{
    EffectData* data = state->GetEffectData<EffectData>();
    delete data;
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
{
    EffectData* data = state->GetEffectData<EffectData>();

    for (unsigned int n = 0; n < length; n++)
    {
        for (int i = 0; i < outchannels; i++)
        {
            outbuffer[n * outchannels + i] = inbuffer[n * outchannels + i] * data->p[P_MIXER_OUT_VOLUME];
        }
    }
    
    // WARNING! This works only depending on the framerate
    // configured in Unity and Jack.
    // The "Best Performance" setting gives us BUFSIZE = 1024 samples per channel
    // which corresponds to 1024 samples in Jack
    
#ifdef DEBUG_OUT
	if (data->p[P_OBJECT_MODE] > 0.5f) {
		if (inchannels == 1) {
			JackClient::getInstance().SetData(data->p[P_JACK_CHANNEL_INDEX], inbuffer);
		} else {
			//downmix
			for (int inputSampleIndex = 0, outputSampleIndex = 0; inputSampleIndex < length * inchannels; inputSampleIndex += inchannels, outputSampleIndex++) {
				data->tmpbuffer_out[outputSampleIndex] = 0;
				for (int inputChannelIndex = 0; inputChannelIndex < inchannels; inputChannelIndex++) {
					data->tmpbuffer_out[outputSampleIndex] += inbuffer[inputSampleIndex + inputChannelIndex];
				}
			}
			JackClient::getInstance().SetData(data->p[P_JACK_CHANNEL_INDEX], data->tmpbuffer_out);
		}
	} else {
		// channel-split to Jack's inputs
		for (int inputChannelIndex = 0; inputChannelIndex < inchannels; inputChannelIndex++) {
			for (int inputSampleIndex = 0, outputSampleIndex = 0; inputSampleIndex < length * inchannels; inputSampleIndex += inchannels, outputSampleIndex++) {
				data->tmpbuffer_out[outputSampleIndex] = inbuffer[inputSampleIndex + inputChannelIndex];;
			}
			JackClient::getInstance().SetData(data->p[P_JACK_CHANNEL_INDEX] + inputChannelIndex, data->tmpbuffer_out);
		}
	}
#else
    if (inchannels == 2)
    {
        // upmix
        JackClient::getInstance().GetData( data->p[P_INDEX], data->tmpbuffer_in);

        for (int i = 0,j=0; i < length * 2; i += 2)
        {
            outbuffer[i] = data->tmpbuffer_in[j++];
            outbuffer[i+1] = outbuffer[i];
        }
    } else if (inchannels == 1) {
        JackClient::getInstance().GetData( data->p[P_INDEX], outbuffer);
    }
#endif
    
//    std::cout << "Processing data " << length << " channels " << inchannels << std::endl;
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
{
    EffectData* data = state->GetEffectData<EffectData>();
    data->p[index] = value;
    return UNITY_AUDIODSP_OK;
}
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
{
    EffectData* data = state->GetEffectData<EffectData>();
    if (value != NULL) *value = data->p[index];
    return UNITY_AUDIODSP_OK;
}
int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
{
    return UNITY_AUDIODSP_OK;
}



} //!namespace

extern "C" UNITY_AUDIODSP_EXPORT_API bool CreateClient(int inputs, int outputs)
{
    return TestSharedStack::JackClient::getInstance().createClient(inputs, outputs);
}
extern "C" UNITY_AUDIODSP_EXPORT_API bool DestroyClient()
{
    return TestSharedStack::JackClient::getInstance().destroyClient();
}

extern "C" UNITY_AUDIODSP_EXPORT_API void GetAllData(float* buffer)
{
    TestSharedStack::JackClient::getInstance().GetAllData(buffer);
}

extern "C" UNITY_AUDIODSP_EXPORT_API void SetAllData(float* buffer)
{
    TestSharedStack::JackClient::getInstance().SetAllData(buffer);
}

int FMOD_Main() {
	FMOD::System* system;
	FMOD::Sound* sound1, * sound2, * sound3;
	FMOD::Channel* channel = 0;
	FMOD_RESULT       result;
	unsigned int      version;
	void* extradriverdata = 0;

	Common_Init(&extradriverdata);

	/*
		Create a System object and initialize
	*/
	result = FMOD::System_Create(&system);
	ERRCHECK(result);

	result = system->getVersion(&version);
	ERRCHECK(result);

	if (version < FMOD_VERSION) {
		Common_Fatal("FMOD lib version %08x doesn't match header version %08x", version, FMOD_VERSION);
	}

	result = system->init(32, FMOD_INIT_NORMAL, extradriverdata);
	ERRCHECK(result);

	result = system->createSound(Common_MediaPath("drumloop.wav"), FMOD_DEFAULT, 0, &sound1);
	ERRCHECK(result);

	result = sound1->setMode(FMOD_LOOP_OFF);    /* drumloop.wav has embedded loop points which automatically makes looping turn on, */
	ERRCHECK(result);                           /* so turn it off here.  We could have also just put FMOD_LOOP_OFF in the above CreateSound call. */

	result = system->createSound(Common_MediaPath("jaguar.wav"), FMOD_DEFAULT, 0, &sound2);
	ERRCHECK(result);

	result = system->createSound(Common_MediaPath("swish.wav"), FMOD_DEFAULT, 0, &sound3);
	ERRCHECK(result);

	result = system->playSound(sound1, 0, false, &channel);
	ERRCHECK(result);

	///*
	//	Main loop
	//*/
	//do {
	//	Common_Update();

	//	if (Common_BtnPress(BTN_ACTION1)) {
	//		result = system->playSound(sound1, 0, false, &channel);
	//		ERRCHECK(result);
	//	}

	//	if (Common_BtnPress(BTN_ACTION2)) {
	//		result = system->playSound(sound2, 0, false, &channel);
	//		ERRCHECK(result);
	//	}

	//	if (Common_BtnPress(BTN_ACTION3)) {
	//		result = system->playSound(sound3, 0, false, &channel);
	//		ERRCHECK(result);
	//	}

	//	result = system->update();
	//	ERRCHECK(result);

	//	{
	//		unsigned int ms = 0;
	//		unsigned int lenms = 0;
	//		bool         playing = 0;
	//		bool         paused = 0;
	//		int          channelsplaying = 0;

	//		if (channel) {
	//			FMOD::Sound* currentsound = 0;

	//			result = channel->isPlaying(&playing);
	//			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN)) {
	//				ERRCHECK(result);
	//			}

	//			result = channel->getPaused(&paused);
	//			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN)) {
	//				ERRCHECK(result);
	//			}

	//			result = channel->getPosition(&ms, FMOD_TIMEUNIT_MS);
	//			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN)) {
	//				ERRCHECK(result);
	//			}

	//			channel->getCurrentSound(&currentsound);
	//			if (currentsound) {
	//				result = currentsound->getLength(&lenms, FMOD_TIMEUNIT_MS);
	//				if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN)) {
	//					ERRCHECK(result);
	//				}
	//			}
	//		}

	//		system->getChannelsPlaying(&channelsplaying, NULL);

	//		Common_Draw("==================================================");
	//		Common_Draw("Play Sound Example.");
	//		Common_Draw("Copyright (c) Firelight Technologies 2004-2019.");
	//		Common_Draw("==================================================");
	//		Common_Draw("");
	//		Common_Draw("Press %s to play a mono sound (drumloop)", Common_BtnStr(BTN_ACTION1));
	//		Common_Draw("Press %s to play a mono sound (jaguar)", Common_BtnStr(BTN_ACTION2));
	//		Common_Draw("Press %s to play a stereo sound (swish)", Common_BtnStr(BTN_ACTION3));
	//		Common_Draw("Press %s to quit", Common_BtnStr(BTN_QUIT));
	//		Common_Draw("");
	//		Common_Draw("Time %02d:%02d:%02d/%02d:%02d:%02d : %s", ms / 1000 / 60, ms / 1000 % 60, ms / 10 % 100, lenms / 1000 / 60, lenms / 1000 % 60, lenms / 10 % 100, paused ? "Paused " : playing ? "Playing" : "Stopped");
	//		Common_Draw("Channels Playing %d", channelsplaying);
	//	}

	//	Common_Sleep(50);
	//} while (!Common_BtnPress(BTN_QUIT));

	/*
		Shut down
	*/
	//result = sound1->release();
	//ERRCHECK(result);
	//result = sound2->release();
	//ERRCHECK(result);
	//result = sound3->release();
	//ERRCHECK(result);
	//result = system->close();
	//ERRCHECK(result);
	//result = system->release();
	//ERRCHECK(result);

	//Common_Close();

	return 0;
}
