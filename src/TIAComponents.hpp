// TIAComponents.hpp
// Atari2600 TIA components emulation

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef TIAComponents_h
#define TIAComponents_h

#include <cstdint>
#include <array>
#include <iostream>
#include <algorithm>
#include <bitset>
#include <climits>
#include "json.hpp"

#undef jput
#undef jget
#undef cmp
#define jput(m) j[# m] = x.m
#define jget(m) x.m = j[# m]
#define cmp(x) (x == rhs.x)

namespace jigo {

  // Reflect bits in an integer.
  template <typename T>
  T reflect(T y) {
    T x = 0 ;
    for (int b = 0 ; b < sizeof(T)*CHAR_BIT ; ++b) {
      x <<= 1 ;
      x |= (y & 1) ;
      y >>= 1 ;
    }
    return x ;
  }

  // -----------------------------------------------------------------
  // MARK: - Dual phase, delay and delayed latch
  // -----------------------------------------------------------------

  class TIADualPhase
  {
  public:
    void cycle(bool CLK, bool RES) {
      if (RES) {
        phase = 0 ;
        RESL = RES ;
      } else if (CLK) {
        phase = (phase + 1) & 0x3 ;
        RESL &= (phase <= 1) ;
      }
    }

    int getPhase() const { return phase ; }
    bool getPhi1() const { return (phase == 0) ; }
    bool getPhi2() const { return (phase == 2) ; }
    bool getRESL() const { return RESL ; }

    void reset(int phase = 0, bool RESL = false) {
      this->phase = phase ;
      this->RESL = RESL ;
    }

    bool operator==(TIADualPhase const& rhs) const {
      return cmp(phase) && cmp(RESL) ;
    }

    friend void to_json(nlohmann::json& j, TIADualPhase const& x) {
      j = nlohmann::json::array({x.phase,x.RESL}) ;
    }

    friend void from_json(nlohmann::json const& j, TIADualPhase& x) {
      x.phase = j[0].get<int>(); x.RESL = j[1].get<bool>() ;
    }

  protected:
    int phase = 0 ;
    bool RESL = false ;
  } ;

  template <typename T>
  class TIADelay
  {
  public:
    void cycle(TIADualPhase const& phase, T data, bool reset = false) {
      if (phase.getPhi1()) { value[0] = data ; }
      else if (phase.getPhi2()) { value[1] = value[0] ; }
      if (reset) { value[1] = static_cast<T>(0) ; }
    }

    T get() const { return value[1] ; }

    void reset(T x = 0) {
      value = {x, x} ;
    }

    bool operator==(TIADelay const& rhs) const {
      return cmp(value) ;
    }

    template<class U> friend void
    to_json(nlohmann::json& j, TIADelay<U> const& x) ;

    template<class U> friend void
    from_json(nlohmann::json const& j, TIADelay<U>& x);

  protected:
    std::array<T,2> value ;
  } ;

  template<class U> void
  to_json(nlohmann::json& j, TIADelay<U> const& x) {
    j = x.value ;
  }

  template<class U> void
  from_json(nlohmann::json const& j, TIADelay<U>& x) {
    x.value = j ;
  }

  class TIADelayedLatch : public TIADelay<bool>
  {
  public:
    void cycle(TIADualPhase const& phase, bool value, bool reset) {
      TIADelay::cycle(phase, get() | value, reset) ;
    }
  } ;

  // -----------------------------------------------------------------
  // MARK: - Counter
  // -----------------------------------------------------------------

  template <int maxCount>
  class TIADualPhaseAndCounter : public TIADualPhase
  {
  public:
    void cycle(bool CLK, bool reset) {
      if (reset) {
        phase = 0 ;
        RESL = true ;
      } else if (CLK) {
        phase = (phase + 1) & 0x3 ;
        if (phase == 2) {
          RES = RESL | (C == maxCount) ;
          C = RES ? 0 : C + 1 ;
          RESL = false ;
        }
      }
    }

    int get() const { return C ; }
    bool getRES() const { return RES ; }

    void reset(int phase = 0, bool RESL = false, int C = 0, int RES = false) {
      TIADualPhase::reset(phase, RESL) ;
      this->C = C ;
      this->RES = RES ;
    }

