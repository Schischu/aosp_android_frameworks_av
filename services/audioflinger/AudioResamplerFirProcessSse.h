/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIO_RESAMPLER_FIR_PROCESS_SSE_H
#define ANDROID_AUDIO_RESAMPLER_FIR_PROCESS_SSE_H

namespace android {

// depends on AudioResamplerFirOps.h, AudioResamplerFirProcess.h

//
// SSE specializations are enabled for Process() and ProcessL()
//
#ifdef __SSSE3__

#include <xmmintrin.h>     //SSE
#include <emmintrin.h>     //SSE2
#include <pmmintrin.h>     //SSE3
#include <tmmintrin.h>     //SSSE3

#ifdef __SSE4_1__
#include <smmintrin.h>     //SSE4.1
#define MM_EXTRACT_EPI32 _mm_extract_epi32
#define MM_EXTRACT_PS _mm_extract_ps
#define MM_MUL_EPI32 _mm_mul_epi32

#else
static inline
int MM_EXTRACT_EPI32(__m128i a, const int imm8)
{
    __attribute__((aligned(16))) int32_t tmp[4];
    _mm_store_si128((__m128i*)tmp, a);
    return tmp[imm8];
}

static inline
int MM_EXTRACT_PS(__m128 a, const int imm8)
{
    return MM_EXTRACT_EPI32(_mm_castps_si128(a), imm8);
}

static inline
__m128i MM_MUL_EPI32(__m128i a, __m128i b)
{
    __m128i sign, zero, mul_us, a_neg, b_neg, mul_us_neg;

    sign = _mm_xor_si128(a, b);
    sign =  _mm_srai_epi32(sign, 31);
    sign = _mm_shuffle_epi32(sign, _MM_SHUFFLE(2, 2, 0, 0));
    zero = _mm_setzero_si128();
    a_neg = _mm_abs_epi32(a);
    b_neg = _mm_abs_epi32(b);
    mul_us = _mm_mul_epu32 (a_neg, b_neg);
    mul_us_neg = _mm_sub_epi64(zero, mul_us);
    mul_us_neg = _mm_and_si128(sign, mul_us_neg);
    mul_us = _mm_andnot_si128(sign, mul_us);

    return _mm_or_si128(mul_us, mul_us_neg);
}
#endif

static inline
__m128i rnd_dbl_sta_64_32(__m128i a)
{
    __m128i mask = _mm_set_epi64x(0x40000000, 0x40000000);

    a = _mm_add_epi64(a, mask);
    a = _mm_slli_epi64(a, 1);
    mask = _mm_set1_epi32(0x80000000);
    mask = _mm_cmpeq_epi32(a, mask);
    return _mm_xor_si128 (a,  mask);
}

static inline
void adjvol(__m128i accum, const int32_t* const volumeLR, int32_t* const out)
{
    __m128i vrl = _mm_set_epi32 (0, volumeLR[1], 0, volumeLR[0]);

    vrl = MM_MUL_EPI32(accum, vrl);
    vrl = rnd_dbl_sta_64_32(vrl);
    out[0] += MM_EXTRACT_EPI32(vrl, 1);
    out[1] += MM_EXTRACT_EPI32(vrl, 3);
}

static inline
void adjvolf(__m128 accum, const float* const volumeLR, float* const out)
{
    __m128 vrl = _mm_set_ps (volumeLR[1], 0, volumeLR[0], 0);
    float t;

    vrl = _mm_mul_ps(accum, vrl);

    *((int*)&(t)) = MM_EXTRACT_PS(vrl, 1);
    out[0] += t;
    *((int*)&(t)) = MM_EXTRACT_PS(vrl, 3);
    out[1] += t;
}

static inline
__m128i mul_add_16(__m128i acc, __m128i a, __m128i b)
{
    __m128i mul_hi = _mm_mulhi_epi16(a, b);
    __m128i mul_lo = _mm_mullo_epi16(a, b);
    __m128i t = _mm_unpacklo_epi16(mul_lo, mul_hi);

    acc = _mm_add_epi32(acc, t);
    t = _mm_unpackhi_epi16(mul_lo, mul_hi);
    acc = _mm_add_epi32(acc, t);
    return acc;
}

static inline
__m128i mul_add_32(__m128i acc, __m128i a, __m128i a1, __m128i b)
{
    __m128i t, mul_2_0, mul_3_1, t1;

    t = _mm_unpacklo_epi16(_mm_setzero_si128(), b);
    mul_2_0 = MM_MUL_EPI32(a, t);

    t = _mm_shuffle_epi32(t, _MM_SHUFFLE(2, 3, 0, 1));
    a = _mm_shuffle_epi32(a, _MM_SHUFFLE(2, 3, 0, 1));
    mul_3_1 = MM_MUL_EPI32(a, t);
    t = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(mul_2_0),_mm_castsi128_ps(mul_3_1), 0xDD));

    t1 = _mm_unpackhi_epi16(_mm_setzero_si128(), b);
    mul_2_0 = MM_MUL_EPI32(a1, t1);

    t1 = _mm_shuffle_epi32(t1, _MM_SHUFFLE(2, 3, 0, 1));
    a1 = _mm_shuffle_epi32(a1, _MM_SHUFFLE(2, 3, 0, 1));
    mul_3_1 = MM_MUL_EPI32(a1, t1);
    t1 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(mul_2_0),_mm_castsi128_ps(mul_3_1), 0xDD));

    t =_mm_add_epi32(t, t1);

    acc = _mm_add_epi32(acc, t);
    return acc;
}

