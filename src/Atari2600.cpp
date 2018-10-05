// Atari2600.cpp
// Atari2600 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "Atari2600.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

using namespace std ;
using namespace jigo ;
using json = nlohmann::json ;

std::ostream & operator<< (std::ostream& os, Atari2600::DecodedAddress const& da)
{
  if (da.device == Atari2600::DecodedAddress::TIA) {
    os << da.tiaRegister ;
  }
  else if (da.device == Atari2600::DecodedAddress::PIA) {
    if (da.piaRegister == M6532State::Register::RAM) {
      os
      << "$" << hex << setfill('0') << uppercase << setw(2)
      << (int)da.address << "_RAM" ;
    } else {
      os << da.piaRegister ;
    }
  }
  else {
    os
    << "$" << hex << setfill('0') << uppercase << setw(4)
    << (int)da.address << "_ROM" ;
  }
  return os ;
}

// -------------------------------------------------------------------
// MARK: - Serlialize & deserialize state
// -------------------------------------------------------------------

void jigo::to_json(json& j, const Atari2600State& state)
{
  j["version"] = "1.0" ;
  j["cpu"] = *state.cpu ;
  j["pia"] = *state.pia ;
  j["tia"] = *state.tia ;
  if (state.cartridge) {
    json jx ;
    state.cartridge->serialize(jx) ;
    j["cart"] = jx ;
  }
}

/// Throws `nlohmann::json::exception` on parsing errors.
void jigo::from_json(const json& j, Atari2600State& state)
{
  *state.cpu = j["cpu"] ;
  *state.pia = j["pia"] ;
  *state.tia = j["tia"] ;
  if (state.cartridge) {
    const json& jx = j.at("cart") ;
    state.cartridge->deserialize(jx) ;
  }
}

// -------------------------------------------------------------------
// MARK: - Decode addresses
// -------------------------------------------------------------------

Atari2600::DecodedAddress::DecodedAddress(uint32_t address, bool Rw)
{
  this->address = address ;
  this->Rw = Rw ;
  if (address & (1 << 12)) {
    this->device = DecodedAddress::Cartridge ;
  }
  else if (address & (1 << 7)) {
    this->device = DecodedAddress::PIA ;
    this->piaRegister = M6532::decodeAddress(address & 0x200, Rw, address) ;
  }
  else {
    this->device = DecodedAddress::TIA ;
    this->tiaRegister = jigo::TIA::decodeAddress(Rw,address) ;
  }
}

// -------------------------------------------------------------------
// MARK: - Atari2600State
// -------------------------------------------------------------------

/// Create a new state using the specified component states.
Atari2600State::Atari2600State
(std::shared_ptr<M6502State> cpu,
 std::shared_ptr<M6532State> pia,
 std::shared_ptr<jigo::TIAState> tia,
 std::shared_ptr<Atari2600CartridgeState> cartridge)
: cpu(cpu), pia(pia), tia(tia), cartridge(cartridge)
{ }

/// Create a new state instance. The new cartridge state is null.
Atari2600State::Atari2600State()
{
  cpu = make_shared<M6502State>() ;
  pia = make_shared<M6532State>() ;
  tia = make_shared<jigo::TIAState>() ;
  cartridge = nullptr ;
}

// Helper.
template<class T, class U>
bool cmp(const std::shared_ptr<T>&a,const std::shared_ptr<U>&b)
{
  if(a == b) return true;
  if(a && b) return *a == *b;
  return false;
}

/// Compare two states for value equality.
bool Atari2600State::operator== (Atari2600State const &s) const {
  return
  cmp(cpu, s.cpu) &&
  cmp(pia, s.pia) &&
  cmp(tia, s.tia) &&
  cmp(cartridge, s.cartridge) ;
}

// -------------------------------------------------------------------
// MARK: - Simulation
// -------------------------------------------------------------------

/// Run the simulation until `maxNumCPUClocks` have been executed, a new frame is
/// generated, or a breakpoint is reached, depending which events occur first.
/// Note that more than one of these criteria can be met at the same time; the function returns all reasons why it stopped.