    bool operator==(TIADualPhaseAndCounter const& rhs) const {
      return TIADualPhase::operator==(rhs) && cmp(C) && cmp(RES) ;
    }

    template <int n> friend void
    to_json(nlohmann::json& j, TIADualPhaseAndCounter<n> const& x) ;

    template <int n> friend void
    from_json(nlohmann::json const& j, TIADualPhaseAndCounter<n>& x) ;

  protected:
    int C ;
    bool RES ;
  } ;

  template <int n> void
  to_json(nlohmann::json& j, TIADualPhaseAndCounter<n> const& x) {
    j = nlohmann::json::array({x.phase,x.RESL,x.C,x.RES}) ;
  }

  template <int n> void
  from_json(nlohmann::json const& j, TIADualPhaseAndCounter<n>& x) {
    x.phase = j[0].get<int>() ;
    x.RESL  = j[1].get<bool>() ;
    x.C     = j[2].get<int>() ;
    x.RES   = j[3].get<bool>() ;
  }

  template <int maxCount>
  class TIACounter
  {
  public:
    void cycle(TIADualPhase const& phase) {
      RES.cycle(phase, phase.getRESL() || count.get() == maxCount) ;
      count.cycle(phase, count.get() + 1, RES.get()) ;
    }

    int get() const { return count.get() ; }
    bool getRES() const { return RES.get() ; }

    void reset(int C = 0, int RES = false) {
      this->count.reset(C) ;
      this->RES.reset(RES) ;
    }

    bool operator==(TIACounter<maxCount> const& rhs) const {
      return cmp(count) && cmp(RES) ;
    }

    friend void to_json(nlohmann::json& j, TIACounter const& x) {
      jput(count) ; jput(RES) ;
    }

    friend void from_json(nlohmann::json const& j, TIACounter& x) {
      jget(count) ; jget(RES) ;
    }

  protected:
    TIADelay<int> count ;
    TIADelay<bool> RES ;
  } ;

  template <int maxCount>
  class TIADualPhaseAndCounterAlt : public TIADualPhase, public TIACounter<maxCount>
  {
  public:
    void cycle(bool CLK, bool reset) {
      TIADualPhase::cycle(CLK,reset) ;
      TIACounter<maxCount>::cycle(*this) ;
    }

    void reset(int phase = 0, bool RESL = false, int C = 0, int RES = false) {
      TIADualPhase::reset(phase,RESL) ;
      TIACounter<maxCount>::reset(C,RES) ;
    }

    bool operator==(TIADualPhaseAndCounterAlt const& rhs) const {
      return TIADualPhase::operator==(rhs) && TIACounter<maxCount>::operator==(rhs) ;
    }

    template <int n>
    friend void to_json(nlohmann::json& j, TIADualPhaseAndCounterAlt<n> const& x) {
      j["phase"] = static_cast<TIADualPhase const&>(x) ;
      j["C"] = static_cast<TIACounter<n> const&>(x) ;
    }

    template <int n>
    friend void from_json(nlohmann::json const& j, TIADualPhaseAndCounterAlt<n>& x) {
      from_json(j["phase"],static_cast<TIADualPhase&>(x)) ;
      from_json(j["C"],static_cast<TIACounter<n>&>(x)) ; }
  } ;

  // -----------------------------------------------------------------
  // MARK: - TIASEC
  // -----------------------------------------------------------------

  class TIASEC
  {
  public:
    void cycle(TIADualPhase const& phase, bool HMOVE) {
      if (phase.getPhi2()) {
        SEC[1] = SEC[0] ;
      }
      HMOVEL |= HMOVE ;
      if (phase.getPhi1()) {
        HMOVEL &= !SEC[1] ;
        SEC[0] = HMOVEL ;
      }
    }

    bool get() const { return SEC[1] ; }

    void reset() {
      SEC = {false, false} ;
      HMOVEL = false ;
    }

    bool operator==(TIASEC const& rhs) const {
      return cmp(SEC) & cmp(HMOVEL) ;
    }

    friend void to_json(nlohmann::json& j, TIASEC const& x) {
      j = nlohmann::json::array({x.SEC, x.HMOVEL}) ;
    }

