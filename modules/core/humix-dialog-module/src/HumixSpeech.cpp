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

#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/select.h>

#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include <pocketsphinx.h>

#include "HumixSpeech.hpp"

#include "StreamTTS.hpp"
#include "WavUtil.hpp"

/* Sleep for specified msec */
static void sleep_msec(int32 ms) {
    /* ------------------- Unix ------------------ */
    struct timeval tmo;

    tmo.tv_sec = 0;
    tmo.tv_usec = ms * 1000;

    select(0, NULL, NULL, NULL, &tmo);
}

static const arg_t cont_args_def[] = {
    POCKETSPHINX_OPTIONS,
    /* Argument file. */
    {"-argfile",
     ARG_STRING,
     NULL,
     "Argument file giving extra arguments."},
    {"-adcdev",
     ARG_STRING,
     NULL,
     "Name of audio device to use for input."},
    {"-time",
     ARG_BOOLEAN,
     "no",
     "Print word times in file transcription."},
    {"-cmdproc",
     ARG_STRING,
     "./processcmd.sh",
     "command processor."},
    {"-wav-say",
     ARG_STRING,
     "./voice/interlude/pleasesay1.wav",
     "the wave file of saying."},
    {"-wav-proc",
     ARG_STRING,
     "./voice/interlude/process1.wav",
     "the wave file of processing."},
    {"-wav-bye",
     ARG_STRING,
     "./voice/interlude/bye.wav",
     "the wave file of goodbye."},
    {"-keyword-name",
      ARG_STRING,
      "HUMIX",
      "keyword of the name."},
    {"-lang",
     ARG_STRING,
     "zh-tw",
     "language locale."},

    CMDLN_EMPTY_OPTION
};


static char* sGetObjectPropertyAsString(
        v8::Local<v8::Context> ctx,
        v8::Local<v8::Object> &obj,
        const char* name,
        const char* defaultValue) {

    v8::Local<v8::Value> valObj;
    if ( obj->Get(ctx, Nan::New(name).ToLocalChecked()).ToLocal(&valObj) &&
            !valObj->IsUndefined() &&
            !valObj->IsNull()) {
        v8::String::Utf8Value val(valObj);
        return strdup(*val);
    } else {
        return strdup(defaultValue);
    }
}

HumixSpeech::HumixSpeech(const v8::FunctionCallbackInfo<v8::Value>& args)
        : mThread(0) {
    v8::Local<v8::Object> config = args[0]->ToObject();
    v8::Local<v8::Context> ctx = args.GetIsolate()->GetCurrentContext();
    mState = kReady;

    mCMDProc = sGetObjectPropertyAsString(ctx, config, "cmdproc", "./util/processcmd.sh");
    mWavSay =  sGetObjectPropertyAsString(ctx, config, "wav-say", "./voice/interlude/pleasesay1.wav");
    mWavProc =  sGetObjectPropertyAsString(ctx, config, "wav-proc", "./voice/interlude/process1.wav");
    mWavBye =  sGetObjectPropertyAsString(ctx, config, "wav-bye", "./voice/interlude/bye.wav");
    mLang =  sGetObjectPropertyAsString(ctx, config, "lang", "zh-tw");
    mSampleRate =  sGetObjectPropertyAsString(ctx, config, "samprate", "16000");

    char const *cfg;
    v8::Local<v8::Array> props = config->GetPropertyNames();
    int propsNum = props->Length();
    mArgc = propsNum * 2;
    mArgv = (char**)calloc(mArgc, sizeof(char**));
    int counter = 0;
    for ( int i = 0; i < propsNum; i++ ) {
        v8::Local<v8::Value> valObj;
        if ( props->Get(ctx, i).ToLocal(&valObj) ) {
            //option: need to add '-' prefix as an option
            v8::String::Utf8Value name(valObj);
            char** p = mArgv + counter++;
            *p = (char*)malloc(name.length() + 2);
            sprintf(*p, "-%s", *name);
            if ( config->Get(ctx, valObj).ToLocal(&valObj) &&
                    !valObj->IsNull() &&
                    !valObj->IsUndefined()) {
                //option value
                v8::String::Utf8Value val(valObj);
                p = mArgv + counter++;
                *p = strdup(*val);
            }
        }
    }
    mConfig = cmd_ln_parse_r(NULL, cont_args_def, mArgc, mArgv, TRUE);

    /* Handle argument file as -argfile. */
    if (mConfig && (cfg = cmd_ln_str_r(mConfig, "-argfile")) != NULL) {
        mConfig = cmd_ln_parse_file_r(mConfig, cont_args_def, cfg, FALSE);
    }

    ps_default_search_args(mConfig);
    mPSDecoder = ps_init(mConfig);
    if (mPSDecoder == NULL) {
        cmd_ln_free_r(mConfig);
        mConfig = NULL;
        args.GetIsolate()->ThrowException(v8::Exception::Error(Nan::New("Can't initialize ps").ToLocalChecked()));
    }
    uv_mutex_init(&mAplayMutex);
    uv_mutex_init(&mCommandMutex);
    mStreamTTS = NULL;
    Wrap(args.This());
}

