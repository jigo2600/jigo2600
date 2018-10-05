//  M6532.cpp
//  M6532 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "M6532.hpp"
#include "string.h"

using namespace std ;
using namespace jigo ;
using json = nlohmann::json ;

#define cmp(x) (x==s.x)
bool M6532State::operator==(const jigo::M6532State &s) const {
  return
  cmp(portA) && cmp(portB) &&
  cmp(ORA) && cmp(ORB) &&
  cmp(DDRA) && cmp(DDRB) &&
  cmp(timerInterval) && cmp(timerCounter) &&
  cmp(INTIM) && cmp(positiveEdgeDetect) &&
  cmp(timerInterrupt) && cmp(timerInterruptEnabled) &&
  cmp(pa7Interrupt) && cmp(pa7InterruptEnabled) ;
}
#undef cmp

#define N(x) {M6532State::Register::x,# x}
static map<M6532State::Register,char const *> registerNames
{
  N(RAM),
  N(ORA), N(DDRA), N(ORB), N(DDRB), N(EDGCTL),
  N(TIM1T), N(TIM8T), N(TIM64T), N(T1024T), N(INTIM), N(INSTAT)
} ;
#undef N

void M6532::reset()
{
  fill(begin(ram),end(ram),0) ;
  portA = 0 ;
  portB = 0 ;
  ORA = 0 ;
  ORB = 0 ;
  DDRA = 0 ;
  DDRB = 0 ;
  timerInterval = 1024 ;
  timerCounter = 0 ;
  INTIM = 0 ;
  positiveEdgeDetect = false ;
  timerInterrupt = false ;
  timerInterruptEnabled = false ;
  pa7Interrupt = false ;
  pa7InterruptEnabled = false ;
}

bool M6532::cycle(bool CS, bool RSnot, bool RW, uint16_t address, uint8_t& data)
{
  // Timer.
  if (timerInterrupt) {
#if 1
    if (INTIM != 0x80) { INTIM-- ; } // Up to -255
#else
    INTIM-- ;
#endif
  }
  else if ((timerCounter & (timerInterval - 1)) == 0) {
    // The interrupt is raised the cycle *after* INTIM reaches 0.
    timerInterrupt |= (INTIM-- == 0) ;
  }
  timerCounter++ ;

  // Registers.
  if (CS) {
    auto reg = decodeAddress(RSnot, RW, address) ;
    if (RW) { // Read.
      switch (reg) {
        case Register::RAM:   data = ram[address & 0x7f] ; break ;
        case Register::ORA:   data = (DDRA & ORA) | (~DDRA & portA) ; break ;
        case Register::DDRA:  data = DDRA ; break ;
        case Register::ORB:   data = (DDRB & ORB) | (~DDRB & portB) ; break ;
        case Register::DDRB:  data = DDRB ; break ;
        case Register::INTIM:
          data = INTIM ;
          // See manual: interrupt is not cleared if INTIM is read
          // exactly at the cycle when the interrupt occurred.
          // Write does appear to clear it?
          if (INTIM != 0xff) { timerInterrupt = false ; }
          timerInterruptEnabled = address & 0x08 ;
          break ;
        case Register::INSTAT:
          data = (timerInterrupt << 7) | (pa7Interrupt << 6) ;
          pa7Interrupt = false ;
          break ;
        default: break ;
      }
    } else { // Write.
      switch (reg) {
        case Register::RAM: ram[address & 0x7f] = data; break;
        case Register::ORA:
          ORA = data ;
          updateA((DDRA & ORA) | (~DDRA & portA)) ;
          return bool(DDRA) ; // signal possible change in out port
        case Register::DDRA: DDRA = data ; return bool(DDRA) ;
        case Register::ORB:
          ORB = data ;
          updateB((DDRB & ORA) | (~DDRB & portB)) ;
          return bool(DDRB) ;
        case Register::DDRB: DDRB = data ; return bool(DDRB) ;
        case Register::TIM1T:
        case Register::TIM8T:
        case Register::TIM64T:
        case Register::T1024T:
          switch (address & 0x03) {
            case 0: timerInterval = 1 ; break ;
            case 1: timerInterval = 8 ; break ;
            case 2: timerInterval = 64 ; break ;
            case 3: timerInterval = 1024 ; break ;
          }
          timerInterruptEnabled = address & 0x08 ;
          timerInterrupt = false ;
          timerCounter = false ;
          INTIM = data ;
          break ;
        case Register::EDGCTL:
          pa7InterruptEnabled = address & 0x02 ;
          positiveEdgeDetect = address & 0x01 ;
          break ;
        default: break ;
      }
    }
  }
  return false ;
}

void jigo::to_json(nlohmann::json& j, M6532State const& state)
{
#undef jput
#define jput(x) j[# x] = state.x
  jput(ram) ;
  jput(portA) ;
  jput(ORA) ;
  jput(DDRA) ;
  jput(portB) ;
  jput(ORB) ;
  jput(DDRB) ;
  jput(timerInterval) ;
  jput(timerCounter) ;
  jput(INTIM) ;
  jput(positiveEdgeDetect) ;
  jput(timerInterrupt) ;
  jput(timerInterruptEnabled) ;
  jput(pa7Interrupt) ;
  jput(pa7InterruptEnabled) ;
}

/// Throws `nlohmann::json::exception` on parsing errors.
void jigo::from_json(nlohmann::json const& j, M6532State& state)
{
#undef jget
#define jget(m) state.m = j[# m].get<decltype(state.m)>()
  jget(ram) ;
  jget(portA) ;
  jget(ORA) ;
  jget(DDRA) ;
  jget(portB) ;
  jget(ORB) ;
  jget(DDRB) ;
  jget(timerInterval) ;
  jget(timerCounter) ;
  jget(INTIM) ;
  jget(positiveEdgeDetect) ;
  jget(timerInterrupt) ;
  jget(timerInterruptEnabled) ;
  jget(pa7Interrupt) ;
  jget(pa7InterruptEnabled) ;
}

std::ostream & operator<< (std::ostream& os, M6532State::Register r)
{
  auto n = registerNames.find(r) ;
  if (n != registerNames.end()) {
    os << n->second ;
  } else {
    os << hex << setfill('0') << setw(2) << (int)r << " (PIA?)" ;
  }
  return os ;
}
