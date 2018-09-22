// Atari2600.hpp
// Atari2600

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef Atari2600_hpp
#define Atari2600_hpp

#include "M6502.hpp"
#include "M6532.hpp"
#include "TIA.hpp"
#include "json.hpp"

#include <cstdint>
#include <iostream>
#include <vector>
#include <array>
#include <map>

namespace sim {
  class Atari2600 ;
  enum class Atari2600Error : int { success, cartridgeTypeMismatch } ;

  // -----------------------------------------------------------------
  // MARK: - Cartridge
  // -----------------------------------------------------------------

  class Atari2600CartridgeState
  {
  public:
    enum class Type : int {
      unknown, standard,
      S2K, S4K, S8K, S12K, S16K, S32K,
      S2K128R, S4K128R, S8K128R, S12K128R, S16K128R, S32K128R,
      E0, FE, F0
    } ;

    // Operatrion.
    virtual Type getType() const = 0 ;
    virtual void reset() = 0 ;

    // Lifecycle.
    virtual void serialize(nlohmann::json& j) const = 0 ;
    virtual void deserialize(const nlohmann::json& j) = 0 ;
    virtual Atari2600Error load(Atari2600CartridgeState const&) = 0 ;
    virtual std::unique_ptr<Atari2600CartridgeState> save() const = 0 ;
    virtual std::unique_ptr<Atari2600CartridgeState> makeAlike() const = 0 ;
    virtual bool operator==(Atari2600CartridgeState const &) const = 0 ;
    virtual ~Atari2600CartridgeState() = default ;
  } ;

  class Atari2600Cartridge : public virtual Atari2600CartridgeState
  {
  public:
    // Operation.
    virtual void reset() = 0 ;
    virtual std::uint32_t cycle(Atari2600 &system, bool chipSelects) = 0 ;
    virtual void setVerbosity(int verbosity) = 0 ;
    virtual int getVerbosity() const = 0 ;

    // Lifecycle.
    virtual ~Atari2600Cartridge() = default ;

    // Inspection.
    struct Region {
      std::string name ;
      int number ;
      bool writable ;
      std::uint8_t const * bytes ;
      std::uint32_t numBytes ;
      std::uint32_t virtualAddress ;
    } ;
    struct ConcreteAddress {
      bool valid ;
      int regionNumber ;
      int regionOffset ;
    } ;
    virtual std::uint32_t decodeAddress(std::uint16_t pc) const = 0 ;
    virtual ConcreteAddress decodeVirtualAddress(std::uint32_t) const = 0 ;
    virtual std::uint32_t getSize() const = 0 ;
    virtual int getNumBanks() const = 0 ;
    virtual int getNumRegions() const = 0 ;
    virtual Region getRegion(int number) const = 0 ;
  } ;

  std::shared_ptr<Atari2600Cartridge>
  makeCartridgeFromBytes(const char* begin, const char* end,
                         Atari2600Cartridge::Type type = Atari2600Cartridge::Type::unknown) ;

  std::shared_ptr<Atari2600Cartridge>
  makeCartridgeFromBytes(const std::vector<char>& data,
                         Atari2600Cartridge::Type type = Atari2600Cartridge::Type::unknown) ;

  // -----------------------------------------------------------------
  // MARK: - System
  // -----------------------------------------------------------------

  struct Atari2600BreakPoint
  {
    std::uint16_t address ;
    bool persistent ;
    bool temporary ;
  } ;

  struct Atari2600State
  {
    Atari2600State() ;
    Atari2600State(std::shared_ptr<M6502State> cpu,
                   std::shared_ptr<M6532State> pia,
                   std::shared_ptr<sim::TIAState> tia,
                   std::shared_ptr<Atari2600CartridgeState> cartridge) ;
    bool operator== (Atari2600State const&) const ;
    virtual ~Atari2600State() = default ;

    std::shared_ptr<M6502State> cpu ;
    std::shared_ptr<M6532State> pia ;
    std::shared_ptr<sim::TIAState> tia ;
    std::shared_ptr<Atari2600CartridgeState> cartridge ;
  } ;

  class Atari2600 : public Atari2600State
  {
  public:
    typedef sim::TIAState::VideoStandard VideoStandard ;
    enum class Switch : int {reset, select, colorMode, difficultyLeft, difficultyRight} ;
    struct StoppingReason : std::bitset<32> {
      enum {frameDone=0,breakpoint,numClocksReached} ;
    } ;

    struct DecodedAddress
    {
      DecodedAddress(uint32_t address, bool RW) ;
      std::uint32_t address ; /// Virtual address.
      bool Rw ; /// Read-write flag.
      enum { Cartridge, TIA, PIA } device ;
      sim::TIAState::Register tiaRegister ;
      M6532State::Register piaRegister ;
    } ;