Atari2600::StoppingReason
Atari2600::cycle(size_t &maxNumCPUCycles)
{
  // Handle inputs.
  syncPorts() ;

  // Simulate cycles.
  StoppingReason reason ;
  auto _cpu = getCpu() ;
  auto _pia = getPia() ;
  auto _tia = getTia() ;
  auto _cart = getCartridge() ;

  // If no cycles should be simulated, make sure we stop immediately.
  reason.set(StoppingReason::numCyclesReached, maxNumCPUCycles == 0) ;

  // Loop until one of the stopping reasons is met.
  while (!reason.any())
  {
    // Step the CPU and decode the address it places on the bus.
    _cpu->cycle(_tia->RDY) ; maxNumCPUCycles-- ;
    auto da = DecodedAddress(_cpu->getAddressBus(), _cpu->getRW()) ;

    // Step the PIA.
    bool oututPortsChanged =
    _pia->cycle(da.device == DecodedAddress::PIA,
                _cpu->getAddressBus() & 0x200,
                _cpu->getRW(),
                _cpu->getAddressBus(),
                _cpu->getDataBus()) ;

    if (oututPortsChanged) {
      syncPorts() ;
    }

    // Step the TIA. Remember the current frame in order to detect
    // the beginning of a new one.
    auto lastFrame = _tia->numFrames ;
    _tia->cycle(da.device == DecodedAddress::TIA,
                _cpu->getRW(),
                _cpu->getAddressBus(),
                _cpu->getDataBus()) ;

    // Step the cartridge. The cartridge must be updated last
    // as some rare cart types (FE banking) "sniff"
    // the data bus to work, and the latter must be up to date.
    if (cartridge) {
      _cart->cycle(*this,da.device==DecodedAddress::Cartridge) ;
    }

    if (_tia->getVerbose() & false) {
      cout 
      << (da.Rw ? "R" : "W") 
      << setfill('0') << setw(4) << hex << (int)_cpu->getAddressBus() 
      << " (" << setfill(' ') << setw(8) << da << ") = "
      << setfill('0') << setw(2) << hex << (int)_cpu->getDataBus() << " "
      << M6502::decode(_cpu->getIR()) << " T" << _cpu->getT()
      << std::endl ;
    }

    // Check if the maximum number of CPU clocks have been simulated.
    reason.set(StoppingReason::numCyclesReached, maxNumCPUCycles==0) ;

    // Check if a new frame has started.
    if (_tia->numFrames > lastFrame) {
      reason.set(StoppingReason::frameDone) ;
    }

    // Check if a breakpoint was hit.
    if ((_cpu->getT() == 1) && _tia->RDY && breakOnNextInstruction) {
      // T=1 means that the CPU is executing the first cycle of a
      // new instruction. At this point, the CPU registers
      // are already updated with the *input* to that instruction,
      // including cpu.PCForCurrentInstruction().
      reason.set(StoppingReason::breakpoint) ;
      breakOnNextInstruction = false ;
    }

    if (_cpu->getT() == 0) {
      // T=0 means that the CPU has put on the address bus the
      // address of the next instruction opcode. Note, however,
      // that the *previous* instruction is still finishing during this
      // cycle, so cpu.PCForCurrentInstruction() is still the old one
      // and registers are still not updated with the new data.
      //
      // The breakpoint list is scanned to check for a hit after the *next* cycle
      // is executed.
      uint32_t virtualAddress = da.address ;
      if (da.device == DecodedAddress::Cartridge && _cart) {
        virtualAddress = _cart->decodeAddress(virtualAddress) ;
      }
      if (breakPoints.find(virtualAddress) != breakPoints.end()) {
        // Clear the breakpoint if temporary.
        clearBreakPoint(virtualAddress,true) ;
        breakOnNextInstruction |= true ;
      }
    }
  }
  return reason  ;
}

// -------------------------------------------------------------------
// MARK: - Manipulate state
// -------------------------------------------------------------------

