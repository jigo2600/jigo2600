// TIASound.hpp
// Atari 2600 TIA sound emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "TIASound.hpp"
#include <cstdint>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <iostream>

using namespace jigo ;
using namespace std ;

TIASound::TIASound()
{
  reset() ;
}

void TIASound::reset()
{
  AUDC = 0 ;
  AUDF = 0 ;
  AUDV = 0 ;
  poly5 = 0x1f ;
  poly4 = 0xf ;
  counter = 0 ;
  bufferEnd = 0 ;
  // Resampler.
  samples = {} ;
  sampleCycles = {} ;
  smoother = {} ;
  lastCycleEmitted = 0 ;
  emitPosition = 0 ;
}

void TIASound::setAUDC(std::uint8_t x) { AUDC = (x & 0x0f) ; }
void TIASound::setAUDF(std::uint8_t x) { AUDF = (x & 0x1f) ; }
void TIASound::setAUDV(std::uint8_t x) { AUDV = (x & 0x0f) ; }
void TIASound::setState(std::uint16_t x) {
  poly5 = 0x1f & x ;
  poly4 = 0x0f & (x >> 5) ;
}

void TIASound::cycle(long long colorCycle)
{
  // Add a sample to audio to buffer.
  long long p = bufferEnd & bufferMask ;
  sampleCycles[p] = colorCycle ;
  if (poly4 & 0x8) {
    samples[p] = AUDV ;
  } else {
    samples[p] = 0 ;
  }
  ++bufferEnd ;

  // Advance the AUDF counter.
  // The *previous* value of counter is compared to AUDC.
  // Note that this is a slight approximation, as the code is only correct if
  // AUDF register was stable for 4 CLK prior to this comparison. Modelling AUDF
  // changes exactly would require to explicitely model APhi1 and APhi2,
  // whereas only the latter is done here.

  if (counter == AUDF) {
    TP2Cycle() ;
    counter = 0 ;
  } else {
    counter = (counter + 1) & 0x1f ;
  }
}

// Helper.
void TIASound::TP2Cycle()
{
  if (AUDC == 0000) {
    // Constant signal update.
    poly5 = (poly5 << 1) | 1 ;
    poly4 = (poly4 << 1) | 1 ;
    return ;
  }

  // poly5 = C[0:4]
  if (AUDC & 0x3) {
    // poly5 update (xor bit 2 and 4)
    bool cond1 = poly5 & 0x04 ; // C[2]
    bool cond2 = poly5 & 0x10 ; // C[4]
    bool cond3 = (poly5 & 0x1f) == 0 ;
    poly5 = (poly5 << 1) ;
    if ((cond1 ^ cond2) | cond3) { poly5 |= 1 ; }
  } else {
    // poly9 update (xor bit 4 and 8)
    bool cond1 = poly5 & 0x10 ; // C[4]
    bool cond2 = poly4 & 0x08 ; // C[4+3]
    bool cond3 = (poly5 & 0x1f) == 0 && (poly4 & 0xf) == 0 ;
    poly5 = (poly5 << 1) ;
    if ((cond1 ^ cond2) | cond3) { poly5 |= 1 ; }
  }

  // Check if poly4 should be clocked. This depends on
  // the first two bits ouf AUDC and the value of C[0:4].
  // Note also that C[0:4] has already been updated (shifted to the left), so
  // we check out bits shifted by one to the left compared to the stated condition.
  if ((AUDC & 0x3) == 0x2) {
    // Clock poly4 if C[0:3]=0001, otherwise finish.
    if ((poly5 & 0x1e) != 0x10) { return ; }
  }
  else if ((AUDC & 0x3) == 0x3) {
    // Clock poly4 if C[4]=1, otherwise finish.
    if ((poly5 & 0x20) != 0x20) { return ; }
  }
  // Always clock poly4 fot other values of AUDC.

  // poly4 = C[5:7]
  switch (AUDC >> 2) {
    case 0: { // poly4 updates.
      bool cond1 = poly4 & 0x04 ; // C[7]
      bool cond2 = poly4 & 0x08 ; // C[8]
      bool cond3 = (poly4 & 0xf) == 0 ;
      poly4 <<= 1 ;
      if ((cond1 ^ cond2) | cond3) { poly4 |= 1 ; }
      break ;
    }
    case 1: { // 0,1 wave.
      if (poly4 & 0x01) { poly4 = (poly4 << 1) ; }
      else { poly4 = (poly4 << 1) | 1 ; }
      break ;
    }
    case 2: { // Chain poly5 -> poly4.
      poly4 <<= 1 ;
      if (poly5 & 0x20) { poly4 |= 1 ; }
      break ;
    }
    case 3: { // 0,0,0,1,1,1 wave.
      bool cond1 = (poly4 & 0x7) == 0x5 ; // C[4:6]=5
      bool cond2 = (poly4 & 0x4) == 0x0 ; // C[6]=0
      poly4 <<= 1 ;
      if (cond1 | cond2) { poly4 |= 1 ; }
      break ;
    }
    default: assert(false) ;
  }
}

