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

#include "WavUtil.hpp"


void WavWriter::WriteHeader() {
    mOut.write(mRIFFMarker, 4);
    mOut.write((char*) &mFileSize, 4);
    mOut.write(mFiletypeHeader, 4);
    mOut.write(mFormatMarker, 4);
    mOut.write((char*) &mDataHeaderLength, 4);
    mOut.write((char*) &mFormatType, 2);
    mOut.write((char*) &mNumberOfChannels, 2);
    mOut.write((char*) &mSampleRate, 4);
    mOut.write((char*) &mBytesPerSecond, 4);
    mOut.write((char*) &mBytesPerFrame, 2);
    mOut.write((char*) &mBitsPerSample, 2);
    mOut.write("data", 4);

    uint32_t data_size = mFileSize - 36;
    mOut.write((char*) &data_size, 4);
}

void WavWriter::WriteData(const char* data, size_t size) {
    mOut.write(data, size);
    mFileSize += size;
}

WavPlayer::WavPlayer(const char *filename) {
    int pcm = 0;
    int dir = 0;
    snd_pcm_hw_params_t *params;
    mHandle = NULL;
    mBuff = NULL;
    mError = false;
    mSize = 0;
    mFile = NULL;

    SF_INFO sfinfo;
    mFile = sf_open(filename, SFM_READ, &sfinfo);
    if (!mFile) {
        printf("ERROR: %s\n", sf_strerror(NULL));
        mError = true;
        return;
    }

    /* Open the PCM device in playback mode */
    if ((pcm = snd_pcm_open(&mHandle, "default", SND_PCM_STREAM_PLAYBACK, 0))
            < 0) {
        printf("ERROR: %s\n", snd_strerror(pcm));
        mError = true;
        return;
    }
    /* Allocate the snd_pcm_hw_params_t structure on the stack. */
    snd_pcm_hw_params_alloca(&params);
    /* Init hwparams with full configuration space */
    if (snd_pcm_hw_params_any(mHandle, params) < 0) {
        printf("Can not configure this PCM device.\n");
        mError = true;
        return;
    }
    if (snd_pcm_hw_params_set_access(mHandle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        printf("Error setting access.\n");
        mError = true;
        return;
    }

    /* Set sample format: always S16_LE */
    if (snd_pcm_hw_params_set_format(mHandle, params, SND_PCM_FORMAT_S16_LE)
            < 0) {
        printf("Error setting format.\n");
        return;
    }

    /* Set sample rate */
    uint32_t exact_rate = sfinfo.samplerate;
    if (snd_pcm_hw_params_set_rate_near(mHandle, params, &exact_rate, &dir)
            < 0) {
        printf("Error setting rate.\n");
        mError = true;
        return;
    }
    /* Set number of channels */
    if (snd_pcm_hw_params_set_channels(mHandle, params, sfinfo.channels)
            < 0) {
        printf("Error setting channels.\n");
        mError = true;
        return;
    }
    /* Write the parameters to the driver */
    if ((pcm = snd_pcm_hw_params(mHandle, params)) < 0) {
        printf("unable to set hw parameters: %s\n", snd_strerror(pcm));
        mError = true;
        return;
    }

    /* Use a buffer large enough to hold one period */
    if (snd_pcm_hw_params_get_period_size(params, &mFrames, NULL) < 0) {
        printf("Error get buffer size.\n");
        mError = true;
        return;
    }
    snd_pcm_nonblock(mHandle, 0);
//        printf("frames:%lu\n", mFrames);

    mSize = mFrames * sfinfo.channels * 2; /* 2 -> sample size */
    ;
    mBuff = (char *) malloc(mSize);
}

WavPlayer::~WavPlayer() {
    if (!mError) {
        if (mHandle) {
            snd_pcm_drain(mHandle);
            snd_pcm_close(mHandle);
        }
        if (mBuff) {
            free(mBuff);
        }
    }
}

void
WavPlayer::Play() {
    if (mFile) {
        int pcmrc = 0;
        int readcount = 0;
        while ((readcount = sf_readf_short(mFile, (short*) mBuff, mFrames))
                > 0) {
            pcmrc = snd_pcm_writei(mHandle, mBuff, readcount);
            if (pcmrc == -EPIPE) {
                printf("Underrun!\n");
                snd_pcm_recover(mHandle, pcmrc, 1);
            } else if (pcmrc != readcount) {
                printf("wframe count mismatched: %d, %d\n", pcmrc,
                        readcount);
            }
        }
        sf_close(mFile);
        mFile = NULL;
    }
}
