/**
 *
 * Copyright (c) 2013 Pascal Gauthier.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 */

#ifndef PLUGINFX_H_INCLUDED
#define PLUGINFX_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

// ---- Simple Direct Form I biquad filter ----
struct BiquadFilter {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;

    BiquadFilter() : b0(1.f), b1(0.f), b2(0.f), a1(0.f), a2(0.f),
                     x1(0.f), x2(0.f), y1(0.f), y2(0.f) {}

    void clear() { x1 = x2 = y1 = y2 = 0.f; }

    inline float process(float x) {
        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

class PluginFx {
	float s1,s2,s3,s4;
	float sampleRate;
	float sampleRateInv;
    float d, c;
	float R24;
	float rcor24,rcor24Inv;
    float bright;
    
	// 24 db multimode
    float mm;
	float mmt;
	int mmch;
    inline float NR24(float sample,float g,float lpc);

    // preprocess values taken the UI
    float rCutoff;
    float rReso;
    float rGain;
    
    // thread values; if these are different from the UI,
    // it needs to be recalculated.
    float pReso;
    float pCutoff;
    float pGain;
    
    // I am still keeping the 2pole w/multimode filter
    inline float NR(float sample, float g);
	bool bandPassSw;
	float rcor,rcorInv;
    int R;
    
    float dc_id;
    float dc_od;
    float dc_r;

    // SVF (State-Variable Filter) state
    float svfBP, svfLP;
    float svfBPR, svfLPR;
    float pSvfCutoff, pSvfReso;
    float svfF, svfQ;
    void  updateSvfCoeffs();

    // Juno-style BBD chorus
    static const int CHORUS_DELAY_LEN = 8192;
    float chorusBufL[CHORUS_DELAY_LEN];
    float chorusBufR[CHORUS_DELAY_LEN];
    int   chorusWritePos;
    float chorusLfoPhase;

    // 4-band EQ (low shelf / low-mid peak / high-mid peak / high shelf)
    // Two filters per band: L and R channels
    BiquadFilter eqL[4], eqR[4];
    float pEqGain[4];   // cached gain values to detect changes
    void  updateEqCoeffs();

    // Reverse ping-pong buffer
    static const int REVERSE_SEG = 4096;
    float revBufL[2][REVERSE_SEG];
    float revBufR[2][REVERSE_SEG];
    int   revWriteBuf;   // which half-buffer is being written (0 or 1)
    int   revWritePos;   // write position within current half

public:
    PluginFx();

    // this is set directly by the ui / parameter
    float uiCutoff;
    float uiReso;
    float uiGain;
    // 0 = off, 0.5 = mode I, 1.0 = mode II
    float uiChorusMode;

    // Drift: 0 = none, 1 = max (~20 cents). Per-voice processing happens in
    // PluginProcessor; uiDrift is stored here only for the parameter/CtrlFloat.
    float uiDrift;

    // Saturation drive: 0 = bypass, 1 = full drive
    float uiSaturation;

    // Reverse: 0 = off, 1 = on (ping-pong reverse buffer)
    float uiReverse;

    // 4-band EQ: each value 0..1, where 0.5 = 0 dB, 0 = -12 dB, 1 = +12 dB
    float uiEqLowGain;
    float uiEqLowMidGain;
    float uiEqHighMidGain;
    float uiEqHighGain;

    // State-Variable Filter (SVF)
    float uiSvfCutoff;
    float uiSvfReso;
    float uiSvfType;

    void init(int sampleRate);
    // Stereo-aware process: left[] is filtered in-place, right[] gets chorus-widened output
    void process(float *left, float *right, int sampleSize);
};

#endif  // PLUGINFX_H_INCLUDED
