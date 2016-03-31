/*******************************************************************************
* Copyright (c) 2015,2016 IBM Corp.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef SRC_HUMIXSPEECH_HPP_
#define SRC_HUMIXSPEECH_HPP_

#include <queue>
#include <string>

#include <nan.h>

class StreamTTS;

class HumixSpeech : public Nan::ObjectWrap{
public:
    HumixSpeech(const v8::FunctionCallbackInfo<v8::Value>& args);
    ~HumixSpeech();

    typedef enum {
        kReady,
        kKeyword,
        kWaitCommand,
        kCommand,
        kStop
    } State;

    static v8::Local<v8::FunctionTemplate> sFunctionTemplate(
            v8::Isolate* isolate);
private:
    static void sV8New(const v8::FunctionCallbackInfo<v8::Value>& info);
    static void sStart(const v8::FunctionCallbackInfo<v8::Value>& info);
    static void sPlay(const v8::FunctionCallbackInfo<v8::Value>& info);
    static void sStop(const v8::FunctionCallbackInfo<v8::Value>& info);
    static void sSetupEngine(const v8::FunctionCallbackInfo<v8::Value>& info);

    void Start(const v8::FunctionCallbackInfo<v8::Value>& info);
    void Stop(const v8::FunctionCallbackInfo<v8::Value>& info);
    void Play(const v8::FunctionCallbackInfo<v8::Value>& info);

    static void sLoop(void* arg);
    int ProcessCommand(char* msg, int len);

    static void sReceiveCmd(uv_async_t* handle);
    static void sFreeHandle(uv_handle_t* handle);

    State mState;
    ps_decoder_t *mPSDecoder;
    cmd_ln_t *mConfig;
    char* mCMDProc;
    char* mWavSay;
    char* mWavProc;
    char* mWavBye;
    char* mLang;
    char* mSampleRate;
    int mArgc;
    char** mArgv;
    uv_thread_t mThread;
    uv_mutex_t mAplayMutex;
    uv_mutex_t mCommandMutex;
    v8::Persistent<v8::Function> mCB;
    std::queue<std::string> mAplayFiles;
    std::queue<std::string> mCommands;
    StreamTTS* mStreamTTS;
};



#endif /* SRC_HUMIXSPEECH_HPP_ */