HumixSpeech::~HumixSpeech() {
    if ( mPSDecoder )
        ps_free(mPSDecoder);
    if ( mConfig)
        cmd_ln_free_r(mConfig);
    if (mCMDProc)
        free(mCMDProc);
    if (mWavSay)
        free(mWavSay);
    if (mWavProc)
        free(mWavProc);
    if (mWavBye)
        free(mWavBye);
    if (mSampleRate)
        free(mSampleRate);
    if (mLang)
        free(mLang);

    if ( mArgv ) {
        for ( int i = 0; i < mArgc; i++ ) {
            if ( mArgv[i] ) {
                free(mArgv[i]);
            }
        }
        free(mArgv);
    }
    uv_mutex_destroy(&mAplayMutex);
    uv_mutex_destroy(&mCommandMutex);
    if ( mStreamTTS ) {
        delete mStreamTTS;
    }
    mCB.Reset();
}

/*static*/
void HumixSpeech::sV8New(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    if ( info.Length() != 1 ) {
        info.GetIsolate()->ThrowException(
                v8::Exception::SyntaxError(Nan::New("one argument").ToLocalChecked()));
        return info.GetReturnValue().Set(v8::Undefined(isolate));
    }

    v8::Local<v8::Object> configObj = info[0]->ToObject();

    if ( configObj.IsEmpty() ) {
        info.GetIsolate()->ThrowException(
                v8::Exception::SyntaxError(Nan::New("The first argument shall be an object").ToLocalChecked()));
        return info.GetReturnValue().Set(v8::Undefined(isolate));
    }

    new HumixSpeech(info);
    return info.GetReturnValue().Set(info.This());
}

/*static*/
void HumixSpeech::sStart(const v8::FunctionCallbackInfo<v8::Value>& info) {
    HumixSpeech* hs = Unwrap<HumixSpeech>(info.Holder());
    if ( hs == nullptr ) {
        info.GetIsolate()->ThrowException(v8::Exception::ReferenceError(
                Nan::New("Not a HumixSpeech object").ToLocalChecked()));
        return;
    }

    if ( info.Length() < 1 || !info[0]->IsFunction() ) {
        info.GetIsolate()->ThrowException(v8::Exception::SyntaxError(
                Nan::New("Usage: start(callback)").ToLocalChecked()));
        return;
    }
    hs->Start(info);
}

void
HumixSpeech::Start(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Local<v8::Function> cb = info[0].As<v8::Function>();
    if ( mStreamTTS ) {
        mStreamTTS->SetCB(cb);
    }
    mCB.Reset(info.GetIsolate(), cb);
    uv_thread_create(&mThread, sLoop, this);
}

/*static*/
void HumixSpeech::sStop(const v8::FunctionCallbackInfo<v8::Value>& info) {
    HumixSpeech* hs = Unwrap<HumixSpeech>(info.Holder());
    if ( hs == nullptr ) {
        info.GetIsolate()->ThrowException(v8::Exception::ReferenceError(
                Nan::New("Not a HumixSpeech object").ToLocalChecked()));
        return;
    }

    if ( info.Length() != 0 ) {
        info.GetIsolate()->ThrowException(v8::Exception::SyntaxError(
                Nan::New("Usage: stop()").ToLocalChecked()));
        return;
    }
    hs->Stop(info);
}

