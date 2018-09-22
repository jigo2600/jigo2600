// TIASound.hpp
// Atari 2600 TIA emulator.

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef Atari2600Sound_hpp
#define Atari2600Sound_hpp

#include <cstdint>
#include <array>

namespace sim {

  class TIASound {
  public:
    TIASound() ;
    void cycle(long long colorCycle) ;
    void setAUDC(std::uint8_t x) ;
    void setAUDF(std::uint8_t x) ;
    void setAUDV(std::uint8_t x) ;
    void setState(std::uint16_t x) ;
    void reset() ;

    // Audio buffer.
    static constexpr int bufferSize = (1 << 16) ;
    static constexpr int bufferMask = bufferSize - 1 ;
    inline std::uint8_t const* getBufferSamples() const ;
    inline int const* getBufferSampleCycles() const ;
    inline long long getBufferEnd() const ;

    // Audio buffer resampler.
    void resample(uint8_t *begin, uint8_t *end, bool mix, double nominalRate = 3.579545e6) const ;

  protected:
    std::uint8_t AUDC ;
    std::uint8_t AUDF ;
    std::uint8_t AUDV ;
    std::uint8_t poly5 ;
    std::uint8_t poly4 ;
    int counter ;
    long long bufferEnd ;
    std::array<std::uint8_t, bufferSize> samples ;
    std::array<int, bufferSize> sampleCycles ;

    // Resampler.
    static constexpr size_t smootherOrder = 2 ;
    mutable std::array< std::array<double,2>, smootherOrder> smoother ;
    mutable double lastCycleEmitted ;
    mutable long long emitPosition ;

  private:
    void TP2Cycle() ;
  } ;

  std::uint8_t const* TIASound::getBufferSamples() const {
    return &samples[0] ;
  }

  int const* TIASound::getBufferSampleCycles() const {
    return &sampleCycles[0] ;
  }

  long long TIASound::getBufferEnd() const {
    return bufferEnd ;
  }
}

#endif /* Atari2600Sound_hpp */
