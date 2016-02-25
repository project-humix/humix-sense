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

#ifndef SRC_STREAMTTS_HPP_
#define SRC_STREAMTTS_HPP_

#include <nan.h>

#include <queue>
#include "FLAC++/metadata.h"
#include "FLAC++/encoder.h"

#define ONE_SEC_FRAMES 16000

class StreamTTS;

class FLACEncoder: public FLAC::Encoder::Stream {
public:
    FLACEncoder(StreamTTS* tts);
    ~FLACEncoder();

    bool Init(uint32_t rate, uint8_t channel);
    FLAC__StreamEncoderInitStatus init();
    bool finish();


protected:
    FLAC__StreamEncoderWriteStatus write_callback(const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame);

    StreamTTS* mTTS;
    bool mReady;
    char mHeader[4096];
    size_t mHeaderIndex;
    bool mHeaderSent;
    char mWavHeader[44];
};

/**
 * This is a Wrapper class for the JS function.
 * It use the JS function to create TTS session and write
 * wav file to the websocket/http.
 */
class StreamTTS {
public:
    enum Engine {
        kGoogle = 0,
        kWatson = 1
    };
    /**
     * create a Watson TTS wrapper with the username and passwd.
     * the most important part is the Watson JS function
     */
    StreamTTS(const char* username, const char* passwd,
            Engine engine, bool flac, v8::Local<v8::Function> func);

    ~StreamTTS();

    /**
     * It's a data structure to store the wav raw data in network byte order (BE)
     */
    class WriteData {
    public:
        /**
         * store S16_BE data to be sent over network, it will do
         * LE to BE converting if the alloc=true and le=true
         */
        WriteData(const char* data, uint32_t size, StreamTTS* tts, bool alloc, bool le = true)
                : mAlloced(alloc){
            if ( alloc ) {
                mData = (char*)malloc(size);
                memcpy(mData, data, size);
//                if ( le ) {
//                    for ( uint32_t i = 0; i < size ; i+=2) {
//                        char t = mData[i];
//                        mData[i] = mData[i+1];
//                        mData[i+1] = t;
//                    }
//                }
            } else {
                mData = const_cast<char*>(data);
            }
            mSize = size;
            mThis = tts;
        }
        ~WriteData() {
            if ( mAlloced ) {
                free (mData);
            }
        }
        char* mData;
        uint32_t mSize;
        StreamTTS* mThis;
        bool mAlloced;
    };

    /**
     * create ws connection. need to be called in main loop
     */
    void WSConnect();

    void ReConnectIfNeeded();

    /**
     * Set the callback function. need to be called in main loop
     */
    void SetCB(v8::Local<v8::Function> cb) {
        mCB.Reset(cb->CreationContext()->GetIsolate(), cb);
    }

    /**
     * stop the session if there is. This will create an
     * async handle and perform the conn.close() in main loop
     */
    void Stop();

    /**
     * Send the Wav data to server via websocket.
     * it will send an async and call the ws.write() in main loop
     */
    void SendVoiceWav(char* data, uint32_t length);

    /**
     * Send the silent pause wav data to server via websocket.
     * it will send any aynnc and call the ws.write() in main loop
     */
    void SendSilentWav();

    void SendIdleSilent();

    void SendVoiceRaw(char* data, uint32_t length);
private:

    void EncodeWav(char* data, uint32_t length, bool be = true);
    /**
     * callback for the async handle for calling
     * the Watson JS function to create TTS session
     */
    static void sCreateSession(uv_async_t* handle);

    /**
     * callback for the async handle for close
     * the web socket connection
     */
    static void sCloseSession(uv_async_t* handle);

    /**
     * callback for the async handle for calling
     * the ws.write() in main loop
     */
    static void sWrite(uv_async_t* handle);

    /**
     * callback for async handle's clean up
     */
    static void sFreeHandle(uv_handle_t* handle);

    /**
     * callback for buffer's clean up
     */
    static void sFreeCallback(char* data, void* hint);

    /**
     * callback function to 'listening' event
     */
    static void sListening(const v8::FunctionCallbackInfo<v8::Value>& info);

    /**
     * call ws.write() to write data to TTS service
     */
    void Write();

    /**
     * call JS function to create TTS session
     */
    void CreateSession();

    /**
     * call conn.close()
     */
    void CloseSession();

    v8::Local<v8::Function> GetListeningFunction(v8::Isolate* isolate);

    char* mUserName;
    char* mPasswd;
    char mSilent[ONE_SEC_FRAMES*2]; //1 second
    v8::Persistent<v8::Object> mObj;
    v8::Persistent<v8::Function> mFunc;
    v8::Persistent<v8::Function> mCB;
    v8::Persistent<v8::Object> mConn;
    v8::Persistent<v8::Function> mConnCB;
    Engine mEngine;
    uv_async_t* mWriteAsync;
    std::queue<WriteData*> mWriteQueue;
    uv_mutex_t mQueueMutex;
    FLACEncoder* mEncoder;
    FLAC__int32 mBuff[ONE_SEC_FRAMES];
};

#endif /* SRC_STREAMTTS_HPP_ */