    friend void from_json(nlohmann::json const& j, TIASEC& x) {
      x.SEC = j[0].get<decltype(SEC)>() ;
      x.HMOVEL = j[1].get<decltype(HMOVEL)>() ;
    }

  protected:
    std::array<bool,2> SEC ;
    bool HMOVEL ;
  } ;

  // -----------------------------------------------------------------
  // MARK: - Extra clocks
  // -----------------------------------------------------------------

  class TIAExtraClock
  {
  public:
    void cycle(TIADualPhase const& phase, bool SEC, int HMC) {
      if (phase.getPhi1()) {
        bool RESET = (HMC == HM) ;
        ENA[0] = (ENA[1] | SEC) & !RESET ;
      }
      else if (phase.getPhi2()) {
        ENA[1] = ENA[0] ;
      }
    }

    void setHM(std::uint8_t data) {
      HM = 8 + (static_cast<int8_t>(data) >> 4) ;
    }

    void clearHM() {
      HM = 8 ;
    }

    bool get(TIADualPhase const& phase) const {
      return getENA() & phase.getPhi1() ;
    }

    bool getENA() const { return ENA[1] ; }

    void reset(std::array<bool,2> ENAC = {0, 0}, int HM = 0) {
      this->ENA = ENAC ;
      this->HM = HM ;
    }

    bool operator==(TIAExtraClock const& rhs) const {
      return cmp(ENA) && cmp(HM) ;
    }

    friend void to_json(nlohmann::json& j, TIAExtraClock const& x) {
      j = nlohmann::json::array({x.ENA, x.HM}) ;
    }

    friend void from_json(nlohmann::json const& j, TIAExtraClock& x) {
      x.ENA = j[0].get<decltype(ENA)>() ;
      x.HM = j[1].get<decltype(HM)>() ;
    }

  protected:
    std::array<bool,2> ENA ;
    int HM ;
  } ;

  // -----------------------------------------------------------------
  // MARK: - Playfield
  // -----------------------------------------------------------------

  class TIAPlayField
  {
  public:
    TIA_FORCE_INLINE
    void cycle(std::bitset<6>::reference bit, TIADualPhaseAndCounter<56> const& phasec) {
      if (phasec.getPhi2()) {
        mask = (mask << 1) & 0xfffff ;
        maskr = (maskr >> 1) & 0xfffff ;
      }
      mask |= (!REF && (phasec.get() == 36)) || (phasec.get() == 16) ;
      maskr |= (REF && (phasec.get() == 36)) << 19 ;
      PF.cycle(phasec, PFreg & (mask | maskr)) ;
      bit = get() ;
    }

    void setPF0(std::uint8_t value) {
      PFreg = (PFreg & 0xffff0) | ((((uint32_t)value) >> 4) & 0x0000f) ;
    }

    void setPF1(std::uint8_t value) {
      PFreg = (PFreg & 0xff00f) | tables.reflectAndShift[value] ;
    }

    void setPF2(std::uint8_t value) {
      PFreg = (PFreg & 0x00fff) | ((((uint32_t)value) <<12) & 0xff000) ;
    }

    void setCTRLPF(std::uint8_t D) {
      REF = D & 0x1 ;
      SCORE = D & 0x2 ;
      PFP = D & 0x4 ;
    }

    bool get() { return PF.get() ; }
    bool getREF() const { return REF ; }
    bool getSCORE() const { return SCORE ; }
    bool getPFP() const { return PFP ; }

    void reset() {
      REF = 0 ;
      SCORE = 0 ;
      PFP = 0 ;
    }

    bool operator==(TIAPlayField const& rhs) const {
      return
      cmp(PF)   && cmp(PFreg) &&
      cmp(mask) && cmp(maskr) &&
      cmp(REF)  && cmp(SCORE) &&
      cmp(PFP) ;
    }

    friend void to_json(nlohmann::json& j, TIAPlayField const& x) {
      jput(PF)   ; jput(PFreg) ;
      jput(mask) ; jput(maskr) ;
      jput(REF)  ; jput(SCORE) ;
      jput(PFP) ;
    }

    friend void from_json(nlohmann::json const& j, TIAPlayField& x) {
      jget(PF)   ; jget(PFreg) ;
      jget(mask) ; jget(maskr) ;
      jget(REF)  ; jget(SCORE) ;
      jget(PFP) ;
    }