static inline
__m128i interp16(__m128i coefs0, __m128i coefs1, __m128i lerp)
{
    __m128i mask = _mm_set1_epi32(0x8000);
    __m128i t = _mm_sub_epi16(coefs1, coefs0);

    t = _mm_mulhrs_epi16(t, lerp);
    mask = _mm_cmpeq_epi16(t, mask);
    t = _mm_xor_si128 (t,  mask);

    return _mm_add_epi16(t, coefs0);
}

static inline
__m128i interp32(__m128i coefs0, __m128i coefs1, __m128i lerp)
{
    __m128i mask = _mm_set_epi64x(0x40000000, 0x40000000);
    __m128i t = _mm_sub_epi32(coefs1, coefs0);
    __m128i t1 = _mm_shuffle_epi32(t, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i mul_20 = MM_MUL_EPI32(t, lerp);
    __m128i mul_31 = MM_MUL_EPI32(t1, lerp);

    mul_20 = _mm_add_epi64(mul_20, mask);
    mul_31 = _mm_add_epi64(mul_31, mask);
    mul_20 = _mm_slli_epi64 (mul_20, 1);
    mul_31 = _mm_slli_epi64 (mul_31, 1);
    t = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(mul_20), _mm_castsi128_ps(mul_31), 0xDD));
    t = _mm_shuffle_epi32(t, _MM_SHUFFLE(3, 1, 2, 0));

    mask = _mm_set1_epi32(0x80000000);
    mask = _mm_cmpeq_epi32(t, mask);
    t = _mm_xor_si128 (t,  mask);

    return _mm_add_epi32(t, coefs0);
}

static inline
__m128 interpf(__m128 coefs0, __m128 coefs1, __m128 lerp)
{
    __m128 t = _mm_sub_ps(coefs1, coefs0);
    t = _mm_mul_ps(t, lerp);

    return _mm_add_ps(t, coefs0);
}

