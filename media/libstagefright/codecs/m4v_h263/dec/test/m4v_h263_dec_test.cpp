/*
 * Copyright (C) 2014 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
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
#include <stdint.h>
#include <assert.h>
#include "mp4dec_api.h"

//Constants
enum {
    kInputBufferSize  = 100 * 1024,
    kMaxWidth         = 704,
    kMaxHeight        = 352,
    kOutputBufferSize = kMaxWidth * kMaxHeight * 2,
    kVopStartCode     = 0xb6
};

bool readTillNextFrameMpeg4(FILE *fp, uint8_t buf[], int32_t *length, bool *isEofReached){
    uint8_t nextBytes[4];
    int bytesRead;
    int offset;

    *isEofReached = false;

    // Read the next start code
    bytesRead = fread(buf, 1, 4, fp);
    if(bytesRead != 4)
        return false;

    offset = 4;
    bytesRead = fread(nextBytes, 1, 3, fp);
    if(bytesRead != 3) {
        if(bytesRead == 2){
            buf[offset++] = nextBytes[0];
            buf[offset++] = nextBytes[1];
            *isEofReached = true;
            *length = offset;
            return true;
        } else {
            return false; // Frame has to be at least 6 bytes
        }
    }
    while(1) {
        bytesRead = fread(&nextBytes[3], 1, 1, fp);
        if(bytesRead != 1) { // Reached end of file
            buf[offset++] = nextBytes[0];
            buf[offset++] = nextBytes[1];
            buf[offset++] = nextBytes[2];
            *isEofReached = true;
            *length = offset;
            return true;
        }

        if(nextBytes[0] == 0x00 &&
           nextBytes[1] == 0x00 &&
           nextBytes[2] == 0x01 &&
           nextBytes[3] == kVopStartCode) {
            fseek(fp, -4, SEEK_CUR); // Seek back 4 bytes
            *length = offset;
            return true;
        }

        buf[offset++] = nextBytes[0];
        nextBytes[0]  = nextBytes[1];
        nextBytes[1]  = nextBytes[2];
        nextBytes[2]  = nextBytes[3];
    }
    return false;
}

bool readTillNextFrameH263(FILE *fp, uint8_t buf[], int32_t *length, bool *isEofReached){
    uint8_t nextBytes[4];
    int bytesRead;
    int offset;

    *isEofReached = false;

    // Read the next start code
    bytesRead = fread(buf, 1, 3, fp);
    if(bytesRead != 3)
        return false; // Frame has to be at least 3 bytes

    bytesRead = fread(nextBytes, 1, 2, fp);
    if(bytesRead != 2)
        return false; // Frame has to be at least 5 bytes

    offset = 3;
    while(1) {
        bytesRead = fread(&nextBytes[2], 1, 1, fp);
        if(bytesRead != 1) { // Reached end of file
            buf[offset++] = nextBytes[0];
            buf[offset++] = nextBytes[1];
            buf[offset++] = nextBytes[2];
            *isEofReached = true;
            *length = offset;
            return true;
        }

        if(nextBytes[0] == 0x00 &&
           nextBytes[1] == 0x00 &&
           ((nextBytes[2] & 0xFC) == 0x80)) {
            fseek(fp, -3, SEEK_CUR); // Seek back 3 bytes
            *length = offset;
            return true;
        }

        buf[offset++] = nextBytes[0];
        nextBytes[0]  = nextBytes[1];
        nextBytes[1]  = nextBytes[2];
    }
    return false;
}


int main(int argc, char* argv[] ){

    if(argc < 5){
        fprintf(stderr, "Usage %s <input m4v file> <output yuv> <width> <height>\n", argv[0]);
        fprintf(stderr, "Max Width %d Max Height %d\n", kMaxWidth, kMaxHeight);
        return 1;
    }

    // Read height and width
    int32_t width;
    int32_t height;
    width = atoi(argv[3]);
    height = atoi(argv[4]);
    if(width > kMaxWidth || height > kMaxHeight || width <= 0 || height <= 0){
        fprintf(stderr, "Unsupported dimensions %dx%d\n", width, height);
        return 1;
    }

    // Allocate input buffer
    uint8_t *inputBuf = (uint8_t *)malloc(kInputBufferSize);
    assert(inputBuf != NULL);

    // Allocate output buffer
    uint8_t *outputBuf = (uint8_t *)malloc(kOutputBufferSize);
    assert(outputBuf != NULL);

    // Open the input file
    FILE* fpInput = fopen(argv[1], "rb");
    if (!fpInput) {
        fprintf(stderr, "Could not open %s\n", argv[1]);
        return 1;
    }

    // Open the output file
    FILE* fpOutput = fopen(argv[2], "wb");
    if (!fpOutput) {
        fprintf(stderr, "Could not open %s\n", argv[2]);
        return 1;
    }

    // Initialise the handle
    tagvideoDecControls handle;
    memset(&handle, 0, sizeof(tagvideoDecControls));

    // Initialise the decoder
    uint8_t *vol_data[1];
    int32_t vol_size = 0;
    int32_t inputLength;

    vol_data[0] = NULL;

    bool isEofReached = false;
    bool success;
    bool (*readTillNextFrame)(FILE *fp, uint8_t buf[], int32_t *length, bool *isEofReached);
    MP4DecodingMode mode;

    // Determine whether file is H.263 or MPEG4
    {
        uint8_t buf[3];

        int bytesRead = fread(buf, 1, 3, fpInput);
        if(bytesRead != 3) {
            fprintf(stderr, "Error reading input file %s\n", argv[1]);
            fclose(fpInput);
            return 1;
        }
        fseek(fpInput, SEEK_SET, 0);

        if(buf[0] == 0x0 && buf[1] == 0x00 && (buf[2] & 0xFC) == 0x80)
            mode = H263_MODE;
        else
            mode = MPEG4_MODE;

    }

    // Read header in case of MPEG4
    if(mode == MPEG4_MODE) {
        readTillNextFrame = readTillNextFrameMpeg4;
        success = readTillNextFrame(fpInput, inputBuf, &inputLength, &isEofReached);
        if(!success) {
            fprintf(stderr, "Error reading input file %s\n", argv[1]);
            fclose(fpInput);
            return 1;
        }
        vol_data[0] = inputBuf;
        vol_size = inputLength;
    } else {
        readTillNextFrame = readTillNextFrameH263;
    }

    // Initialise decoder
    Bool pvSuccess = PVInitVideoDecoder(
        &handle, vol_data, &vol_size, 1, width, height, mode);

    if(pvSuccess != PV_TRUE){
        fprintf(stderr, "Error decoding header\n");
        fclose(fpInput);
        return 1;
    }

    // Decode frames
    int retVal = 0;
    int frameCount = 1;
    while(!isEofReached){
        success = readTillNextFrame(fpInput, inputBuf, &inputLength, &isEofReached);
        if(!success) {
            fprintf(stderr, "Error reading input file %s\n", argv[1]);
            retVal = 1;
            break;
        }

        uint32_t timestamp = 0xFFFFFFFF;
        uint32_t useExtTimestamp = 0;
        int32_t tmp = inputLength;
        if (PVDecodeVideoFrame(
                    &handle, &inputBuf, &timestamp, &tmp,
                    &useExtTimestamp,
                    outputBuf) != PV_TRUE) {
            fprintf(stderr, "Error decoding frame");
            break;
        }

        fwrite(outputBuf, 1, (width * height * 3)/2, fpOutput);
        printf("Decoding frame %d\r", frameCount++);

    }
    // Close input and output file
    fclose(fpInput);
    fclose(fpOutput);

    //Free allocated memory
    free(inputBuf);
    free(outputBuf);

    // Close decoder instance
    PVCleanUpVideoDecoder(&handle);
    return retVal;
}
