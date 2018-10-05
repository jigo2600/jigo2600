// Atari2600.hpp
// Atari2600 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).
#ifndef Atari2600_hpp
#define Atari2600_hpp

#include "M6502.hpp"
#include "M6532.hpp"
#include "TIA.hpp"
#include "json.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace jigo {
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

    // Operate.
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
    // Operate.
    virtual void reset() = 0 ;
    virtual std::uint32_t cycle(Atari2600 &system, bool chipSelects) = 0 ;
    virtual void setVerbosity(int verbosity) = 0 ;
    virtual int getVerbosity() const = 0 ;

    // Lifecycle.
    virtual ~Atari2600Cartridge() = default ;

    // Inspect.
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
    // Lifecycle.
    Atari2600State() ;
    Atari2600State(std::shared_ptr<M6502State> cpu,
                   std::shared_ptr<M6532State> pia,
                   std::shared_ptr<jigo::TIAState> tia,
                   std::shared_ptr<Atari2600CartridgeState> cartridge = nullptr) ;
    virtual ~Atari2600State() = default ;

    // Insepct.
    bool operator== (Atari2600State const&) const ;

    // Data.
    std::shared_ptr<M6502State> cpu ;
    std::shared_ptr<M6532State> pia ;
    std::shared_ptr<TIAState> tia ;
    std::shared_ptr<Atari2600CartridgeState> cartridge ;
  } ;

  class Atari2600 : public Atari2600State
  {
  public:
    typedef jigo::TIAState::VideoStandard VideoStandard ;
    struct StoppingReason : std::bitset<32> {
      enum { frameDone = 0, breakpoint, numCyclesReached } ;
    } ;

    struct DecodedAddress
    {
      DecodedAddress(uint32_t address, bool RW) ;
      std::uint32_t address ; /// Virtual address.
      bool Rw ; /// Read-write flag.
      enum { Cartridge, TIA, PIA } device ;
      jigo::TIAState::Register tiaRegister ;
      M6532State::Register piaRegister ;
    } ;

    // Lifecycle.
    Atari2600() ;
    ~Atari2600() ;
    void reset() ;

    // Access the components.
    M6502 * getCpu() const { return static_cast<M6502*>(cpu.get()) ; }
    M6532 * getPia() const { return static_cast<M6532*>(pia.get()) ; }
    TIA * getTia() const { return static_cast<TIA*>(tia.get()) ; }

    // Manipulate the cartridge.
    void setCartridge(std::shared_ptr<Atari2600Cartridge> cartridge) ;
    Atari2600Cartridge * getCartridge() const { return dynamic_cast<Atari2600Cartridge*>(cartridge.get()) ; }

    // Manipulate the state.
    Atari2600Error loadState(const Atari2600State& state) ;
    std::shared_ptr<Atari2600State> saveState() const ;
    std::shared_ptr<Atari2600State> makeState() const ;
    
    // Debug.
    uint32_t virtualizeAddress(uint16_t address) const ;
    uint8_t const * dataForVirtualAddress(std::uint32_t virtualAddress) const ;
    uint8_t * mutableDataForVirtualAddress(std::uint32_t virtualAddress) ; // todo: think if we really want this

    std::map<std::uint32_t,Atari2600BreakPoint> const & getBreakPoints() ;
    void setBreakPoint(std::uint32_t virtualAddress, bool temporary = false) ;
    void clearBreakPoint(std::uint32_t virtualAddress, bool temporary = false) ;
    void setBreakPointOnNextInstruction() ;
    void clearBreakPointOnNextInstruction() ;
    static std::ostream& printInstruction(std::ostream &os, M6502::Instruction const &ins) ;

    // Run the simulation.
    StoppingReason cycle(size_t &maxNumCPUCycles) ;
    long long getColorCycleNumber() const ;
    long long getFrameNumber() const ;
    float getColorClockRate() const ;

    // Configure the machine.
    void setVideoStandard(VideoStandard standard) ;
    VideoStandard getVideoStandard() const ;

    // Panel and perpipherals.
    struct Panel : std::bitset<5> {
      typedef std::bitset<5> super ;
      using bitset::bitset; // inherit constructors
      enum { reset, select, colorMode, difficultyLeft, difficultyRight } ;
    } ;

    struct Joystick : std::bitset<5> {
      typedef std::bitset<5> super ;
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

    void setPanel(Panel panel) ;
    void setJoystick(int num, Joystick joystick) ;
    void setPaddle(int num, Paddle paddle) ;
    void setKeyboard(int num, Keyboard keys) ;
    void setVerbosity(int verbosity) ;

    Panel getPanel() const ;

  protected:
    // Panel and perpipherals.
    Panel panel ;
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

  void to_json(nlohmann::json& j, const jigo::Atari2600Cartridge::Type& type) ;
  void from_json(const nlohmann::json& j, jigo::Atari2600Cartridge::Type& type) ;
  void to_json(nlohmann::json& j, const jigo::Atari2600State& state) ;
  void from_json(const nlohmann::json& j, jigo::Atari2600State& state) ;
  void to_json(nlohmann::json& j, const jigo::Atari2600::VideoStandard& standard) ;
  void from_json(const nlohmann::json& j, jigo::Atari2600::VideoStandard& standard) ;
}

std::ostream& operator<< (std::ostream& os, jigo::Atari2600::DecodedAddress const& da) ;

#endif /* Atari2600_hpp */
