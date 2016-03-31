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
#include <sndfile.h>
#include "StreamTTS.hpp"

#include <fstream>

FLACEncoder::FLACEncoder(StreamTTS* tts)
    : FLAC::Encoder::Stream(), mTTS(tts), mReady(false),
      mHeaderIndex(0), mHeaderSent(false) {
    std::ifstream ifs ("./voice/interlude/pleasesay1.wav", std::ifstream::in);
    ifs.read(mWavHeader, 44);
    ifs.close();
    mWavHeader[40] = 0x60;
    mWavHeader[41] = 0x6d;
    mWavHeader[42] = 0x0;
    mWavHeader[43] = 0x0;
}

FLAC__StreamEncoderInitStatus
FLACEncoder::init() {
    FLAC__int32 buff[22];
    FLAC__StreamEncoderInitStatus rev = FLAC::Encoder::Stream::init();
    //write wav header
    for(uint32_t i = 0; i < 22; i++) {
        buff[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)mWavHeader[i*2+1] << 8) | (FLAC__int16)mWavHeader[i*2]);
    }
    process_interleaved(buff, 22);
    return rev;
}

bool
FLACEncoder::finish() {
    mHeaderIndex = 0;
    mReady = false;
    mHeaderSent = false;
    return FLAC::Encoder::Stream::finish();
}

FLACEncoder::~FLACEncoder() {
    if (mReady) {
        FLAC::Encoder::Stream::finish();
    }
}

bool
FLACEncoder::Init(uint32_t rate, uint8_t channel)
{
    bool rev = true;
    rev &= set_verify(false);
    rev &= set_compression_level(5);
    rev &= set_channels(channel);
    rev &= set_bits_per_sample(16);//S16_LE
    rev &= set_sample_rate(rate);
    rev &= set_blocksize(1152);
    rev &= set_max_lpc_order(8);
    rev &= set_max_residual_partition_order(5);
    rev &= set_loose_mid_side_stereo(false);
    rev &= set_total_samples_estimate(0);
    mReady = rev;
    return rev;
}

//std::ofstream debug("myflac.flac", std::ofstream::out | std::ofstream::trunc);

FLAC__StreamEncoderWriteStatus
FLACEncoder::write_callback(const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame) {
//    printf("size:%lu, frame:%u\n", bytes, current_frame);
//    if ( current_frame < 50) {
//        debug.write((char*)buffer, bytes);
//    } else {
//        debug.close();
//    }

    if ( samples == 0 ) {
        memcpy(mHeader + mHeaderIndex, buffer, bytes);
        mHeaderIndex += bytes;
        return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
    } else {
        if ( !mHeaderSent ) {
            mTTS->SendVoiceRaw((char*)mHeader, mHeaderIndex);
            mHeaderSent = true;
        }
        mTTS->SendVoiceRaw((char*)buffer, bytes);
        return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
    }
}

StreamTTS::StreamTTS(const char* username, const char* passwd, Engine engine, bool flac, v8::Local<v8::Function> func)
        : mUserName(strdup(username)), mPasswd(strdup(passwd)),
          mEngine(engine) {

    v8::Isolate* isolate = func->CreationContext()->GetIsolate();
    mFunc.Reset(isolate, func);
    SF_INFO sfinfo;
    SNDFILE* silent = sf_open("./voice/interlude/empty.wav", SFM_READ, &sfinfo);
    if (!silent) {
        printf("can't open silent wav: %s\n", sf_strerror(NULL));
        return;
    }
    /*sf_count_t count = */sf_readf_short(silent, (short*) mSilent, ONE_SEC_FRAMES)/* * 2*/;
//    if ( engine == kWatson ) {
//        char p = '0';
//        for (sf_count_t i = 0; i < count ; i+=2) {
//            p = mSilent[i];
//            mSilent[i] = mSilent[i+1];
//            mSilent[i+1] = p;
//        }
//    }
    sf_close(silent);
    uv_mutex_init(&mQueueMutex);
    //this will be delete in uv_close callback
    mWriteAsync = new uv_async_t;
    mWriteAsync->data = this;
    uv_async_init(uv_default_loop(), mWriteAsync,
            sWrite);
    mEncoder = NULL;
    if ( flac ) {
        mEncoder = new FLACEncoder(this);
    }
    mConnCB.Reset(isolate, GetListeningFunction(isolate));
}