template <>
inline void ProcessL<1, 16>(int32_t* const out,
        int count,
        const int16_t* coefsP,
        const int16_t* coefsN,
        const int16_t* sP,
        const int16_t* sN,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 1; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1);

    __m128i mask = _mm_set_epi8(1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14);
    __m128i accum = _mm_setzero_si128();

    for (; count > 0; count -= 8) {

        __m128i coefs =  _mm_shuffle_epi8(_mm_load_si128((__m128i*) coefsP), mask);

        __m128i s =  _mm_loadu_si128((__m128i*) sP);
        accum = mul_add_16(accum, s, coefs);

        coefs =  _mm_load_si128((__m128i*) coefsN);

        s =  _mm_loadu_si128((__m128i*) sN);
        accum = mul_add_16(accum, s, coefs);

        sP -= 8;
        sN += 8;
        coefsP += 8;
        coefsN += 8;
    }

    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_hadd_epi32(accum, accum);

    adjvol(accum, volumeLR, out);

}

template <>
inline void ProcessL<2, 16>(int32_t* const out,
        int count,
        const int16_t* coefsP,
        const int16_t* coefsN,
        const int16_t* sP,
        const int16_t* sN,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 2; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1);

    __m128i mask = _mm_set_epi8(1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14);
    __m128i accum = _mm_setzero_si128();

    for (; count > 0; count -= 8) {

        __m128i coefs =  _mm_shuffle_epi8(_mm_load_si128((__m128i*) coefsP), mask);
        __m128i coefs1 = _mm_unpacklo_epi16(coefs, coefs);
        __m128i coefs0 = _mm_unpackhi_epi16(coefs, coefs);

        __m128i s1 =  _mm_loadu_si128((__m128i*) sP);
        __m128i s0 =  _mm_loadu_si128((__m128i*) (sP + 8));
        accum = mul_add_16(accum, s0, coefs0);
        accum = mul_add_16(accum, s1, coefs1);

        coefs = _mm_load_si128((__m128i*) coefsN);
        coefs0 = _mm_unpacklo_epi16(coefs, coefs);
        coefs1 = _mm_unpackhi_epi16(coefs, coefs);

        s0 =  _mm_loadu_si128((__m128i*) sN);
        s1 =  _mm_loadu_si128((__m128i*) (sN + 8));
        accum = mul_add_16(accum, s0, coefs0);
        accum = mul_add_16(accum, s1, coefs1);

        sP -= 16;
        sN += 16;
        coefsP += 8;
        coefsN += 8;
    }

    // R1 L1 R0 L0 -> R1 R0 L1 L0
    accum = _mm_shuffle_epi32(accum, _MM_SHUFFLE(3, 1, 2, 0));
    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_unpacklo_epi32(accum, accum);

    adjvol(accum, volumeLR, out);
}

template <>
inline void Process<1, 16>(int32_t* const out,
        int count,
        const int16_t* coefsP,
        const int16_t* coefsN,
        const int16_t* coefsP1,
        const int16_t* coefsN1,
        const int16_t* sP,
        const int16_t* sN,
        uint32_t lerpP,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 1; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1);

    __m128i mask = _mm_set_epi8(1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14);
    __m128i accum = _mm_setzero_si128();
    __m128i lerp16 = _mm_set1_epi16((int16_t)lerpP);

    for (; count > 0; count -= 8) {

        __m128i coefs0 =  _mm_shuffle_epi8(_mm_load_si128((__m128i*) coefsP), mask);
        __m128i coefs1 =  _mm_shuffle_epi8(_mm_load_si128((__m128i*) coefsP1), mask);
        __m128i coefs = interp16(coefs0, coefs1, lerp16);

        __m128i s =  _mm_loadu_si128((__m128i*) sP);
        accum = mul_add_16(accum, s, coefs);

        coefs0 =  _mm_load_si128((__m128i*) coefsN1);
        coefs1 =  _mm_load_si128((__m128i*) coefsN);
        coefs = interp16(coefs0, coefs1, lerp16);

        s =  _mm_loadu_si128((__m128i*) sN);
        accum = mul_add_16(accum, s, coefs);

        sP -= 8;
        sN += 8;
        coefsP += 8;
        coefsN += 8;
        coefsP1 += 8;
        coefsN1 += 8;
    }

    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_hadd_epi32(accum, accum);

    adjvol(accum, volumeLR, out);
}