  protected:
    static struct Tables {
      Tables() {
        for (int n = 0 ; n < 256 ; ++n) {
          reflectAndShift[n] = ((unsigned int)reflect((uint8_t)n)) << 4 ;
        }
      }
      unsigned int reflectAndShift [256] ;
    } tables ;

    TIADelay<bool> PF ;
    std::uint32_t PFreg = 0 ; // PF0-2
    std::uint32_t mask = 0 ;
    std::uint32_t maskr = 0 ;

    // In CTRLPF:
    bool REF = false ;
    bool SCORE = false ;
    bool PFP = false ;
  } ;

  // -----------------------------------------------------------------
  // MARK: - Player
  // -----------------------------------------------------------------

  class TIAPlayer
  {
  public:
    TIA_FORCE_INLINE
    void cycle(bool CLK, bool PLRE)
    {
      if (CLK) {
        // Update SC. This is an in-place update of edge sensitive logic,
        // so it must occur before updating ENA.
        if (ENA) {
          if (SC) { SC >>= 1 ; }
          else if (START.get()) { SC = (1 << 7) ; }
        }
        // Update ENA (enable counting).
        if (NUSIZ == 5) {
          ENA = (PC.getPhi1() || PC.getPhi2()) ;
        }
        else if (NUSIZ == 7) {
          ENA = PC.getPhi2() ;
        }
        else {
          ENA = true ;
        }
      }
      // Update the dual-phase driven logic.
      PC.cycle(CLK, PLRE) ;
      START.cycle(PC, tables.start[NUSIZ][PC.get()]) ;
    }

    bool get() const {
      return graphics & SC ;
    }

    uint8_t getNUSIZ() const {
      return NUSIZ ; 
    }

    bool getRESMP() const {
      return ENA && (SC == 1) && (START.get() == 1) ;
    }

    void shiftGRP() { GRP[1] = GRP[0]  ; sync() ; }
    void setNUSIZ(uint8_t D) { NUSIZ = D & 0x7 ; }
    void setGRP(uint8_t D) { GRP[0] = D ; sync() ; }
    void setVDELP(uint8_t D) { VDELP = D & 0x1 ; sync() ; }
    void setREFL(uint8_t D) { REFL = D & 0x8 ; sync() ; }

    void reset() {
      PC.reset() ;
      START.reset() ;
      SC = 0 ;
      GRP = {0,0} ;
      NUSIZ = 0 ;
      VDELP = false ;
      ENA = false ;
      REFL = false ;
      sync() ;
    }

    void sync() {
      graphics = GRP[VDELP] ;
      if (REFL) { graphics = tables.refl[graphics] ; }
    }

    bool operator==(TIAPlayer const& rhs) const {
      return
      cmp(PC)    && cmp(START) && cmp(SC)  && cmp(GRP)  &&
      cmp(NUSIZ) && cmp(VDELP) && cmp(ENA) && cmp(REFL) ;
    }

    friend void to_json(nlohmann::json& j, TIAPlayer const& x) {
      jput(PC)    ; jput(START) ; jput(SC)  ; jput(GRP)  ;
      jput(NUSIZ) ; jput(VDELP) ; jput(ENA) ; jput(REFL) ;
    }

    friend void from_json(nlohmann::json const& j, TIAPlayer& x) {
      jget(PC)    ; jget(START) ; jget(SC)  ; jget(GRP)  ;
      jget(NUSIZ) ; jget(VDELP) ; jget(ENA) ; jget(REFL) ;
      x.sync() ;
    }

  protected:
    static struct Tables {
      int start [8][40] ;
      uint8_t refl [256] ;
      Tables() {
        for (int NUSIZ = 0 ; NUSIZ < 8 ; ++NUSIZ) {
          for (int C = 0 ; C < 40 ; ++C) {
            // The `start` table distingushes 1st and other copies in order
            // to implement FSTOB and RESMP.
            start[NUSIZ][C] =
            ((C == 39)) ? 1 : 2 *
            (((C ==  3) && (NUSIZ == 1 || NUSIZ == 3)) ||
             ((C ==  7) && (NUSIZ == 2 || NUSIZ == 3 || NUSIZ == 6)) ||
             ((C == 15) && (NUSIZ == 4 || NUSIZ == 6)))  ;
          }
        }
        for (int i = 0  ; i < 256 ; ++i) {
          refl[i] = reflect((uint8_t)i) ;
        }
      }
    } tables ;