void Atari2600::setCartridge(shared_ptr<Atari2600Cartridge> cartridge) {
  this->cartridge = cartridge ;
  reset() ;
}

/// Create an unitialized state object.
shared_ptr<jigo::Atari2600State>
Atari2600::makeState() const
{
  auto s = make_shared<Atari2600State>() ;
  s->cartridge = cartridge ? cartridge->makeAlike() : NULL ;
  return s ;
}

/// Make a *copy* of the system state.
shared_ptr<Atari2600State>
Atari2600::saveState() const
{
  auto s = make_shared<Atari2600State>() ;
  s->cpu = make_shared<M6502State>(*cpu) ;
  s->pia = make_shared<M6532State>(*pia) ;
  s->tia = make_shared<TIAState>(*tia) ;
  s->cartridge = cartridge ? cartridge->save() : NULL ;
  return s ;
}

/// Reset the system's state by copying the specified state.
Atari2600Error
Atari2600::loadState(const Atari2600State & s)
{
  if (!cartridge && s.cartridge) {
    return Atari2600Error::cartridgeTypeMismatch ;
  }
  if (cartridge) {
    auto error = getCartridge()->load(*s.cartridge) ;
    if (error != Atari2600Error::success) { return error ; }
  }
  *getCpu() = *s.cpu ;
  *getPia() = *s.pia ;
  *getTia() = *s.tia ;
  // To update dependents.
  setVideoStandard(getTia()->videoStandard) ;
  return Atari2600Error::success ;
}

/// Set the emulator verbosity level. The following levels are supported:
///
/// - 0: supporesses all messages.
/// - 1: shows basic simulator events, such as resetting the machine.
/// - 2: also show sthe CPU instructions as they are executed.
/// - 3: also shows the individual CPU cycles.
/// - 4: also shows the TIA, PIA, and cartidge cycles.

void Atari2600::setVerbosity(int verbosity)
{
  if (verbosity > 0) {
    getPia()->setVerbose(false) ;
    getTia()->setVerbose(true) ;
    getCpu()->setVerbose(false) ;
    if (getCartridge()) { getCartridge()->setVerbosity(0) ; }
  } else {
    getPia()->setVerbose(false) ;
    getTia()->setVerbose(false) ;
    getCpu()->setVerbose(false) ;
    if (cartridge) { getCartridge()->setVerbosity(0) ; }
  }
}

/// Get the system clock rate.
float Atari2600::getColorClockRate() const
{
  return clockRate ;
}

/// Get the nmber of clock cycles simulated so far.
/// Note that this is not the number of CPU clock cycles, as the CPU
/// runs at a third of the color clock rate.
long long Atari2600::getColorCycleNumber() const
{
  return tia->numCycles ;
}

/// Get the number of video frames generated so far.
long long Atari2600::getFrameNumber() const
{
  return tia->numFrames ;
}

void
Atari2600::setVideoStandard(VideoStandard type)
{
  getTia()->setVideoStandard(type) ;
  // Set the clock rate in Hz.
  switch (type) {
    case VideoStandard::NTSC: clockRate = TIA_NTSC_COLOR_CLOCK_RATE ; break ;
    default: clockRate = TIA_PAL_COLOR_CLOCK_RATE ; break ;
  }
}

Atari2600::VideoStandard
Atari2600::getVideoStandard() const
{
  return getTia()->getVideoStandard() ;
}

/// Set the state of the console panel switches.
void
Atari2600::setPanel(Panel panel)
{
  this->panel = panel ;
}

/// Get the state of the console panel switches.
Atari2600::Panel
Atari2600::getPanel() const
{
  return panel ;
}

void
Atari2600::setJoystick(int num, Joystick joystick)
{
  assert(num == 0 || num == 1) ;
  joysticks[num] = joystick ;
  inputType = InputType::joystick ;
}

template<typename T,size_t n> ostream& operator<<(ostream& os, array<T,n> const& x) {
  for (auto i : x) os << i << " " ;
  return os ;
}