template <>
inline void Process<2, 16>(int32_t* const out,
        int count,
        const int16_t* coefsP,
        const int16_t* coefsN,
        const int16_t* coefsP1,
        const int16_t* coefsN1,
        const int16_t* sP,
        const int16_t* sN,
        uint32_t lerpP,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 2; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1);

    __m128i mask = _mm_set_epi8(1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14);
    __m128i accum = _mm_setzero_si128();
    __m128i lerp16 = _mm_set1_epi16((int16_t)lerpP);

    for (; count > 0; count -= 8) {

        __m128i coefs0 =  _mm_shuffle_epi8(_mm_load_si128((__m128i*) coefsP), mask);
        __m128i coefs1 =  _mm_shuffle_epi8(_mm_load_si128((__m128i*) coefsP1), mask);
        __m128i coefs = interp16(coefs0, coefs1, lerp16);
        coefs1 = _mm_unpacklo_epi16(coefs, coefs);
        coefs0 = _mm_unpackhi_epi16(coefs, coefs);

        __m128i s1 =  _mm_loadu_si128((__m128i*) sP);
        __m128i s0 =  _mm_loadu_si128((__m128i*) (sP + 8));
        accum = mul_add_16(accum, s0, coefs0);
        accum = mul_add_16(accum, s1, coefs1);

        coefs0 =  _mm_load_si128((__m128i*) coefsN1);
        coefs1 =  _mm_load_si128((__m128i*) coefsN);
        coefs = interp16(coefs0, coefs1, lerp16);
        coefs0 = _mm_unpacklo_epi16(coefs, coefs);
        coefs1 = _mm_unpackhi_epi16(coefs, coefs);

        s0 =  _mm_loadu_si128((__m128i*) sN);
        s1 =  _mm_loadu_si128((__m128i*) (sN + 8));
        accum = mul_add_16(accum, s0, coefs0);
        accum = mul_add_16(accum, s1, coefs1);

        sP -= 16;
        sN += 16;
        coefsP += 8;
        coefsN += 8;
        coefsP1 += 8;
        coefsN1 += 8;
    }

    // R1 L1 R0 L0 -> R1 R0 L1 L0
    accum = _mm_shuffle_epi32(accum, _MM_SHUFFLE(3, 1, 2, 0));
    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_unpacklo_epi32(accum, accum);

    adjvol(accum, volumeLR, out);
}

template <>
inline void ProcessL<1, 16>(int32_t* const out,
        int count,
        const int32_t* coefsP,
        const int32_t* coefsN,
        const int16_t* sP,
        const int16_t* sN,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 1; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1);

    __m128i accum = _mm_setzero_si128();

    for (; count > 0; count -= 8) {

        __m128i coefs1 = _mm_castps_si128(_mm_loadr_ps((float*)coefsP));
        __m128i coefs0 = _mm_castps_si128(_mm_loadr_ps((float*)(coefsP + 4)));

        __m128i s =  _mm_loadu_si128((__m128i*) sP);
        accum = mul_add_32(accum, coefs0, coefs1, s);

        coefs0 =  _mm_load_si128((__m128i*) coefsN);
        coefs1 =  _mm_load_si128((__m128i*) (coefsN + 4));

        s =  _mm_loadu_si128((__m128i*) sN);;
        accum = mul_add_32(accum, coefs0, coefs1, s);

        sP -= 8;
        sN += 8;
        coefsP += 8;
        coefsN += 8;
    }

    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_hadd_epi32(accum, accum);

    adjvol(accum, volumeLR, out);

}