    TIADualPhaseAndCounter<39> PC ;
    TIADelay<int> START ;
    int SC ;
    std::array<uint8_t,2> GRP ;
    int NUSIZ ;
    bool VDELP ;
    bool ENA ;
    bool REFL ;
    // Transient.
    int graphics ;
  } ;

  class TIAPlayerAlt
  {
  public:
    void cycle(bool CLK, bool PLRE) {
      if (CLK) {
        // Update SC.
        if (ENA) {
          if (SC < 8) { SC++ ; }
          else if (START.get()) { SC = 0 ; }
        }
        // Update ENA (enable counting).
        if (NUSIZ == 5) {
          ENA = (phasec.getPhi1() || phasec.getPhi2()) ;
        }
        else if (NUSIZ == 7) {
          ENA = phasec.getPhi2() ;
        }
        else {
          ENA = true ;
        }
      }
      // Update the dual-phase driven logic.
      phasec.cycle(CLK, PLRE) ;
      // The START signal distingushes 1st and other copies for RESMP.
      START.cycle(phasec,
                  ((phasec.get() == 39)) ? 1 : 2 *
                  (((phasec.get() ==  3) && (NUSIZ == 1 || NUSIZ == 3)) ||
                   ((phasec.get() ==  7) && (NUSIZ == 2 || NUSIZ == 3 || NUSIZ == 6)) ||
                   ((phasec.get() == 15) && (NUSIZ == 4 || NUSIZ == 6)))) ;
    }

    bool get() const {
      if (SC < 8) {
        uint8_t grp = GRP[VDELP] ;
        int i =  SC ;
        if (!REFL) { i = 7 - i ; }
        return (grp >> i) & 0x1 ;
      }
      return false ;
    }

    uint8_t getNUSIZ() const { return NUSIZ ; }

    bool getRESMP() const {
      return ENA && (SC == 1) && (START.get() == 1) ;
    }

    void setNUSIZ(uint8_t D) { NUSIZ = D & 0x7 ; }
    void setGRP(uint8_t D) { GRP[0] = D ; }
    void setVDELP(uint8_t D) { VDELP = D & 0x1 ; }
    void setREFL(uint8_t D) { REFL = D & 0x8 ; }
    void shiftGRP() { GRP[1] = GRP[0] ; }

    void reset() {
      phasec.reset() ;
      START.reset() ;
      SC = 0 ;
      GRP = {0,0} ;
      NUSIZ = 0 ;
      VDELP = false ;
      ENA = false ;
      REFL = false ;
    }

    bool operator==(TIAPlayerAlt const& rhs) const {
      return
      cmp(phasec) && cmp(START) &&
      cmp(SC)     && cmp(GRP)   &&
      cmp(NUSIZ)  && cmp(VDELP) &&
      cmp(ENA)    && cmp(REFL)  ;
    }

    friend void to_json(nlohmann::json& j, TIAPlayerAlt const& x) {
      jput(phasec) ; jput(START) ;
      jput(SC)     ; jput(GRP)   ;
      jput(NUSIZ)  ; jput(VDELP) ;
      jput(ENA)    ; jput(REFL)  ;
    }

    friend void from_json(nlohmann::json const& j, TIAPlayerAlt& x) {
      jget(phasec) ; jget(START) ;
      jget(SC)     ; jget(GRP)   ;
      jget(NUSIZ)  ; jget(VDELP) ;
      jget(ENA)    ; jget(REFL)  ;
    }

  protected:
    TIADualPhaseAndCounterAlt<39> phasec ;
    TIADelay<int> START ;
    std::array<uint8_t,2> GRP ;
    int SC ;
    int NUSIZ ;
    bool VDELP ;
    bool ENA ;
    bool REFL ;
  } ;

  // -----------------------------------------------------------------
  // MARK: - Missile
  // -----------------------------------------------------------------

