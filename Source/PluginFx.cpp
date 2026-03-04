/**
 *
 * Copyright (c) 2013-2014 Pascal Gauthier.
 * Copyright (c) 2013-2014 Filatov Vadim.
 * 
 * Filter taken from the Obxd project :
 *   https://github.com/2DaT/Obxd
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

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstring>
#include "PluginFx.h"
#include "PluginProcessor.h"

const float dc = 1e-18;

inline static float tptpc(float& state,float inp,float cutoff) {
	double v = (inp - state) * cutoff / (1 + cutoff);
	double res = v + state;
	state = res + v;
	return res;
}

inline static float tptlpupw(float & state , float inp , float cutoff , float srInv) {
	cutoff = (cutoff * srInv)*juce::MathConstants<float>::pi;
	double v = (inp - state) * cutoff / (1 + cutoff);
	double res = v + state;
	state = res + v;
	return res;
}

static float linsc(float param,const float min,const float max) {
    return (param) * (max - min) + min;
}

static float logsc(float param, const float min,const float max,const float rolloff = 19.0f) {
	return ((expf(param * logf(rolloff+1)) - 1.0f) / (rolloff)) * (max-min) + min;
}

PluginFx::PluginFx() {
    uiCutoff = 1;
    uiReso = 0;
    uiGain = 1;
    uiChorusMode = 0;
}

void PluginFx::init(int sr) {
    mm=0;
    s1=s2=s3=s4=c=d=0;
    R24=0;
    
    mmch = (int)(mm * 3);
    mmt = mm*3-mmch;

    sampleRate = sr;
    sampleRateInv = 1/sampleRate;
    float rcrate =sqrt((44000/sampleRate));
    rcor24 = (970.0 / 44000)*rcrate;
    rcor24Inv = 1 / rcor24;
    
    bright = tan((sampleRate*0.5f-10) * juce::MathConstants<float>::pi * sampleRateInv);
    
    R = 1;
	rcor = (480.0 / 44000)*rcrate;
    rcorInv = 1 / rcor;
    bandPassSw = false;

    pCutoff = -1;
    pReso = -1;
    
    dc_r = 1.0-(126.0/sr);
    dc_id = 0;
    dc_od = 0;

    // Juno chorus state
    chorusWritePos = 0;
    chorusLfoPhase = 0.0f;
    memset(chorusBufL, 0, sizeof(chorusBufL));
    memset(chorusBufR, 0, sizeof(chorusBufR));
}

inline float PluginFx::NR24(float sample,float g,float lpc) {
    float ml = 1 / (1+g);
    float S = (lpc*(lpc*(lpc*s1 + s2) + s3) +s4)*ml;
    float G = lpc*lpc*lpc*lpc;
    float y = (sample - R24 * S) / (1 + R24*G);
    return y + 1e-8;
};

inline float PluginFx::NR(float sample, float g) {
    float y = ((sample- R * s1*2 - g*s1  - s2)/(1+ g*(2*R + g))) + dc;
    return y;
}

void PluginFx::process(float *left, float *right, int sampleSize) {
    // very basic DC filter
    float t_fd = left[0];
    left[0] = left[0] - dc_id + dc_r * dc_od;
    dc_id = t_fd;
    for (int i=1; i<sampleSize; i++) {
        t_fd = left[i];
        left[i] = left[i] - dc_id + dc_r * left[i-1];
        dc_id = t_fd;
        
    }
    dc_od = left[sampleSize-1];
    
    if ( uiGain != 1 ) {
        for(int i=0; i < sampleSize; i++ )
            left[i] *= uiGain;
    }
    
    // don't apply the LPF if the cutoff is to maximum
    if ( uiCutoff != 1 ) {
        if ( uiCutoff != pCutoff || uiReso != pReso ) {
            rReso = (0.991-logsc(1-uiReso,0,0.991));
            R24 = 3.5 * rReso;
            
            float cutoffNorm = logsc(uiCutoff,60,19000);
            rCutoff = (float)tan(cutoffNorm * sampleRateInv * juce::MathConstants<float>::pi);
                
            pCutoff = uiCutoff;
            pReso = uiReso;
            
            R = 1 - rReso;
        }
            
        // Moog-style 4-pole ladder filter (OBXd-based)
        float g = rCutoff;
        float lpc = g / (1 + g);
        
        for(int i=0; i < sampleSize; i++ ) {
            float s = left[i];
            s = s - 0.45*tptlpupw(c,s,15,sampleRateInv);
            s = tptpc(d,s,bright);
            
            float y0 = NR24(s,g,lpc);
            
            //first low pass in cascade
            double v = (y0 - s1) * lpc;
            double res = v + s1;
            s1 = res + v;
            
            //damping
            s1 =atan(s1*rcor24)*rcor24Inv;
            float y1= res;
            float y2 = tptpc(s2,y1,g);
            float y3 = tptpc(s3,y2,g);
            float y4 = tptpc(s4,y3,g);
            float mc;
        
            switch(mmch) {
                case 0:
                    mc = ((1 - mmt) * y4 + (mmt) * y3);
                    break;
                case 1:
                    mc = ((1 - mmt) * y3 + (mmt) * y2);
                    break;
                case 2:
                    mc = ((1 - mmt) * y2 + (mmt) * y1);
                    break;
                case 3:
                    mc = y1;
                    break;
                default:
                    mc = y4;
                    break;
            }
            
            //half volume comp
            left[i] = mc * (1 + R24 * 0.45);
        }
    }

    // Juno-style BBD chorus
    int chorusMode = (uiChorusMode <= 0.25f) ? 0 : (uiChorusMode <= 0.75f) ? 1 : 2;
    if (chorusMode == 0) {
        // No chorus: copy mono to right (skip if same buffer)
        if (right != left)
            memcpy(right, left, (size_t)sampleSize * sizeof(float));
    } else {
        // Roland Juno chorus: modulated BBD delay
        // Mode I: 0.513 Hz LFO, Mode II: 0.863 Hz LFO
        const float lfoRates[2] = { 0.513f, 0.863f };
        float lfoHz  = lfoRates[chorusMode - 1];
        float lfoInc = lfoHz / sampleRate;

        // Center delay ~7.7ms, depth ~3.7ms (matches Juno hardware)
        float centerDelay = 0.0077f * sampleRate;
        float depth       = 0.0037f * sampleRate;

        // Helper: linearly interpolated read from circular buffer.
        // CHORUS_DELAY_LEN must be a power of 2 for the mask to work.
        auto readInterp = [](const float* buf, int writePos, float delaySamples) -> float {
            // rp may be negative; the bitwise mask handles wrapping for power-of-2 sizes
            int   ri = (int)floorf((float)writePos - delaySamples);
            float fr = ((float)writePos - delaySamples) - (float)ri;
            int   i0 = ri & (CHORUS_DELAY_LEN - 1);
            int   i1 = (i0 + 1) & (CHORUS_DELAY_LEN - 1);
            return buf[i0] * (1.0f - fr) + buf[i1] * fr;
        };

        for (int i = 0; i < sampleSize; i++) {
            float input = left[i];

            // Write current sample into both delay lines
            chorusBufL[chorusWritePos] = input;
            chorusBufR[chorusWritePos] = input;

            // Triangle LFO: maps phase [0,1] to [-1,+1] triangle
            float triL = (chorusLfoPhase < 0.5f)
                         ? (4.0f * chorusLfoPhase - 1.0f)
                         : (3.0f - 4.0f * chorusLfoPhase);
            // R channel is 180° out of phase for stereo width
            float phaseR = chorusLfoPhase + 0.5f;
            if (phaseR >= 1.0f) phaseR -= 1.0f;
            float triR = (phaseR < 0.5f)
                         ? (4.0f * phaseR - 1.0f)
                         : (3.0f - 4.0f * phaseR);

            float delayL = centerDelay + depth * triL;
            float delayR = centerDelay + depth * triR;

            float wetL = readInterp(chorusBufL, chorusWritePos, delayL);
            float wetR = readInterp(chorusBufR, chorusWritePos, delayR);

            // 50/50 dry/wet mix
            left[i]  = 0.5f * input + 0.5f * wetL;
            right[i] = 0.5f * input + 0.5f * wetR;

            chorusWritePos = (chorusWritePos + 1) & (CHORUS_DELAY_LEN - 1);
            chorusLfoPhase += lfoInc;
            if (chorusLfoPhase >= 1.0f) chorusLfoPhase -= 1.0f;
        }
    }
}

/*
 
 // THIS IS THE 2POLE FILTER
 
 for(int i=0; i < sampleSize; i++ ) {
 float s = work[i];
 s = s - 0.45*tptlpupw(c,s,15,sampleRateInv);
 s = tptpc(d,s,bright);
 
 //float v = ((sample- R * s1*2 - g2*s1 - s2)/(1+ R*g1*2 + g1*g2));
 float v = NR(s,g);
 float y1 = v*g + s1;
 //damping
 s1 = atan(s1 * rcor) * rcorInv;
 
 float y2 = y1*g + s2;
 s2 = y2 + y1*g;
 
 double mc;
 if(!bandPassSw)
 mc = (1-mm)*y2 + (mm)*v;
 else
 {
 
 mc =2 * ( mm < 0.5 ?
 ((0.5 - mm) * y2 + (mm) * y1):
 ((1-mm) * y1 + (mm-0.5) * v)
 );
 }
 
 work[i] = mc;
 }
 
*/