void
HumixSpeech::Stop(const v8::FunctionCallbackInfo<v8::Value>& info) {
    mCB.Reset();
    mState = kStop;
    uv_thread_join(&mThread);
}

/*static*/
void HumixSpeech::sPlay(const v8::FunctionCallbackInfo<v8::Value>& info) {
    HumixSpeech* hs = Unwrap<HumixSpeech>(info.Holder());
    if ( hs == nullptr ) {
        info.GetIsolate()->ThrowException(v8::Exception::ReferenceError(
                Nan::New("Not a HumixSpeech object").ToLocalChecked()));
        return;
    }

    if ( info.Length() != 1 || !info[0]->IsString()) {
        info.GetIsolate()->ThrowException(v8::Exception::SyntaxError(
                Nan::New("Usage: play(filename)").ToLocalChecked()));
        return;
    }
    hs->Play(info);
}

void
HumixSpeech::Play(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::String::Utf8Value filename(info[0]);
    uv_mutex_lock(&mAplayMutex);
    mAplayFiles.push(*filename);
    uv_mutex_unlock(&mAplayMutex);
}

/*static*/
void HumixSpeech::sSetupEngine(const v8::FunctionCallbackInfo<v8::Value>& info) {
    HumixSpeech* hs = Unwrap<HumixSpeech>(info.Holder());
    if ( hs == nullptr ) {
        info.GetIsolate()->ThrowException(v8::Exception::ReferenceError(
                Nan::New("Not a HumixSpeech object").ToLocalChecked()));
        return;
    }

    if ( info.Length() != 4 || !info[0]->IsString() ||
            !info[1]->IsString() || !info[2]->IsNumber() ||
            !info[3]->IsFunction()) {
        info.GetIsolate()->ThrowException(v8::Exception::SyntaxError(
                Nan::New("Usage: enableWatson(username, passwd, function)").ToLocalChecked()));
        return;
    }
    v8::Local<v8::Context> ctx = info.GetIsolate()->GetCurrentContext();
    v8::String::Utf8Value username(info[0]);
    v8::String::Utf8Value passwd(info[1]);
    v8::Local<v8::Number> engine = info[2]->ToNumber(ctx).ToLocalChecked();
    hs->mStreamTTS = new StreamTTS(*username, *passwd,
            (StreamTTS::Engine)(engine->Int32Value()),
            false,
            info[3].As<v8::Function>());
}