  class TIAMissile
  {
  public:
    TIA_FORCE_INLINE
    void cycle(bool CLK, bool MRE, TIAPlayer const& P) {
      // Update the dual-phase logic.
      bool RES = MRE || (RESMP && P.getRESMP()) ;
      MC.cycle(CLK, RES) ;
      if (MC.getPhi1()) {
        START = tables.start[P.getNUSIZ()][MC.get()] ;
      }
      if (RES) {
        counter = (counter & 0xffc) | 0x2 ;
      } else {
        counter += CLK ;
      }
      if (START && MC.getPhi2()) counter = 0 ;
    }

    bool get() const {
      return enabled && (counter < stop) ;
    }

    void setENAM(uint8_t D) { ENAM = D & 0x2 ; sync() ; }
    void setRESMP(uint8_t D) { RESMP = D & 0x2 ; sync() ; }
    void setSIZ(uint8_t D) { SIZ = (D >> 4) & 0x3 ; sync() ; }

    void reset() {
      MC.reset() ;
      counter = 8 ;
      START = false ;
      ENAM = false ;
      RESMP = false ;
      SIZ = 0 ;
      sync() ;
    }

    void sync() {
      enabled = ENAM && !RESMP ;
      stop = 1 << SIZ ;
    }

    bool operator==(TIAMissile const& rhs) const {
      return
      cmp(MC)     && cmp(START)   &&
      cmp(SIZ)    && cmp(ENAM)    &&
      cmp(RESMP)  && cmp(counter) ;
    }

    friend void to_json(nlohmann::json& j, TIAMissile const& x) {
      jput(MC)    ; jput(START)   ;
      jput(SIZ)   ; jput(ENAM)    ;
      jput(RESMP) ; jput(counter) ;
    }

    friend void from_json(nlohmann::json const& j, TIAMissile& x) {
      jget(MC)    ; jget(START)   ;
      jget(SIZ)   ; jget(ENAM)    ;
      jget(RESMP) ; jget(counter) ;
      x.sync() ;
    }

  protected:
    static struct Tables {
      bool start [8][40] ;
      Tables()
      {
        for (int NUSIZ = 0 ; NUSIZ < 8 ; ++NUSIZ) {
          for (int C = 0 ; C < 40 ; ++C) {
            start[NUSIZ][C] =
            ((C == 39)) ||
            ((C ==  3) && (NUSIZ == 1 || NUSIZ == 3)) ||
            ((C ==  7) && (NUSIZ == 2 || NUSIZ == 3 || NUSIZ == 6)) ||
            ((C == 15) && (NUSIZ == 4 || NUSIZ == 6)) ;
          }
        }
      }
    } tables ;
    TIADualPhaseAndCounter<39> MC ;
    bool START ;
    int SIZ ;
    bool ENAM ;
    bool RESMP ;
    int counter ;
    // Transient state.
    bool enabled ;
    int stop ;
  } ;

  class TIAMissileAlt
  {
  public:
    void cycle(bool CLK, bool MRE, TIAPlayerAlt const& PL) {
      // Update the dual-phase logic.
      MC.cycle(CLK, MRE || (RESMP && PL.getRESMP())) ;
      int NUSIZ = PL.getNUSIZ() ;
      START1.cycle(MC,
                   ((MC.get() == 39)) ||
                   ((MC.get() ==  3) && (NUSIZ == 1 || NUSIZ == 3)) ||
                   ((MC.get() ==  7) && (NUSIZ == 2 || NUSIZ == 3 || NUSIZ == 6)) ||
                   ((MC.get() == 15) && (NUSIZ == 4 || NUSIZ == 6))) ;
      START2.cycle(MC, START1.get()) ;
    }

    bool get() const {
      if (ENAM && !RESMP) {
        if (START1.get()) {
          return
          (SIZ>=2) ||
          ((SIZ>=1) & (MC.getPhase()>=2)) ||
          (MC.getPhase()==2) ;
        }
        else if (START2.get()) {
          return (SIZ==3) ;
        }
      }
      return false ;
    }

    void setENAM(uint8_t D) { ENAM = D & 0x2 ; }
    void setRESMP(uint8_t D) { RESMP = D & 0x2 ; }
    void setSIZ(uint8_t D) { SIZ = (D >> 4) & 0x3 ; }