void
Atari2600::setPaddle(int num, Paddle paddle)
{
  assert(0 <= num && num < 4) ;
  paddles[num] = paddle ;
  inputType = InputType::paddle ;
}

void
Atari2600::setKeyboard(int num, Atari2600::Keyboard keys)
{
  assert(0 <= num && num < 2) ;
  keyboards[num] = keys ;
  inputType = InputType::keyboard ;
}

void Atari2600::syncPorts()
{
  auto& tia = *getTia() ;
  auto& pia = *getPia() ;
  switch (inputType) {
    case InputType::joystick: {
      auto setj = [&](int num){return
        (1 << 3) * !joysticks[num][Joystick::right] +
        (1 << 2) * !joysticks[num][Joystick::left] +
        (1 << 1) * !joysticks[num][Joystick::down] +
        (1 << 0) * !joysticks[num][Joystick::up] ;
      } ;
      tia.ports.setI45({!joysticks[0][Joystick::fire],!joysticks[1][Joystick::fire]}) ;
      pia.setPortA((setj(0) << 4) | setj(1)) ;
      break ;
    }
    case InputType::paddle: {
      array<float,4> rates ;
      for (int num = 0 ; num < 4 ; ++num) {
        // It takes 380 scanlines for the paddle capacitor to fully
        // charge when the paddle is turned all the way clockwise.
        rates[num] = 270.0f / (380.0f * (paddles[num].angle + 135.1f)) ;
      }
      tia.ports.setI03(rates) ;
      pia.setPortA((1 << 7) * !paddles[0].fire +
                    (1 << 6) * !paddles[1].fire +
                    (1 << 3) * !paddles[2].fire +
                    (1 << 2) * !paddles[3].fire) ;
      break ;
    }
    case InputType::keyboard: {
      auto I03 = tia.ports.getI03() ;
      auto I45 = tia.ports.getI45() ;
      // num = 0: left
      for (int num = 0 ; num < 2 ; ++num) {
        std::uint8_t row = pia.getPortA() >> (4 - num * 4) ;
        I03[1+num*2] = 1.f ;
        I03[num*2] = +1.f ;
        I45[num] = true
        ;
        if (~row & 0x1) { // row pulls down
          if (keyboards[num][0]) I03[num*2] = -1.f ;
          if (keyboards[num][1]) I03[1+num*2] = -1.f ;
          if (keyboards[num][2]) I45[num] = false ;
        }
        if (~row & 0x2) {
          if (keyboards[num][3]) I03[num*2] = -1.f ;
          if (keyboards[num][4]) I03[1+num*2] = -1.f ;
          if (keyboards[num][5]) I45[num] = false ;
        }
        if (~row & 0x4) {
          if (keyboards[num][6]) I03[num*2] = -1.f ;
          if (keyboards[num][7]) I03[1+num*2] = -1.f ;
          if (keyboards[num][8]) I45[num] = false ;
        }
        if (~row & 0x8) {
          if (keyboards[num][9]) I03[num*2] = -1.f ;
          if (keyboards[num][10]) I03[1+num*2] = -1.f ;
          if (keyboards[num][11]) I45[num] = false ;
        }
      }
      tia.ports.setI03(I03) ;
      tia.ports.setI45(I45) ;
      // cout << tia.ports.getI03() << " " << tia.ports.getI45() << endl ;
      break ;
    }
    default:
      assert(false) ;
      break;
  }
  pia.setPortB((1 << 0) * !panel[Panel::reset] +
               (1 << 1) * !panel[Panel::select] +
               (1 << 3) * !panel[Panel::colorMode] +
               (1 << 6) * !panel[Panel::difficultyLeft] +
               (1 << 7) * !panel[Panel::difficultyRight]) ;
}

// -------------------------------------------------------------------
// MARK: - Init, reset, cleanup
// -------------------------------------------------------------------