    // Lifecycle.
    Atari2600() ;
    ~Atari2600() ;
    Atari2600Error loadState(const Atari2600State& state) ;
    std::shared_ptr<Atari2600State> saveState() const ;
    std::shared_ptr<Atari2600State> makeState() const ;

    //
    void reset() ;
    void setCartridge(std::shared_ptr<Atari2600Cartridge> cartridge) ;
    M6502 * getCpu() const { return static_cast<M6502*>(cpu.get()) ; }
    M6532 * getPia() const { return static_cast<M6532*>(pia.get()) ; }
    TIA * getTia() const { return static_cast<TIA*>(tia.get()) ; }
    Atari2600Cartridge * getCartridge() const { return dynamic_cast<Atari2600Cartridge*>(cartridge.get()) ; }

    // Debugging.
    uint32_t virtualizeAddress(uint16_t address) const ;
    uint8_t const * dataForVirtualAddress(std::uint32_t virtualAddress) const ;
    uint8_t * mutableDataForVirtualAddress(std::uint32_t virtualAddress) ; // todo: think if we really want this

    std::map<std::uint32_t,Atari2600BreakPoint> const & getBreakPoints() ;
    void setBreakPoint(std::uint32_t virtualAddress, bool temporary = false) ;
    void clearBreakPoint(std::uint32_t virtualAddress, bool temporary = false) ;
    void setBreakPointOnNextInstruction() ;
    void clearBreakPointOnNextInstruction() ;
    static std::ostream& printInstruction(std::ostream &os, M6502::Instruction const &ins) ;

    // Running the simulation.
    StoppingReason cycle(size_t &maxNumCPUCycles) ;
    long long getColorCycleNumber() const ;
    long long getFrameNumber() const ;
    float getColorClockRate() const ;
    std::uint32_t * getCurrentScreen() const ;
    std::uint32_t * getLastScreen() const ;

    // Configuring the machine.
    void setVideoStandard(VideoStandard standard) ;
    VideoStandard getVideoStandard() const ;

    // I/O devices.
    struct Joystick : std::bitset<5> {
      using bitset::bitset; // inherit constructors
      enum { fire, up, down, left, right } ;
    } ;

    struct Paddle {
      Paddle() = default ;
      Paddle(bool fire, float angle)
      : fire(fire), angle(angle) { }
      bool fire ;
      float angle ;
    } ;

    using Keyboard = std::bitset<12> ;

    void setJoystick(int num, Joystick joystick) ;
    void setPaddle(int num, Paddle paddle) ;
    void setKeyboard(int num, Keyboard keys) ;
    void setVerbosity(int verbosity) ;
    void setSwitch(Switch id, bool state) ;
    bool getSwitch(Switch id) const ;

    static int constexpr screenHeight = 228 + 8 + 10 ; //192 + 8 + 10 ;
    static int constexpr screenWidth = 160 ;
    static float constexpr pixelAspectRatio = 1.8f ;

  protected:
    friend class Atari2600Cartridge ;
    friend class Atari2600CartridgeStandard ;
    friend class Atari2600CartridgeE0 ;
    friend class Atari2600CartridgeFE ;
    friend class Atari2600CartridgeF0 ;

    // Ports and switches.
    bool switchReset;
    bool switchSelect;
    bool switchColorMode;
    bool switchDifficultyLeft;
    bool switchDifficultyRight;

    enum class InputType { joystick, paddle, keyboard } inputType ;
    std::array<Joystick,2> joysticks ;
    std::array<Paddle,4> paddles ;
    std::array<Keyboard,2> keyboards ;

    void syncPorts() ;

    // Transient.
    float clockRate ;
    std::map<std::uint32_t,Atari2600BreakPoint> breakPoints ;
    bool breakOnNextInstruction ;
  } ;

  void to_json(nlohmann::json& j, const sim::Atari2600Cartridge::Type& type) ;
  void from_json(const nlohmann::json& j, sim::Atari2600Cartridge::Type& type) ;
  void to_json(nlohmann::json& j, const sim::Atari2600State& state) ;
  void from_json(const nlohmann::json& j, sim::Atari2600State& state) ;
  void to_json(nlohmann::json& j, const sim::Atari2600::VideoStandard& standard) ;
  void from_json(const nlohmann::json& j, sim::Atari2600::VideoStandard& standard) ;
}

std::ostream& operator<< (std::ostream& os, sim::Atari2600::DecodedAddress const& da) ;

#endif /* Atari2600_hpp */