template <>
inline void ProcessL<2, 16>(int32_t* const out,
        int count,
        const int32_t* coefsP,
        const int32_t* coefsN,
        const int16_t* sP,
        const int16_t* sN,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 2; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1) - 8;

    __m128i accum = _mm_setzero_si128();

    for (; count > 0; count -= 4) {

        __m128i coefs = _mm_castps_si128(_mm_loadr_ps((float*)coefsP));
        __m128i coefs0 = _mm_unpacklo_epi32(coefs, coefs);
        __m128i coefs1 = _mm_unpackhi_epi32(coefs, coefs);

        __m128i s =  _mm_loadu_si128((__m128i*) sP);
        accum = mul_add_32(accum, coefs0, coefs1, s);

        coefs =  _mm_load_si128((__m128i*) coefsN);
        coefs0 = _mm_unpacklo_epi32(coefs, coefs);
        coefs1 = _mm_unpackhi_epi32(coefs, coefs);

        s =  _mm_loadu_si128((__m128i*) sN);
        accum = mul_add_32(accum, coefs0, coefs1, s);

        sP -= 8;
        sN += 8;
        coefsP += 4;
        coefsN += 4;
    }

    // mul_32_16 output channel order: 3 1 2 0
    // accum = _mm_shuffle_epi32(accum, _MM_SHUFFLE(3, 1, 2, 0));
    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_unpacklo_epi32(accum, accum);

    adjvol(accum, volumeLR, out);
}

template <>
inline void Process<1, 16>(int32_t* const out,
        int count,
        const int32_t* coefsP,
        const int32_t* coefsN,
        const int32_t* coefsP1,
        const int32_t* coefsN1,
        const int16_t* sP,
        const int16_t* sN,
        uint32_t lerpP,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 1; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1);

    __m128i accum = _mm_setzero_si128();
    __m128i lerp32 = _mm_set1_epi32(lerpP);

    for (; count > 0; count -= 8) {

        __m128i coefs0 = _mm_castps_si128(_mm_loadr_ps((float*)coefsP));
        __m128i coefs1 = _mm_castps_si128(_mm_loadr_ps((float*)coefsP1));
        __m128i coefs = interp32(coefs0, coefs1, lerp32);

        coefs0 =  _mm_castps_si128(_mm_loadr_ps((float*)(coefsP + 4)));
        coefs1 =  _mm_castps_si128(_mm_loadr_ps((float*)(coefsP1 + 4)));
        coefs0 = interp32(coefs0, coefs1, lerp32);

        __m128i s =  _mm_loadu_si128((__m128i*) sP);
        accum = mul_add_32(accum, coefs0, coefs, s);

        coefs0 =  _mm_load_si128((__m128i*) coefsN1);
        coefs1 =  _mm_load_si128((__m128i*) coefsN);
        coefs = interp32(coefs0, coefs1, lerp32);

        coefs0 =  _mm_load_si128((__m128i*) (coefsN1 + 4));
        coefs1 =  _mm_load_si128((__m128i*) (coefsN + 4));
        coefs1 = interp32(coefs0, coefs1, lerp32);

        s =  _mm_loadu_si128((__m128i*) sN);
        accum = mul_add_32(accum, coefs, coefs1, s);

        sP -= 8;
        sN += 8;
        coefsP += 8;
        coefsN += 8;
        coefsP1 += 8;
        coefsN1 += 8;
    }

    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_hadd_epi32(accum, accum);

    adjvol(accum, volumeLR, out);
}