/*static*/
void HumixSpeech::sLoop(void* arg) {
    HumixSpeech* _this = (HumixSpeech*)arg;
    ps_decoder_t* ps = _this->mPSDecoder;
    ad_rec_t *ad;
    int16 adbuf[2048];
    size_t adbuflen = 2048;
    uint8 in_speech; //utt_started, in_speech;
    int32 k;
    char const *hyp;

    _this->mState = kReady;
    int waitCount = 0;
    int humixCount = 0;
    int samprate = (int) cmd_ln_float32_r(_this->mConfig, "-samprate");
    const char* keywordName = cmd_ln_str_r(_this->mConfig, "-keyword-name");

    printf("keyword-name:%s\n", keywordName);

    WavWriter *wavWriter = NULL;
    setbuf(stdout, NULL);

    if ((ad = ad_open_dev(cmd_ln_str_r(_this->mConfig, "-adcdev"), samprate)) == NULL) {
        E_FATAL("Failed to open audio device\n");
    }
    if (ad_start_rec(ad) < 0) {
        E_FATAL("Failed to start recording\n");
    }

    if (ps_start_utt(_this->mPSDecoder) < 0) {
        E_FATAL("Failed to start utterance\n");
    }
    //utt_started = FALSE;
    printf("READY....\n");

    for (; _this->mState != kStop ;) {
        //read data from rec, we need to check if there is any aplay request from node first
        if (_this->mState != kCommand) {
            uv_mutex_lock(&(_this->mAplayMutex));
            if (!_this->mAplayFiles.empty()) {
                ad_stop_rec(ad);
                std::string file = _this->mAplayFiles.front();
                _this->mAplayFiles.pop();
                printf("play:%s\n", file.c_str());
                {
                    WavPlayer player(file.c_str());
                    player.Play();
                }
                ad_start_rec(ad);
            }
            uv_mutex_unlock(&(_this->mAplayMutex));
        }
        if ((k = ad_read(ad, adbuf, adbuflen)) < 0)
            E_FATAL("Failed to read audio\n");

        //start to process the data we got from rec
        ps_process_raw(ps, adbuf, k, FALSE, FALSE);
        in_speech = ps_get_in_speech(ps);

        switch (_this->mState) {
            case kReady:
                humixCount = 0;
                if (in_speech) {
                    _this->mState = kKeyword;
                    printf("Waiting for keyward: %s...\n", keywordName);
                }
                break;
            case kKeyword:
                if (in_speech) {
                    //keep receiving keyword
                } else {
                    //keyword done
                    ps_end_utt(ps);
                    hyp = ps_get_hyp(ps, NULL);
                    if (hyp != NULL && strcmp(keywordName, hyp) == 0) {
                        _this->mState = kWaitCommand;
                        if (_this->mStreamTTS ) {
                            _this->mStreamTTS->WSConnect();
                        }
                        printf("keyword %s found\n", keywordName);
                        ad_stop_rec(ad);
                        {
                            WavPlayer player(_this->mWavSay);
                            player.Play();
                        }
                        ad_start_rec(ad);
                        printf("Waiting for a command...");
                        humixCount = 0;
                    } else {
                        _this->mState = kReady;
                        printf("READY....\n");
                    }
                    if (ps_start_utt(ps) < 0)
                        E_FATAL("Failed to start utterance\n");
                }
                break;
            case kWaitCommand:
                if (in_speech) {
                    printf("Listening the command...\n");
                    _this->mState = kCommand;
                    if ( _this->mStreamTTS ) {
                        _this->mStreamTTS->ReConnectIfNeeded();
                        _this->mStreamTTS->SendVoiceWav((char*) adbuf, (uint32_t) (k * 2));
                    } else {
                        wavWriter = new WavWriter("/dev/shm/test.wav", 1, samprate);
                        wavWriter->WriteHeader();
                        wavWriter->WriteData((char*) adbuf, (size_t) (k * 2));
                    }
                } else {
                    //increase waiting count;
                    if (++waitCount > 100) {
                        waitCount = 0;
                        if ( _this->mStreamTTS ) {
                            //keep connection
                            _this->mStreamTTS->SendIdleSilent();
                        }
                        if (++humixCount > 20) {
                            //exit humix-loop
                            _this->mState = kReady;
                            if (_this->mStreamTTS ) {
                                _this->mStreamTTS->Stop();
                            }
                            ad_stop_rec(ad);
                            {
                                WavPlayer player(_this->mWavBye);
                                player.Play();
                            }
                            ad_start_rec(ad);
                            printf("READY....\n");
                        } else {
                            printf(".");
                        }
                        ps_end_utt(ps);
                        if (ps_start_utt(ps) < 0)
                            E_FATAL("Failed to start utterance\n");
                    }
                }
                break;
            case kCommand:
                if (in_speech) {
                    //keep receiving command
                    if ( _this->mStreamTTS ) {
                        _this->mStreamTTS->SendVoiceWav((char*) adbuf, (uint32_t) (k * 2));
                    } else {
                        wavWriter->WriteData((char*) adbuf, (size_t) (k * 2));
                    }
                } else {
                    if ( _this->mStreamTTS ) {
                        _this->mStreamTTS->SendVoiceWav((char*) adbuf, (uint32_t) (k * 2));
                        _this->mStreamTTS->SendSilentWav();
                    } else {
                        wavWriter->WriteData((char*) adbuf, (size_t) (k * 2));
                        delete wavWriter;
                        wavWriter = NULL;
                    }
                    //start to process command
                    ps_end_utt(ps);
                    printf("StT processing\n");
                    char msg[1024];
                    ad_stop_rec(ad);
                    {
                        WavPlayer player(_this->mWavProc);
                        player.Play();
                    }
                    if ( !_this->mStreamTTS ) {
                        int result = _this->ProcessCommand(msg, 1024);
                        if (result == 0) {
                            uv_async_t* async = new uv_async_t;
                            async->data = _this;
                            uv_mutex_lock(&(_this->mCommandMutex));
                            _this->mCommands.push(msg);
                            uv_mutex_unlock(&(_this->mCommandMutex));
                            uv_async_init(uv_default_loop(), async,
                                    HumixSpeech::sReceiveCmd);
                            uv_async_send(async);
                        } else {
                            printf("No command found!");
                        }
                    }
                    //once we got command, reset humix-loop
                    humixCount = 0;
                    ad_start_rec(ad);
                    _this->mState = kWaitCommand;
                    printf("Waiting for a command...\n");
                    if (ps_start_utt(ps) < 0)
                        E_FATAL("Failed to start utterance\n");
                }
                break;
            case kStop:
                break;
        }

        sleep_msec(20);
    }
    ad_close(ad);
}

