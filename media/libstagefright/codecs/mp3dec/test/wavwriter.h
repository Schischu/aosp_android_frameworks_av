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

#ifndef WAV_WRITER_H_
#define WAV_WRITER_H_

class WavWriter {
public:
    WavWriter();
    bool init(const char *name, uint32_t sampleFrequency, uint16_t channels,
              uint16_t bitsPerSample);
    uint32_t write(void *buffer, uint32_t size);
    bool close();
    ~WavWriter();
private:
    FILE *mFile;
    uint32_t mSampleRate;
    uint16_t mChannels;
    uint16_t mBitsPerSample;
    uint32_t mSize;
    bool writeHeader();
    uint32_t writeByte(FILE *fp, uint8_t byte);
    uint32_t writeInt16(FILE *fp, uint16_t value);
    uint32_t writeInt32(FILE *fp, uint32_t value);
    uint32_t writeFourCC(FILE *fp, const char *fourCC);
    static const uint32_t WAV_HEADER_SIZE;
};

#endif  // WAV_WRITER_H_
