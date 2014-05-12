/*
 * Copyright (C) 2014 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "wavwriter.h"

const uint32_t WavWriter::WAV_HEADER_SIZE = 44;

WavWriter::~WavWriter(){
    assert(mFile != NULL);
};

WavWriter::WavWriter(){
    mSampleRate = 0;
    mChannels = 0;
    mBitsPerSample = 0;
    mFile = NULL;
    mSize = WAV_HEADER_SIZE;
}

bool WavWriter::init(const char *name, uint32_t sampleRate,
                     uint16_t channels, uint16_t bitsPerSample){
    mFile = fopen(name, "wb");
    if(mFile == NULL)
        return false;
    mSampleRate = sampleRate;
    mChannels = channels;
    mBitsPerSample = bitsPerSample;
    writeHeader();
    return true;
}

bool WavWriter::close(){
    writeHeader();
    assert(mFile != NULL);
    fclose(mFile);
    return true;
}

uint32_t WavWriter::write(void *buffer, uint32_t size){
    mSize += size;
    return fwrite(buffer, size, 1, mFile);
}

uint32_t WavWriter::writeByte(FILE *fp, uint8_t byte){
    return fwrite(&byte, sizeof(byte), 1, fp);
}

uint32_t WavWriter::writeInt32(FILE *fp, uint32_t value){
    return fwrite(&value, sizeof(value), 1, fp);
}

uint32_t WavWriter::writeInt16(FILE *fp, uint16_t value){
    return fwrite(&value, sizeof(value), 1, fp);
}

uint32_t WavWriter::writeFourCC(FILE *fp, const char *fourCC){
    uint32_t retVal = 0;
    for(int i = 0; i < 4; ++i){
        retVal += writeByte(fp, fourCC[i]);
    }
    return retVal;
}
bool WavWriter::writeHeader(){
    // Seek to the beginning
    fseek(mFile, 0, SEEK_SET);

    // Write the header
    uint32_t retVal = 0;
    retVal += writeFourCC(mFile, "RIFF");       // ChunkID
    retVal += writeInt32(mFile, mSize - 8);     // ChunkSize
    retVal += writeFourCC(mFile, "WAVE");       // Format
    retVal += writeFourCC(mFile, "fmt ");       // Subchunk1ID
    retVal += writeInt32(mFile, 16);            // Subchunk1Size
    retVal += writeInt16(mFile, 1);             // AudioFormat
    retVal += writeInt16(mFile, mChannels);
    retVal += writeInt32(mFile, mSampleRate);

    uint32_t byteRate = mSampleRate * mChannels * mBitsPerSample/8;
    retVal += writeInt32(mFile, byteRate);
    retVal += writeInt16(mFile, mChannels * mBitsPerSample/8);  // BlockAlign
    retVal += writeInt16(mFile, mBitsPerSample);
    retVal += writeFourCC(mFile, "data");       // Subchunk2ID
    retVal += writeInt32(mFile, mSize - WAV_HEADER_SIZE);   // Subchunk2Size
    return (retVal != WAV_HEADER_SIZE);
}