StreamTTS::~StreamTTS() {
    free(mUserName);
    free(mPasswd);
    mObj.Reset();
    mFunc.Reset();
    mCB.Reset();
    mConnCB.Reset();
    mConn.Reset();
    uv_close(reinterpret_cast<uv_handle_t*>(mWriteAsync), sFreeHandle);
    uv_mutex_destroy(&mQueueMutex);
    delete mEncoder;
}

/*static*/
void
StreamTTS::sListening(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Local<v8::Context> ctx = info.GetIsolate()->GetCurrentContext();
    v8::Local<v8::Object> data = info.Data()->ToObject(ctx).ToLocalChecked();
    assert(data->InternalFieldCount() > 0);
    StreamTTS* _this = reinterpret_cast<StreamTTS*>(data->GetAlignedPointerFromInternalField(0));
    if ( _this->mEngine == kWatson ) {
        if ( info.Length() == 1 ) {
            //connect event
            _this->mConn.Reset(info.GetIsolate(), info[0]->ToObject(ctx).ToLocalChecked());
        } else if ( info.Length() == 2 ) {
            //connection-close event
            _this->mObj.Reset();
            _this->mConn.Reset();
        }
    } else if ( _this->mEngine == kGoogle ) {
        //speech-closed event
        _this->mObj.Reset();
    }
}

v8::Local<v8::Function>
StreamTTS::GetListeningFunction(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::ObjectTemplate> otmpl = v8::ObjectTemplate::New(isolate);
    otmpl->SetInternalFieldCount(1);
    v8::Local<v8::Object> obj = otmpl->NewInstance(ctx).ToLocalChecked();
    obj->SetAlignedPointerInInternalField(0, this);
    return scope.Escape(v8::Function::New(ctx, sListening, obj, 1).ToLocalChecked());
}

void
StreamTTS::CreateSession() {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::Function> func = v8::Local<v8::Function>::New(isolate, mFunc);
    v8::Local<v8::Function> cb = v8::Local<v8::Function>::New(isolate, mCB);
    v8::Local<v8::String> username = v8::String::NewFromUtf8(isolate, mUserName);
    v8::Local<v8::String> passwd = v8::String::NewFromUtf8(isolate, mPasswd);
    v8::Local<v8::Value> args[] = { username, passwd, cb };
    v8::Local<v8::Value> rev;
    if ( func->Call(ctx, ctx->Global(),3, args).ToLocal(&rev) ) {
        v8::Local<v8::Object> session = rev->ToObject(ctx).ToLocalChecked();
        mObj.Reset(isolate, session);
        if ( mEngine == kWatson ) {
            //watson need the connection object to perform the close();
            v8::Local<v8::Value> cb[] = { Nan::New("connect").ToLocalChecked(),
                    v8::Local<v8::Function>::New(isolate, mConnCB)};
            Nan::MakeCallback(session, "on", 2, cb);
            v8::Local<v8::Value> cb2[] = { Nan::New("connection-close").ToLocalChecked(),
                    v8::Local<v8::Function>::New(isolate, mConnCB)};
            Nan::MakeCallback(session, "on", 2, cb2);
        } else if ( mEngine == kGoogle ) {
            v8::Local<v8::Value> cb[] = { Nan::New("speech-closed").ToLocalChecked(),
                    v8::Local<v8::Function>::New(isolate, mConnCB)};
            Nan::MakeCallback(session, "on", 2, cb);
        }
        if ( mEncoder ) {
            mEncoder->Init(16000, 1);
            mEncoder->init();
        }
    }
}

/*static*/
void StreamTTS::sFreeHandle(uv_handle_t* handle) {
    delete handle;
}

/*static*/
void StreamTTS::sCreateSession(uv_async_t* handle) {
    StreamTTS* _this = (StreamTTS*)handle->data;
    _this->CreateSession();
    uv_close(reinterpret_cast<uv_handle_t*>(handle), sFreeHandle);
}

/*static*/
void StreamTTS::sWrite(uv_async_t* handle) {
    StreamTTS* _this = reinterpret_cast<StreamTTS*>(handle->data);
    _this->Write();
}

void
StreamTTS::WSConnect() {
    uv_async_t* async = new uv_async_t;
    async->data = this;
    uv_async_init(uv_default_loop(), async,
            sCreateSession);
    uv_async_send(async);
}