void
TIASound::resample(uint8_t *begin, uint8_t *end, bool mix, double nominalRate) const
{
  assert(end >= begin) ;
  auto numSamples = end - begin ;

  static constexpr double sos[2][6] = {
    {0.001878908554386676, 0.0037578171087733507, 0.0018789085543866753, 1.0, -1.9114274753486549, 0.9151870157068362},
    {1.0, -1.913266482173494, 0.9151471579521344, 1.0, -1.9114274255653347, 0.9151870530425102},
  };

   // Get the latest simulated cycle.
  auto cycle = sampleCycles[(bufferEnd - 1) & bufferMask] ;

  // Smooth the latest simulated cycle to get a stable cycle rate.
  double smoothCycle = cycle ;
  for (size_t k = 0 ; k < smoother.size() ; ++k) {
    auto w  = sos[k][3] * smoothCycle - sos[k][4] * smoother[k][0] - sos[k][5] * smoother[k][1] ;
    smoothCycle = sos[k][0] * w + sos[k][1] * smoother[k][0] + sos[k][2] * smoother[k][1] ;
    smoother[k] = {w, smoother[k][0]} ;
  }

  if (smoothCycle > cycle + 2*3.5e6/60 || smoothCycle < cycle - 2*3.5e6/60) {
    double y = cycle ;
    double r = nominalRate ;
    for (size_t k = 0 ; k < smoother.size() ; ++k) {
      auto a = sos[k][3] / (1 + sos[k][4] + sos[k][5]) ;
      auto b = (sos[k][5] - 1) / (1 + sos[k][4] + sos[k][5]) ;
      auto p = sos[k][0] + sos[k][1] + sos[k][2] ;
      auto gamma = a * r ;

      smoother[k][0] = a * y + b * gamma ;
      smoother[k][1] = smoother[k][0] - gamma ;
      y = p * smoother[k][0] + (sos[k][0] - sos[k][2]) * gamma ;
      r *= p * a ;
    }
    smoothCycle = cycle ;
  }
  //std::cout.precision(17) ;
  //std::cout<< "[" << cycle << ", " << smoothCycle << "]," << std::endl ;

  // Reproduce sound with a little delay to avoid tripping when overshooting a little.
  smoothCycle = min((decltype(smoothCycle))cycle, smoothCycle - 2 * 3.5e6 / 60);

  // The last cycle emitted cannot be greater than the largest cycle
  // in the buffer. If this happens, it is because the simulation was reset.
  lastCycleEmitted = min(lastCycleEmitted, (decltype(lastCycleEmitted))cycle) ;

  // We would like to emit all available cycles up to smoothCycle. However,
  // we cannot backtrack and we can exceed the largest cycle in the buffer,
  // so we clamp the target cycle number.
  auto targetCycle = min((decltype(lastCycleEmitted))cycle, max(smoothCycle, lastCycleEmitted)) ;

  // We are going to emit cycles in the range [lastCycleEmitted, targetCycle).
  // These cycles are uniformly distributed among audio samples.
  auto nextCycleToEmit = lastCycleEmitted ;
  auto emitRate = (targetCycle - lastCycleEmitted) / (double)numSamples ;

  // Make sure the current position in the ring buffer is valid.
  emitPosition = min(emitPosition, bufferEnd) ;
  emitPosition = max(emitPosition, bufferEnd - bufferSize + 1) ;

  // Resampling.
  uint8_t sample = samples[emitPosition % bufferMask] ;
  while (begin != end) {
    while (true) {
      auto p = emitPosition + 1 ;
      if (p < bufferEnd && sampleCycles[p & bufferMask] <= nextCycleToEmit) {
        sample = samples[p & bufferMask] ;
        emitPosition = p ;
      } else {
        break ;
      }
    }
    // Store.
    int scaledSample = ((int)sample) << 3 ;
    scaledSample += 128 ;
    if (mix) {
      int mixed = *begin ;
      *begin++ = (scaledSample + mixed) >> 1 ;
    } else {
      *begin++ = scaledSample ;
    }
    nextCycleToEmit += emitRate ;
  }

  // Record which is the last emitted cycle for later.
  lastCycleEmitted = targetCycle ;
}
