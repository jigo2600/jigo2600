//  M6532.hpp
//  M6532 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef M6532_hpp
#define M6532_hpp

#include "json.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace jigo {

/// M6532 coprocessor state.
struct M6532State {
public:
  /// Coprocessor register indentifier (inlcluding possibly RAM).
  // We use the following convenvention in order to assign codes to registers.
  // The M6532 has only seven address bits `A[0:6]`, and these are not enough
  // to identify registers uniquely; effectively, `RSnot` an `RW` are used as
  // additional address bits. Thus, codes are virtual addresses obtained by
  // pre-pending the bits `[RSnot,RW]` to the two address nibbles, obtaining
  // register codes of the type `[RSnot,Rw].0xxx.xxx`. Note that the 8th bit
  // is always 0 as there are only 7 address bits.
  enum class Register : int {
    // clang-format off
      // RAM.
      RAM = 0x000,
      // Registers (write).
      ORA = 0x200, DDRA, ORB, DDRB,
      EDGCTL = 0x204,
      TIM1T = 0x214, TIM8T, TIM64T, T1024T,
      // Registers (read).
      INTIM = 0x304, INSTAT,
    // clang-format on
  };

  virtual ~M6532State() = default;
  bool operator==(M6532State const&) const;
  static Register decodeAddress(bool RSnot, bool Rw, std::uint16_t address);

  // RAM.
  std::array<std::uint8_t, 128> ram{};

  // IO ports.
  std::uint8_t portA{};
  std::uint8_t portB{};
  std::uint8_t ORA{};
  std::uint8_t ORB{};
  std::uint8_t DDRA{};
  std::uint8_t DDRB{};

  // Timer.
  int timerInterval{1024};
  int unsigned timerCounter{};
  std::uint8_t INTIM{};

  // Interrupts.
  bool positiveEdgeDetect{};
  bool timerInterrupt{};
  bool timerInterruptEnabled{};
  bool pa7Interrupt{};
  bool pa7InterruptEnabled{};
};

void to_json(nlohmann::json& j, M6532State const& state);
void from_json(nlohmann::json const& j, M6532State& state);

/// M6532 coprocessor.
class M6532 : public M6532State {
public:
  // Lifecycle.
  M6532& operator=(M6532State const& s);
  virtual ~M6532() = default;

  // Operation.
  void reset();
  bool cycle(bool CS, bool RSnot, bool RW, std::uint16_t address, std::uint8_t& data);
  void writePortA(std::uint8_t a);
  void writePortB(std::uint8_t b);
  bool getIRQ() const;
  bool getVerbose() const;
  void setVerbose(bool x);

protected:
  void updateA(std::uint8_t newA);
  void updateB(std::uint8_t newB);

  // Transient.
  bool verbose;
};

// ---------------------------------------------------------------------------
// Inline members
// ---------------------------------------------------------------------------

inline M6532State::Register M6532State::decodeAddress(bool RSnot, bool Rw,
                                                      std::uint16_t address) {
  // Many bit patterns are collapsed to the same 10-bit address of the type
  // [RSnot,Rw,*,A6,A5,...,A0].
  if (!RSnot) { // RAM (0*.*xxx.xxxx).
    return Register::RAM;
  } else if (~address & 0x04) {
    // ORA, DDRA, ORB, DDRB (1*.****.*0xx): 0x200,0x201,0x202,0x203.
    return static_cast<Register>((address & 0x03) + 0x200);
  } else {
    if (Rw) {
      // INTIM, INSTAT (10.****.*1*x): 0x304,0x305.
      return static_cast<Register>((address & 0x05) + 0x300);
    } else {
      if (~address & 0x10) {
        // EDGCTL (11.***0.*1**): 0x204.
        return Register::EDGCTL;
      } else {
        // TIM1T = 0x14, TIM8T, TIM64T, T1024T (11.***1.*1xx):
        // 0x214,0x215,0x216,0x217.
        return static_cast<Register>((address & 0x17) + 0x200);
      }
    }
  }
}

// Only copy the processor state, not the class transient state.
inline M6532& M6532::operator=(M6532State const& s) {
  M6532State::operator=(s);
  return *this;
}

inline void M6532::writePortA(std::uint8_t a) {
  updateA((DDRA & ORA) | (~DDRA & a));
}

inline void M6532::writePortB(std::uint8_t b) {
  updateB((DDRB & ORB) | (~DDRB & b));
}

inline bool M6532::getIRQ() const {
  return (pa7Interrupt && pa7InterruptEnabled) ||
         (timerInterrupt && timerInterruptEnabled);
}

inline void M6532::updateA(std::uint8_t newA) {
  pa7Interrupt |= ((portA ^ newA) & 0x80) && ((bool)portA ^ positiveEdgeDetect);
  portA = newA;
}

inline void M6532::updateB(std::uint8_t newB) {
  portB = newB;
}

inline bool M6532::getVerbose() const {
  return verbose;
}

inline void M6532::setVerbose(bool x) {
  verbose = x;
}

} // namespace jigo

std::ostream& operator<<(std::ostream& os, jigo::M6532::Register r);

#endif /* M6532_hpp */