template <>
inline void Process<2, 16>(int32_t* const out,
        int count,
        const int32_t* coefsP,
        const int32_t* coefsN,
        const int32_t* coefsP1,
        const int32_t* coefsN1,
        const int16_t* sP,
        const int16_t* sN,
        uint32_t lerpP,
        const int32_t* const volumeLR)
{
    const int CHANNELS = 2; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>1)-1) - 8;

    __m128i accum = _mm_setzero_si128();
    __m128i lerp32 = _mm_set1_epi32(lerpP);

    for (; count > 0; count -= 4) {

        __m128i coefs0 = _mm_castps_si128(_mm_loadr_ps((float*)coefsP));
        __m128i coefs1 = _mm_castps_si128(_mm_loadr_ps((float*)coefsP1));
        __m128i coefs = interp32(coefs0, coefs1, lerp32);

        coefs0 = _mm_unpacklo_epi32(coefs, coefs);
        coefs1 = _mm_unpackhi_epi32(coefs, coefs);

        __m128i s =  _mm_loadu_si128((__m128i*) sP);
        accum = mul_add_32(accum, coefs0, coefs1, s);

        coefs0 =  _mm_load_si128((__m128i*) coefsN1);
        coefs1 =  _mm_load_si128((__m128i*) coefsN);
        coefs = interp32(coefs0, coefs1, lerp32);

        coefs0 = _mm_unpacklo_epi32(coefs, coefs);
        coefs1 = _mm_unpackhi_epi32(coefs, coefs);

        s =  _mm_loadu_si128((__m128i*) sN);
        accum = mul_add_32(accum, coefs0, coefs1, s);

        sP -= 8;
        sN += 8;
        coefsP += 4;
        coefsN += 4;
        coefsP1 += 4;
        coefsN1 += 4;
    }

    // mul_32_16 output channel order: 3 1 2 0
    // accum = _mm_shuffle_epi32(accum, _MM_SHUFFLE(3, 1, 2, 0));
    accum = _mm_hadd_epi32(accum, accum);
    accum = _mm_unpacklo_epi32(accum, accum);

    adjvol(accum, volumeLR, out);
}

template <>
inline void ProcessL<1, 16>(float* const out,
        int count,
        const float* coefsP,
        const float* coefsN,
        const float* sP,
        const float* sN,
        const float* const volumeLR)
{
    const int CHANNELS = 1; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>2)-1);

    __m128 accum = _mm_setzero_ps();

    for (; count > 0; count -= 4) {

        __m128 coefs =  _mm_loadr_ps(coefsP);

        __m128 s =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sP));
        s = _mm_mul_ps(s, coefs);
        accum = _mm_add_ps(accum, s);

        coefs =  _mm_load_ps(coefsN);

        s =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sN));
        s = _mm_mul_ps(s, coefs);
        accum = _mm_add_ps(accum, s);

        sP -= 4;
        sN += 4;
        coefsP += 4;
        coefsN += 4;
    }

    accum = _mm_hadd_ps(accum, accum);
    accum = _mm_hadd_ps(accum, accum);

    adjvolf(accum, volumeLR, out);
}

template <>
inline void ProcessL<2, 16>(float* const out,
        int count,
        const float* coefsP,
        const float* coefsN,
        const float* sP,
        const float* sN,
        const float* const volumeLR)
{
    const int CHANNELS = 2; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>2)-1);

    __m128 accum = _mm_setzero_ps();

    for (; count > 0; count -= 4) {

        __m128 coefs =  _mm_loadr_ps(coefsP);
        __m128 coefs1 = _mm_unpacklo_ps(coefs, coefs);
        __m128 coefs0 = _mm_unpackhi_ps(coefs, coefs);

        __m128 s1 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sP));
        __m128 s0 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) (sP + 4)));
        s0 = _mm_mul_ps(s0, coefs0);
        accum = _mm_add_ps(accum, s0);
        s1 = _mm_mul_ps(s1, coefs1);
        accum = _mm_add_ps(accum, s1);

        coefs =  _mm_load_ps(coefsN);
        coefs0 = _mm_unpacklo_ps(coefs, coefs);
        coefs1 = _mm_unpackhi_ps(coefs, coefs);

        s0 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sN));
        s1 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) (sN + 4)));
        s0 = _mm_mul_ps(s0, coefs0);
        accum = _mm_add_ps(accum, s0);
        s1 = _mm_mul_ps(s1, coefs1);
        accum = _mm_add_ps(accum, s1);

        sP -= 8;
        sN += 8;
        coefsP += 4;
        coefsN += 4;
    }

    // R1 L1 R0 L0 -> R1 R0 L1 L0
    accum = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(accum), _MM_SHUFFLE(3, 1, 2, 0)));
    accum = _mm_hadd_ps(accum, accum);
    accum = _mm_unpacklo_ps(accum, accum);

    adjvolf(accum, volumeLR, out);
}