/*static*/
void HumixSpeech::sReceiveCmd(uv_async_t* async) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();

    HumixSpeech* _this = (HumixSpeech*)async->data;
    if ( !_this->mCB.IsEmpty() ) {
        uv_mutex_lock(&(_this->mCommandMutex));
        while( _this->mCommands.size() > 0) {
            std::string cmd = _this->mCommands.front();
            _this->mCommands.pop();
            v8::Local<v8::Value> argv[] = { Nan::New(cmd.c_str()).ToLocalChecked() };
            v8::Local<v8::Function> func = v8::Local<v8::Function>::New(isolate, _this->mCB);
            func->CallAsFunction(ctx, ctx->Global(), 1, argv);
        }
        uv_mutex_unlock(&(_this->mCommandMutex));
    }

    uv_close(reinterpret_cast<uv_handle_t*>(async), HumixSpeech::sFreeHandle);
}

/*static*/
void
HumixSpeech::sFreeHandle(uv_handle_t* handle) {
    delete handle;
}

/*static*/
v8::Local<v8::FunctionTemplate> HumixSpeech::sFunctionTemplate(
        v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(isolate,
            HumixSpeech::sV8New);
    tmpl->SetClassName(Nan::New("HumixSpeech").ToLocalChecked());
    tmpl->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(tmpl, "start", sStart);
    NODE_SET_PROTOTYPE_METHOD(tmpl, "stop", sStop);
    NODE_SET_PROTOTYPE_METHOD(tmpl, "play", sPlay);
    NODE_SET_PROTOTYPE_METHOD(tmpl, "engine", sSetupEngine);

    return scope.Escape(tmpl);
}

int HumixSpeech::ProcessCommand(char* msg, int len) {
    int filedes[2];
    if (pipe(filedes) == -1) {
        printf("pipe error");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    int status;
    if (pid == 0) {
        while ((dup2(filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {
        }
        close(filedes[1]);
        close(filedes[0]);
        int rev = execl(mCMDProc, "processcmd.sh", "/dev/shm/test.wav",
                "/dev/shm/test.flac", mLang, mSampleRate, (char*) NULL);
        if (rev == -1) {
            printf("fork error:%s\n", strerror(errno));
        }
        _exit(1);
    } else {
        waitpid(pid, &status, 0);
    }
    close(filedes[1]);
    if (status == 0 && msg && len > 0) {
        ssize_t outlen = read(filedes[0], msg, len - 1);
        if (outlen > 0) {
            msg[outlen] = 0;
        }
    }
    close(filedes[0]);
    return status;
}



void InitModule(v8::Local<v8::Object> target) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    Nan::HandleScope scope;
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();

    v8::Local<v8::FunctionTemplate> ft = HumixSpeech::sFunctionTemplate(isolate);

    target->Set(ctx, Nan::New("HumixSpeech").ToLocalChecked(),
            ft->GetFunction(ctx).ToLocalChecked());
}

NODE_MODULE(HumixSpeech, InitModule);
