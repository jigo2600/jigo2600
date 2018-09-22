// TIA.hpp
// Atari2600 TIA emulator.

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef TIA_hpp
#define TIA_hpp

#define TIA_FAST 1
#if TIA_FAST
#define TIA_FORCE_INLINE __attribute__((always_inline))
#else
#define TIA_FORCE_INLINE
#endif

#include "TIAComponents.hpp"
#include "TIASound.hpp"
#include "json.hpp"

namespace sim {

  constexpr auto TIA_NTSC_COLOR_CLOCK_RATE = 3.579545e6  ;
  constexpr auto TIA_PAL_COLOR_CLOCK_RATE = 3.546894e6 ;
   
  // -----------------------------------------------------------------
  // MARK: - TIA state
  // -----------------------------------------------------------------

  struct TIAState
  {
    enum class VideoStandard : int {NTSC, PAL, SECAM} videoStandard = VideoStandard::NTSC ;

    enum Register {
      // Writable registers.
      VSYNC = 0, VBLANK, WSYNC, RSYNC,
      NUSIZ0, NUSIZ1,
      COLUP0, COLUP1, COLUPF, COLUBK, CTRLPF,
      REFP0, REFP1,
      PF0, PF1, PF2,
      RESP0, RESP1, RESM0, RESM1, RESBL,
      AUDC0, AUDC1, AUDF0, AUDF1, AUDV0, AUDV1,
      GRP0, GRP1,
      ENAM0, ENAM1, ENABL,
      HMP0, HMP1, HMM0, HMM1, HMBL,
      VDELP0, VDELP1, VDELBL,
      RESMP0, RESMP1, HMOVE, HMCLR, CXCLR,
      // Readable registers.
      CXM0P = 0x30, CXM1P, CXP0FB, CXP1FB,
      CXM0FB, CXM1FB, CXBLPF, CXPPMM,
      INPT0 = 0x38, INPT1, INPT2,
      INPT3, INPT4, INPT5,
      NA1, // 0x3e
      NA2, // 0x3f
      VOID // Placeholder to mean "none".
    } ;

    // Lifecycle.
    virtual ~TIAState() = default ;
    bool operator== (TIAState const& s) const ;

    inline static Register decodeAddress(bool Rw, std::uint16_t address) {
      if (Rw) {
        // Note that registers e and f do not exist. Due to the circuitry,
        // in this case the TIA should read a zero in the last two bits.
        return static_cast<Register>((address & 0xf) | 0x30) ;
      } else {
        return static_cast<Register>(address & 0x3f) ;
      }
    }

    // Counters.
    std::int64_t numCycles ;
    std::int64_t numFrames ;

    // Data bus.
    Register strobe ;
    std::uint8_t D ;
    bool RDY ;

    // Display.
    int beamX ;
    int beamY ;
    TIADualPhaseAndCounter<56> Hphasec ;
    TIADelayedLatch HBnot ;
    TIASEC SEC ;
    bool SECL ;
    bool VB ; // VBLANK latch.
    bool VS ; // VSYNC latch.

    // Extra motion clocks.
    int HMC ;
    TIAExtraClock BEC ;
    std::array<TIAExtraClock,2> MEC ;
    std::array<TIAExtraClock,2> PEC ;

    // Visual objects.
    TIAPlayField PF ;
#if TIA_FAST
    TIABall B ;
    std::array<TIAMissile,2> M ;
    std::array<TIAPlayer,2> P ;
#else
    TIABallAlt B ;
    std::array<TIAMissileAlt,2> M ;
    std::array<TIAPlayerAlt,2> P ;
#endif
    int collisions ;

    // IO ports.
    TIAPorts ports ;
  } ;

  // -----------------------------------------------------------------
  // MARK: - TIA
  // -----------------------------------------------------------------

  class TIA : public TIAState {
  public:
    // Lifecycle.
    TIA& operator= (TIAState const& s) {TIAState::operator=(s);return *this;}
    TIA& operator= (TIA const&) = delete ;

    // Operation.
    void cycle(bool CS, bool Rw, std::uint16_t address, std::uint8_t& data) ;
    void reset() ;
    uint32_t getColor(uint8_t value) const ;
    void setVerbose(bool x) {verbose = x;}

    static int constexpr screenHeight = 228 + 8 + 10 ; //192 + 8 + 10 ;
    static int constexpr screenWidth = 160 ;
    static float constexpr pixelAspectRatio = 1.8f ;

    VideoStandard getVideoStandard() const { return videoStandard ; }
    void setVideoStandard(VideoStandard x) { videoStandard = x ; }

    //private:

    // Transient.
    TIASound sound[2] ;
    std::uint32_t colors [4] ;
    unsigned int collisions ;
    static int constexpr numScreenBuffers = 3 ;
    int currentScreen ;
    std::uint32_t screen [numScreenBuffers] [screenWidth * screenHeight] ;
    int verbose ;
    bool startNewFrame ;

  protected:
    static struct Tables {
      Tables() ;
      unsigned int collisionAndColorTable [2 * 3 * 64] ;
    } tables ;
  } ;

  void to_json(nlohmann::json& j, TIAState const& state) ;
  void from_json(nlohmann::json const& j, TIAState& state) ;
}

std::ostream& operator<< (std::ostream& os, sim::TIA::Register r) ;

#endif /* TIA_hpp */