template <>
inline void Process<1, 16>(float* const out,
        int count,
        const float* coefsP,
        const float* coefsN,
        const float* coefsP1,
        const float* coefsN1,
        const float* sP,
        const float* sN,
        float lerpP,
        const float* const volumeLR)
{
    const int CHANNELS = 1; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>2)-1);

    __m128 accum = _mm_setzero_ps();
    __m128 lerpps = _mm_set1_ps(lerpP);

    for (; count > 0; count -= 4) {

        __m128 coefs0 =  _mm_loadr_ps(coefsP);
        __m128 coefs1 =  _mm_loadr_ps(coefsP1);
        __m128 coefs = interpf(coefs0, coefs1, lerpps);

        __m128 s =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sP));
        s = _mm_mul_ps(s, coefs);
        accum = _mm_add_ps(accum, s);

        coefs0 =  _mm_load_ps(coefsN1);
        coefs1 =  _mm_load_ps(coefsN);
        coefs = interpf(coefs0, coefs1, lerpps);

        s =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sN));
        s = _mm_mul_ps(s, coefs);
        accum = _mm_add_ps(accum, s);

        sP -= 4;
        sN += 4;
        coefsP += 4;
        coefsN += 4;
        coefsP1 += 4;
        coefsN1 += 4;
    }

    accum = _mm_hadd_ps(accum, accum);
    accum = _mm_hadd_ps(accum, accum);

    adjvolf(accum, volumeLR, out);
}

template <>
inline void Process<2, 16>(float* const out,
        int count,
        const float* coefsP,
        const float* coefsN,
        const float* coefsP1,
        const float* coefsN1,
        const float* sP,
        const float* sN,
        float lerpP,
        const float* const volumeLR)
{
    const int CHANNELS = 2; // template specialization does not preserve params
    const int STRIDE = 16;
    sP -= CHANNELS*((STRIDE>>2)-1);

    __m128 accum = _mm_setzero_ps();
    __m128 lerpps = _mm_set1_ps(lerpP);

    for (; count > 0; count -= 4) {

        __m128 coefs0 =  _mm_loadr_ps(coefsP);
        __m128 coefs1 =  _mm_loadr_ps(coefsP1);
        __m128 coefs = interpf(coefs0, coefs1, lerpps);
        coefs1 = _mm_unpacklo_ps(coefs, coefs);
        coefs0 = _mm_unpackhi_ps(coefs, coefs);

        __m128 s1 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sP));
        __m128 s0 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) (sP + 4)));
        s0 = _mm_mul_ps(s0, coefs0);
        accum = _mm_add_ps(accum, s0);
        s1 = _mm_mul_ps(s1, coefs1);
        accum = _mm_add_ps(accum, s1);

        coefs0 =  _mm_load_ps(coefsN1);
        coefs1 =  _mm_load_ps(coefsN);
        coefs = interpf(coefs0, coefs1, lerpps);
        coefs0 = _mm_unpacklo_ps(coefs, coefs);
        coefs1 = _mm_unpackhi_ps(coefs, coefs);

        s0 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) sN));
        s1 =  _mm_castsi128_ps(_mm_loadu_si128((__m128i*) (sN + 4)));
        s0 = _mm_mul_ps(s0, coefs0);
        accum = _mm_add_ps(accum, s0);
        s1 = _mm_mul_ps(s1, coefs1);
        accum = _mm_add_ps(accum, s1);

        sP -= 8;
        sN += 8;
        coefsP += 4;
        coefsN += 4;
        coefsP1 += 4;
        coefsN1 += 4;
    }

    // R1 L1 R0 L0 -> R1 R0 L1 L0
    accum = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(accum), _MM_SHUFFLE(3, 1, 2, 0)));
    accum = _mm_hadd_ps(accum, accum);
    accum = _mm_unpacklo_ps(accum, accum);

    adjvolf(accum, volumeLR, out);
}

#endif //USE_SSE

}; // namespace android

#endif /*ANDROID_AUDIO_RESAMPLER_FIR_PROCESS_SSE_H*/