void StreamTTS::ReConnectIfNeeded() {
    if ( mObj.IsEmpty() ) {
        WSConnect();
    }
}

void
StreamTTS::Stop() {
    uv_async_t* async = new uv_async_t;
    async->data = this;
    uv_async_init(uv_default_loop(), async,
            sCloseSession);
    uv_async_send(async);
}

/*static*/
void StreamTTS::sCloseSession(uv_async_t* handle) {
    StreamTTS* _this = (StreamTTS*)handle->data;
    _this->CloseSession();
    uv_close(reinterpret_cast<uv_handle_t*>(handle), sFreeHandle);
}

void
StreamTTS::CloseSession() {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if ( mEngine == kWatson && !mConn.IsEmpty() ) {
        v8::Local<v8::Object> conn = v8::Local<v8::Object>::New(isolate, mConn);
        Nan::MakeCallback(conn, "close", 0, NULL);
    } else if ( mEngine == kGoogle && !mObj.IsEmpty() ) {
        v8::Local<v8::Object> req = v8::Local<v8::Object>::New(isolate, mObj);
        Nan::MakeCallback(req, "end", 0, NULL);
    }
    if ( mEncoder ) {
        delete mEncoder;
        mEncoder = new FLACEncoder(this);
    }
    mConn.Reset();
    mObj.Reset();
}

void
StreamTTS::Write() {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    if ( mObj.IsEmpty() ) {
        return;
    }
    bool cont = false;
    WriteData* data = NULL;
    do {
        data = NULL;
        uv_mutex_lock(&mQueueMutex);
        if ( mWriteQueue.size() ) {
            data = mWriteQueue.front();
            mWriteQueue.pop();
        }
        uv_mutex_unlock(&mQueueMutex);
        if ( data ) {
            v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(isolate, mObj);
            v8::Local<v8::Object> buff;
            if ( Nan::NewBuffer(data->mData, data->mSize, sFreeCallback, data).ToLocal(&buff) ) {
                v8::Local<v8::Value> args[] = { buff };
                Nan::MakeCallback(obj, "write", 1, args);
            }
        }
        uv_mutex_lock(&mQueueMutex);
        cont = (mWriteQueue.size() > 0);
        uv_mutex_unlock(&mQueueMutex);
    } while (cont);
}

/*static*/
void StreamTTS::sFreeCallback(char* data, void* hint) {
    WriteData* wd = (WriteData*)hint;
    delete wd;
}

void
StreamTTS::SendVoiceRaw(char* data, uint32_t length) {
        uv_mutex_lock(&mQueueMutex);
        mWriteQueue.push(new WriteData(data, length, this, true, false));
        uv_mutex_unlock(&mQueueMutex);
        uv_async_send(mWriteAsync);
}

void
StreamTTS::EncodeWav(char* data, uint32_t length, bool be) {
    uint32_t frames = length/2;
    if ( be ) {
        for(uint32_t i = 0; i < frames; i++) {
            mBuff[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)data[i*2] << 8) | (FLAC__int16)data[i*2+1]);
        }
    } else {
        for(uint32_t i = 0; i < frames; i++) {
            mBuff[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)data[i*2+1] << 8) | (FLAC__int16)data[i*2]);
        }
    }
    mEncoder->process_interleaved(mBuff, frames);
}

void
StreamTTS::SendVoiceWav(char* data, uint32_t length) {
    if ( length > 0 ) {
        if ( mEncoder ) {
            EncodeWav(data, length, false);
        } else {
            uv_mutex_lock(&mQueueMutex);
            mWriteQueue.push(new WriteData(data, length, this, true, mEngine == kWatson));
            uv_mutex_unlock(&mQueueMutex);
            uv_async_send(mWriteAsync);
        }
    }
}

void
StreamTTS::SendSilentWav() {
    if ( mEncoder ) {
        EncodeWav(mSilent, ONE_SEC_FRAMES*2, false);
    } else {
        uv_mutex_lock(&mQueueMutex);
        mWriteQueue.push(new WriteData(mSilent, ONE_SEC_FRAMES*2, this, false));
        uv_mutex_unlock(&mQueueMutex);
        uv_async_send(mWriteAsync);
    }
}

void
StreamTTS::SendIdleSilent() {
    SendSilentWav();
}