    void reset() {
      MC.reset() ;
      START1.reset() ;
      START2.reset() ;
      RESMP = false ;
      ENAM = false ;
      SIZ = 0 ;
    }

    bool operator==(TIAMissileAlt const& rhs) const {
      return
      cmp(MC)    && cmp(SIZ)    && cmp(ENAM)   &&
      cmp(RESMP) && cmp(START1) && cmp(START2) ;
    }

    friend void to_json(nlohmann::json& j, TIAMissileAlt const& x) {
      jput(MC)    ; jput(SIZ)    ; jput(ENAM)   ;
      jput(RESMP) ; jput(START1) ; jput(START2) ;
    }

    friend void from_json(nlohmann::json const& j, TIAMissileAlt& x) {
      jget(MC)    ; jget(SIZ)    ; jget(ENAM)   ;
      jget(RESMP) ; jget(START1) ; jget(START2) ;
    }

  protected:
    TIADualPhaseAndCounterAlt<39> MC ;
    int SIZ ;
    bool ENAM ;
    bool RESMP ;
    TIADelay<bool> START1 ;
    TIADelay<bool> START2 ;
  } ;

  // -----------------------------------------------------------------
  // MARK: - Ball
  // -----------------------------------------------------------------

  class TIABall
  {
  public:
    TIA_FORCE_INLINE
    void cycle(bool CLK, bool BLRE)
    {
      BC.cycle(CLK, BLRE) ;
      if (BLRE) {
        counter = (counter & 0xffc) | 0x2 ;
      } else {
        counter += CLK ;
      }
      if (BC.getRES() && BC.getPhi2()) counter = 0 ;
    }

    bool get() const {
      return enabled && (counter < stop) ;
    }

    void setBLEN(uint8_t D) { BLEN[0] = D & 0x2 ; sync() ; }
    void setBLVD(uint8_t D) { BLVD = D & 0x1 ; sync() ; }
    void setBLSIZ(uint8_t D) { BLSIZ = (D >> 4) & 0x3 ; sync() ; }
    void shiftBLEN() { BLEN[1] = BLEN[0] ; sync() ; }

    void reset() {
      BC.reset() ;
      counter = 8 ;
      BLEN = {false, false} ;
      BLSIZ = 0 ;
      BLVD = 0 ;
      sync() ;
    }

    void sync() {
      enabled = BLEN[BLVD] ;
      stop = 1 << BLSIZ ;
    }

    bool operator==(TIABall const& rhs) const {
      return
      cmp(BC)   && cmp(BLEN)    && cmp(BLSIZ) &&
      cmp(BLVD) && cmp(counter) ;
    }

    friend void to_json(nlohmann::json& j, TIABall const& x) {
      jput(BC)   ; jput(BLEN)    ; jput(BLSIZ) ;
      jput(BLVD) ; jput(counter) ;
    }

    friend void from_json(nlohmann::json const& j, TIABall& x) {
      jget(BC)   ; jget(BLEN)    ; jget(BLSIZ) ;
      jget(BLVD) ; jget(counter) ;
      x.sync();
    }

  protected:
    TIADualPhaseAndCounter<39> BC ;
    std::array<bool,2> BLEN ; // In ENABL.
    int BLSIZ ; // In CTLRPF.
    bool BLVD ; // In VDELBL.
    int counter ;
    // Transietnt.
    bool enabled ;
    int stop ;
  } ;

  class TIABallAlt
  {
  public:
    void cycle(bool CLK, bool BLRE)
    {
      phasec.cycle(CLK, BLRE) ;
      auto START1 = phasec.getRES() ;
      START2.cycle(phasec, START1) ;
    }

    bool get() const {
      auto START1 = phasec.getRES() ;
      if (BLEN[BLVD]) {
        if (START1) {
          return
          (BLSIZ>=2) ||
          ((BLSIZ>=1) & (phasec.getPhase()>=2)) ||
          (phasec.getPhase()==2) ;
        }
        else if (START2.get()) {
          return (BLSIZ==3) ;
        }
      }
      return false ;
    }

    void setBLEN(uint8_t D) { BLEN[0] = D & 0x2 ; }
    void setBLVD(uint8_t D) { BLVD = D & 0x1 ; }
    void setBLSIZ(uint8_t D) { BLSIZ = (D >> 4) & 0x3 ;  }
    void shiftBLEN() { BLEN[1] = BLEN[0] ; }