Atari2600::Atari2600()
: Atari2600State(make_shared<M6502>(),
                 make_shared<M6532>(),
                 make_shared<jigo::TIA>(),
                 nullptr)
{
  setVideoStandard(VideoStandard::NTSC) ;
  panel.Panel::super::reset() ;
  panel.set(Panel::colorMode) ;
  reset() ;
}

Atari2600::~Atari2600()
{ }

void Atari2600::reset()
{
  if (getTia()->getVerbose() ) {
    cout << "--------------------------------------------------------" << endl ;
    cout << "Atari2600 Reset" << endl ;
    cout << "--------------------------------------------------------" << endl ;
  }
  // Input.
  for (auto& j : joysticks) { j = Joystick() ; }
  for (auto& p : paddles) { p = Paddle() ; }
  inputType = InputType::joystick;

  // Chipset.
  getPia()->reset() ;
  getTia()->reset() ;
  getCpu()->reset() ;

  // ROM.
  if (cartridge) {
    getCartridge()->reset() ;
  }
  syncPorts() ;
}

// -------------------------------------------------------------------
// MARK: - Debugger
// -------------------------------------------------------------------

uint32_t
Atari2600::virtualizeAddress(uint16_t address) const
{
  auto da = DecodedAddress(address, true) ;
  uint32_t virtualAddress = da.address ;
  if (da.device == DecodedAddress::Cartridge && cartridge) {
    virtualAddress = getCartridge()->decodeAddress(virtualAddress) ;
  }
  return virtualAddress ;
}

uint8_t const *
Atari2600::dataForVirtualAddress(std::uint32_t address) const
{
  uint8_t const * data = NULL ;
  auto da = DecodedAddress(address,false) ;
  if (da.device == DecodedAddress::PIA && da.piaRegister == M6532State::Register::RAM) {
    data = &pia->ram[0] + (address & 0x7f) ;
  } else if (da.device == DecodedAddress::Cartridge) {
    auto cart = getCartridge() ;
    if (cart) {
      auto ca = cart->decodeVirtualAddress(address) ;
      if (ca.valid) {
        auto region = cart->getRegion(ca.regionNumber) ;
        data = region.bytes + ca.regionOffset ;
      }
    }
  }
  return data ;
}

uint8_t *
Atari2600::mutableDataForVirtualAddress(std::uint32_t address)
{
  uint8_t const * data = dataForVirtualAddress(address) ;
  return (uint8_t*)(data) ;
}

map<uint32_t,Atari2600BreakPoint> const &
Atari2600::getBreakPoints()
{
  return breakPoints ;
}

void Atari2600::setBreakPoint(uint32_t address, bool temporary)
{
  if (breakPoints.find(address) == breakPoints.end()) {
    breakPoints[address] = Atari2600BreakPoint() ;
  }
  auto& bp = breakPoints[address] ;
  bp.address = address ;
  if (!temporary) {
    bp.persistent = true ;
  } else {
    bp.temporary = true ;
  }
}

void Atari2600::clearBreakPoint(uint32_t address, bool temporary)
{
  auto bpi = breakPoints.find(address) ;
  if (bpi != breakPoints.end()) {
    auto& bp = bpi->second ;
    if (!temporary) {
      bp.persistent = false ;
    } else {
      bp.temporary = false ;
    }
    if (!bp.persistent && !bp.temporary) {
      // There are no more breakpoints at this address.
      breakPoints.erase(bpi) ;
    }
  }
}

void Atari2600::setBreakPointOnNextInstruction() {
  breakOnNextInstruction = true ;
}

void Atari2600::clearBreakPointOnNextInstruction() {
  breakOnNextInstruction = false ;
}

std::ostream&
Atari2600::printInstruction(std::ostream & os, M6502::Instruction const &ins)
{
  switch (ins.addressingMode) {
    case M6502::absolute:
    case M6502::zeroPage: {
      auto da = Atari2600::DecodedAddress(ins.operand, ins.accessType == M6502::read) ;
      return os << ins.mnemonic << " " << da ;
    }
    default:
      return os << ins ;
  }
}


