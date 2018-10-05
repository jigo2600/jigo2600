//  M6532.hpp
//  M6532 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef M6532_hpp
#define M6532_hpp

#include <cstddef>
#include <cstdint>
#include <iostream>
#include "json.hpp"

namespace jigo
{
  /// The state of the M6532 coprocessor.
  struct M6532State
  {
  public:
    virtual ~M6532State() = default ;
    bool operator== (M6532State const&) const ;

    /// The M6532 has only seven address bit `A[0:6]`.
    // The seven address bits are not enough to identify registers uniquely. For that,
    // we pre-pend the bits [RSnot,RW], obtaining register codes
    // of the type [RSnot,Rw].0xxx.xxx. Note that the 8-th bit is always
    // 0 as there are only 7 address bit.
    enum class Register : int {
      // RAM.
      RAM = 0x000,
      // Registers (write).
      ORA = 0x200, DDRA, ORB, DDRB,
      EDGCTL = 0x204,
      TIM1T = 0x214, TIM8T, TIM64T, T1024T,
      // Registers (read).
      INTIM = 0x304, INSTAT,
    } ;

    inline static Register decodeAddress(bool RSnot, bool Rw, std::uint16_t address) {
      // Many bit patterns are collapsed to the same 10-bit address of the type
      // [RSnot,Rw,*,A6,A5,...,A0].
      if (!RSnot) { // RAM (0*.*xxx.xxxx).
        return Register::RAM ;
      }
      else if (~address & 0x04) {
        // ORA, DDRA, ORB, DDRB (1*.****.*0xx): 0x200,0x201,0x202,0x203.
        return static_cast<Register>((address & 0x03) + 0x200) ;
      }
      else {
        if (Rw) {
          // INTIM, INSTAT (10.****.*1*x): 0x304,0x305.
          return static_cast<Register>((address & 0x05) + 0x300) ;
        } else {
          if (~address & 0x10) {
            // EDGCTL (11.***0.*1**): 0x204.
            return Register::EDGCTL ;
          } else {
            // TIM1T = 0x14, TIM8T, TIM64T, T1024T (11.***1.*1xx):
            // 0x214,0x215,0x216,0x217.
            return static_cast<Register>((address & 0x17) + 0x200) ;
          }
        }
      }
    }

    // RAM.
    std::array<std::uint8_t,128> ram ;

    // IO ports.
    std::uint8_t portA ;
    std::uint8_t portB ;
    std::uint8_t ORA ;
    std::uint8_t ORB ;
    std::uint8_t DDRA ;
    std::uint8_t DDRB ;

    // Timer.
    int timerInterval ;
    int unsigned timerCounter ;
    std::uint8_t INTIM ;

    // Interrupts.
    bool positiveEdgeDetect ;
    bool timerInterrupt ;
    bool timerInterruptEnabled ;
    bool pa7Interrupt ;
    bool pa7InterruptEnabled ;
  } ;

  void to_json(nlohmann::json& j, M6532State const& state) ;
  void from_json(nlohmann::json const& j, M6532State& state) ;

  /// The M6532 coprocessor.
  class M6532 : public M6532State
  {
  public:
    // Lifecycle.
    M6532() {reset();}
    M6532& operator= (M6532State const& s) {M6532State::operator=(s);return *this;}
    M6532& operator= (M6532 const&) = delete ;
    virtual ~M6532() = default ;

    // Operation.
    void reset() ;
    bool cycle(bool CS, bool RSnot, bool RW, std::uint16_t address, std::uint8_t& data) ;

    void setPortA(std::uint8_t a) {updateA((DDRA & ORA) | (~DDRA & a));}
    void setPortB(std::uint8_t b) {updateB((DDRB & ORB) | (~DDRB & b));}
    std::uint8_t getPortA() const {return portA;}
    std::uint8_t getPortB() const {return portB;}
    bool getIRQ() const {
      return
      (pa7Interrupt && pa7InterruptEnabled) ||
      (timerInterrupt && timerInterruptEnabled) ;
    }
    void setVerbose(bool x) {verbose = x;}

  protected:
    void updateA(std::uint8_t newA) {
      pa7Interrupt |=
      ((portA ^ newA) & 0x80) &&
      ((bool)portA ^ positiveEdgeDetect) ;
      portA = newA ;
    }
    void updateB(std::uint8_t newB) {
      portB = newB ;
    }
    // Transient.
    bool verbose ;
  } ;
} // namespace jigo

std::ostream & operator<< (std::ostream& os, jigo::M6532State::Register r) ;

#endif /* M6532_hpp */