    void reset() {
      phasec.reset() ;
      START2.reset() ;
      BLEN[0] = BLEN[1] = false ;
      BLSIZ = 0 ;
      BLVD = 0 ;
    }

    bool operator==(TIABallAlt const& rhs) const {
      return
      cmp(phasec) && cmp(START2) &&
      cmp(BLEN)   && cmp(BLSIZ)  &&
      cmp(BLVD)   ;
    }

    friend void to_json(nlohmann::json& j, TIABallAlt const& x) {
      jput(phasec) ; jput(START2) ;
      jput(BLEN)   ; jput(BLSIZ)  ;
      jput(BLVD)   ;
    }

    friend void from_json(nlohmann::json const& j, TIABallAlt& x) {
      jget(phasec) ; jget(START2) ;
      jget(BLEN)   ; jget(BLSIZ)  ;
      jget(BLVD)   ;
    }

  protected:
    TIADualPhaseAndCounterAlt<39> phasec ;
    TIADelay<bool> START2 ;
    std::array<bool,2> BLEN ; // In ENABL.
    int BLSIZ ; // In CTLRPF.
    bool BLVD ; // In VDELBL.
  } ;

  // -----------------------------------------------------------------
  // MARK: - Ports
  // -----------------------------------------------------------------

  class TIAPorts {
  public:
    bool operator==(TIAPorts const& rhs) const {
      return
      cmp(INPT) &&
      cmp(charges) &&
      cmp(chargingRates) &&
      cmp(I45) &&
      cmp(INPT45Latched) &&
      cmp(INPT0123Dumped) ;
    }
    
    void reset() {
      INPT = {0} ;
      charges = {0} ;
      chargingRates = {0} ;
      I45 = {0} ;
    }

    TIA_FORCE_INLINE
    void cycle(TIADualPhaseAndCounter<56> const& Hphasec) {
      if (Hphasec.getPhi2() && Hphasec.get() == 0) {
        // INPT0-3 RC circuits.
        if (INPT0123Dumped) {
          charges = {0} ;
          std::fill(begin(INPT),begin(INPT)+4,0) ;
        } else {
          for (int k = 0 ; k < 4 ; ++k) {
            charges[k] += chargingRates[k] ;
            charges[k] = std::min(1.0f, std::max(0.f, charges[k])) ;
            INPT[k] = (charges[k] >= 1.0f) ;
          }
        }
      }
    }

    std::array<float,4> const& getI03() const {
      return chargingRates ;
    }

    void setI03(std::array<float,4> const & chargingRates) {
      this->chargingRates = chargingRates ;
    }

    std::array<bool,2> const & getI45() const {
      return I45 ;
    }

    void setI45(std::array<bool,2> const & I45) {
      this->I45 = I45 ;
      INPT[4] = I45[0] || (INPT45Latched && INPT[4]) ;
      INPT[5] = I45[1] || (INPT45Latched && INPT[5]) ;
    }

    void setINPT(std::uint8_t D) {
      // See sheet 5.
      INPT45Latched =  D & 0x40 ;
      INPT0123Dumped = D & 0x80 ;
      setI45(I45) ; // reflect latched status
    }

    std::uint8_t getINPT(int num) {return INPT[num] ? 0x80 : 0;}

    friend void to_json(nlohmann::json& j, TIAPorts const& x) {
      jput(INPT);
      jput(charges) ;
      jput(chargingRates) ;
      jput(I45) ;
      jput(INPT45Latched) ;
      jput(INPT0123Dumped) ;
    }

    friend void from_json(nlohmann::json const& j, TIAPorts& x) {
      jget(INPT);
      jget(charges) ;
      jget(chargingRates) ;
      jget(I45) ;
      jget(INPT45Latched) ;
      jget(INPT0123Dumped) ;
    }

  protected:
    std::array<bool,6> INPT ;
    std::array<float,4> chargingRates ;
    std::array<float,4> charges ;
    std::array<bool,2> I45 ;
    bool INPT45Latched ;
    bool INPT0123Dumped ;
  } ;
}

#undef jput
#undef jget
#undef cmp
#endif /* TIAComponents_h */
