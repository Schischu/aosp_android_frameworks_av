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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "pvmp3decoder_api.h"
#include "mp3reader.h"
#include "wavwriter.h"

using namespace std;

enum {
        kInputBufferSize = 10 * 1024,
        kOutputBufferSize = 4608 * 2,
    };

int main(int argc, const char **argv) {

    if(argc != 3) {
        printf("Usage %s <input file> <output file>\n", argv[0]);
        return -1;
    }

    // Initialize the config
    tPVMP3DecoderExternal config;
    config.equalizerType = flat;
    config.crcEnabled = false;

    // Allocate the decoder memory
    void *decoderBuf;
    uint32_t memRequirements = pvmp3_decoderMemRequirements();
    decoderBuf = malloc(memRequirements);
    assert(decoderBuf != NULL);

    // Initialize the decoder
    pvmp3_InitDecoder(&config, decoderBuf);

    // Open the input file
    Mp3Reader mp3Reader;
    bool success = mp3Reader.init(argv[1]);
    if(!success) return -1;

    // Open the output file
    WavWriter wavWriter;
    success = wavWriter.init(argv[2],
                   mp3Reader.getSampleRate(),
                   mp3Reader.getNumChannels(),
                   16);
    if(!success) return -1;

    //Allocate input buffer
    void *inputBuf = malloc(kInputBufferSize);
    assert(inputBuf != NULL);

    //Allocate output buffer
    void *outputBuf = malloc(kOutputBufferSize);
    assert(outputBuf != NULL);

    //Decode loop
    bool retVal = 0;
    while(1)
    {
        //Read input from the file
        uint32_t bytesRead;
        bool success = mp3Reader.getFrame(inputBuf, &bytesRead);
        if(!success) break;

        //Set the input config
        config.inputBufferCurrentLength = bytesRead;
        config.inputBufferMaxLength = 0;
        config.inputBufferUsedLength = 0;
        config.pInputBuffer  = reinterpret_cast<unsigned char*>(inputBuf);
        config.pOutputBuffer = reinterpret_cast<short*>(outputBuf);
        config.outputFrameSize = kOutputBufferSize / sizeof(int16_t);

        ERROR_CODE decoderErr;
        decoderErr = pvmp3_framedecoder(&config, decoderBuf);
        if(decoderErr != NO_DECODING_ERROR) {
            //Decoder encountered error
            retVal = -1;
            break;
        }
        wavWriter.write(outputBuf, config.outputFrameSize * sizeof(int16_t));
    }

    //Close input reader and output writer
    mp3Reader.close();
    wavWriter.close();

    //Free allocated memory
    free(inputBuf);
    free(outputBuf);
    free(decoderBuf);

    return retVal;
}
