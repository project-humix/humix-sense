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

#ifndef SRC_WAVUTIL_HPP_
#define SRC_WAVUTIL_HPP_

#include <fstream>

#include <sndfile.h>
#include <alsa/asoundlib.h>

static char RIF_MARKER[5] = "RIFF";
static char WAVE[5] = "WAVE";
static char FMT[5] = "fmt ";
/**
 *
 * Use this class to write wav file
 * fixed format: S16, because sphinx uses it
 */
class WavWriter {
public:
    WavWriter(const char* filename, uint16_t channel, uint32_t sample) {
        mRIFFMarker = RIF_MARKER;
        mFiletypeHeader = WAVE;
        mFormatMarker = FMT;
        mDataHeaderLength = 16;
        mFileSize = 36;
        mFormatType = 1;
        mNumberOfChannels = channel;
        mSampleRate = sample;
        mBytesPerSecond = sample * channel * 16 / 8;//(Sample Rate * Bit Size * Channels) / 8
        mBytesPerFrame = channel * 16 / 8;
        mBitsPerSample = 16;
        mFilename = strdup(filename);
        mOut.open(mFilename, std::ofstream::out | std::ofstream::trunc);
    }

    /**
     * close the wav file and update the file size and data size in
     * the headers
     */
    ~WavWriter() {
        if (mFileSize > 36) {
            //modify the fie size field
            mOut.seekp(4);
            mOut.write((char*) &mFileSize, 4);

            mOut.seekp(40);
            uint32_t data_size = mFileSize - 36;
            mOut.write((char*) &data_size, 4);
        }
        mOut.close();
        free(mFilename);
    }

    /**
     * write headers. for the file size and data length parts,
     * just put 44 and 0 at this moment.
     */
    void WriteHeader();
    /**
     * write data to the data part
     */
    void WriteData(const char *data, size_t size);

private:
    char* mFilename;
    char* mRIFFMarker;
    uint32_t mFileSize;
    char* mFiletypeHeader;
    char* mFormatMarker;
    uint32_t mDataHeaderLength;
    uint16_t mFormatType;
    uint16_t mNumberOfChannels;
    uint32_t mSampleRate;
    uint32_t mBytesPerSecond;
    uint16_t mBytesPerFrame;
    uint16_t mBitsPerSample;
    std::ofstream mOut;
};

/**
 * Play the wav file. each class could only
 * call play once and then delete.
 */
class WavPlayer {
public:

    /**
     * create a wav player for the specified file
     */
    WavPlayer(const char *filename);

    /**
     * delete the wav player
     */
    ~WavPlayer();

    /**
     * play the wav file and can only be called once
     * per instance. other then first call would be
     * no-op.
     */
    void Play();
private:
    snd_pcm_t *mHandle;
    SNDFILE *mFile;
    char* mBuff;
    size_t mSize;
    bool mError;
    snd_pcm_uframes_t mFrames;
};

#endif /* SRC_WAVUTIL_HPP_ */
