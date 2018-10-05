// Atari2600Cartridge.cpp
// Atari2600 cartridges

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "Atari2600.hpp"
#include <array>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <cstring>

using json = nlohmann::json ;
using namespace std ;
using namespace jigo ;

constexpr size_t operator"" _KiB(unsigned long long n) { return 1024 * n ; }

#undef jput
#undef jget
#define jput(x) j[# x] = this->x
#define jget(x) this->x = j.at(# x)

// -------------------------------------------------------------------
// MARK: - Serlialize & deserialize state
// -------------------------------------------------------------------

void to_json(nlohmann::json& j, const Atari2600Cartridge::Type& p)
{
  switch (p) {
    case Atari2600Cartridge::Type::unknown: j = json(); break  ;// null
    case Atari2600Cartridge::Type::standard: j = "standard" ; break ;
    case Atari2600Cartridge::Type::E0: j = "E0" ; break ;
    case Atari2600Cartridge::Type::F0: j = "F0" ; break ;
    case Atari2600Cartridge::Type::FE: j = "FE" ; break ;
    default: assert(false) ;
  }
}

void from_json(const nlohmann::json& j, Atari2600Cartridge::Type& p)
{
  if (j.is_null()) {
    p = Atari2600Cartridge::Type::unknown ;
  }
  else {
    string str = j.get<string>() ;
    if (str == "standard") { p = Atari2600Cartridge::Type::standard ; }
    else if (str == "E0") { p = Atari2600Cartridge::Type::E0 ; }
    else if (str == "F0") { p = Atari2600Cartridge::Type::F0 ; }
    else if (str == "FE") { p = Atari2600Cartridge::Type::FE ; }
    else { throw std::invalid_argument
      (std::string("Unknown cartridge format specifier " + str)) ; }
  }
}

// -------------------------------------------------------------------
// MARK: - Helper templates
// -------------------------------------------------------------------

// MARK: StateHelper

template <class State, Atari2600CartridgeState::Type type, size_t ramSize = 0>
class StateHelper :
public virtual Atari2600CartridgeState
{
public:
  Type getType() const override {
    return type ;
  }

  bool operator==(StateHelper<State,type,ramSize> const& s) const {
    return true ;
  }

  bool operator==(Atari2600CartridgeState const &s) const override {
    try {
      auto x = dynamic_cast<State const&>(s) ;
      return *static_cast<State const*>(this)==(x) ;
    } catch (bad_cast& bc) {
      return false ;
    }
  }

  void reset() override { }
  void serialize(nlohmann::json& j) const override { }
  void deserialize(const nlohmann::json& j) override { }

  Atari2600Error load(Atari2600CartridgeState const& state) override {
    try {
      auto x = dynamic_cast<State const&>(state) ;
      static_cast<State*>(this)->State::operator= (x) ;
    } catch (bad_cast& bc) {
      return Atari2600Error::cartridgeTypeMismatch ;
    }
    return Atari2600Error::success ;
  }

  unique_ptr<Atari2600CartridgeState> save() const override {
#if __cplusplus <= 201103L
    return move(unique_ptr<State>(new State(self()))) ;
#else
    return make_unique<State>(self()) ;
#endif
  }

  unique_ptr<Atari2600CartridgeState> makeAlike() const override {
#if __cplusplus <= 201103L
    return move(unique_ptr<State>(new State())) ;
#else
    return make_unique<State>() ;
#endif
  }

private:
  State& self() { return *static_cast<State*>(this); }
  const State& self() const { return *static_cast<const State*>(this); }
} ;

// MARK: CartridgeHelper

template <class Cartridge, class State, int romSize>
class CartridgeHelper :
public virtual Atari2600Cartridge,
public State
{
public:

  void reset() override {
    this->State::reset() ;
  }

  void setVerbosity(int verbosity) override {
    this->verbosity = verbosity ;
  }

  int getVerbosity() const override {
    return verbosity ;
  }

  uint32_t getSize() const override {
    return romSize ;
  }

  uint32_t decodeAddress(uint16_t pc) const override {
    return (pc & 0x1000) ? (0xf000 | pc) : 0 ;
  }

  ConcreteAddress decodeVirtualAddress(uint32_t address) const override {
    ConcreteAddress ca ;
    ca.regionNumber = address >> 16 ;
    ca.regionOffset = address & 0xfff ;
    ca.valid = (address & 0x1000) && (ca.regionNumber < getNumBanks()) && (ca.regionOffset < romSize) ;
    return ca ;
  }

  int getNumBanks() const override {
    return 1 ;
  }

  int getNumRegions() const override {
    return 1 ;
  }

  Region getRegion(int number) const override {
    assert(number < getNumBanks()) ;
    Region b ;
    b.number = number ;
    b.name = "Bank " + to_string(number) ;
    b.writable = false ;
    b.bytes = &rom[number * 4_KiB] ;
    b.numBytes = min((size_t)romSize, 4_KiB) ;
    b.virtualAddress = 0xf000 + (number << 16) ;
    return b ;
  }

  void loadFromBytes(const std::vector<char>& data) {
    loadFromBytes(begin(data), end(data)) ;
  }

  void loadFromBytes(const char * begin, const char * end) {
    assert(end >= begin) ;
    memset(&rom[0], 0, romSize) ;
    memcpy(&rom[0], begin, min((size_t)romSize, (size_t)(end - begin))) ;
  }

protected:
  // Transient state.
  bool verbosity ;
  array<uint8_t,romSize> rom ;

private:
  Cartridge& self() { return *static_cast<Cartridge*>(this); }
  const Cartridge& self() const { return *static_cast<const Cartridge*>(this); }
} ;

// -------------------------------------------------------------------
// MARK: - Standard cartridges
// -------------------------------------------------------------------

using Type = Atari2600CartridgeState::Type ;

template <size_t sz> struct strobe ;
template<> struct strobe<2_KiB>  { static constexpr int minBankStrobe = 0 ; } ;
template<> struct strobe<4_KiB>  { static constexpr int minBankStrobe = 0 ; } ;
template<> struct strobe<8_KiB>  { static constexpr int minBankStrobe = 0xff8 ; } ;
template<> struct strobe<12_KiB> { static constexpr int minBankStrobe = 0xff8 ; } ;
template<> struct strobe<16_KiB> { static constexpr int minBankStrobe = 0xff6 ; } ;
template<> struct strobe<32_KiB> { static constexpr int minBankStrobe = 0xff4 ; } ;

template <Type ty, int rom, int ram> struct traits_helper : public strobe<rom> {
  static constexpr Type type = ty ;
  static constexpr int romSize = rom ;
  static constexpr int ramSize = ram ;
  static constexpr int numBanks = (romSize / 4_KiB >  1) ? (romSize / 4_KiB) : 1 ;
} ;

template <Type type> struct traits ;
template<> struct traits<Type::S2K>  : traits_helper<Type::S2K, 2_KiB,0>  { } ;
template<> struct traits<Type::S4K>  : traits_helper<Type::S4K, 4_KiB,0>  { } ;
template<> struct traits<Type::S8K>  : traits_helper<Type::S8K, 8_KiB,0>  { } ;
template<> struct traits<Type::S12K> : traits_helper<Type::S12K,12_KiB,0> { } ;
template<> struct traits<Type::S16K> : traits_helper<Type::S16K,16_KiB,0> { } ;
template<> struct traits<Type::S32K> : traits_helper<Type::S32K,32_KiB,0> { } ;
template<> struct traits<Type::S2K128R>  : traits_helper<Type::S2K128R, 2_KiB,128>  { } ;
template<> struct traits<Type::S4K128R>  : traits_helper<Type::S4K128R, 4_KiB,128>  { } ;
template<> struct traits<Type::S8K128R>  : traits_helper<Type::S8K128R, 8_KiB,128>  { } ;
template<> struct traits<Type::S12K128R> : traits_helper<Type::S12K128R,12_KiB,128> { } ;
template<> struct traits<Type::S16K128R> : traits_helper<Type::S16K128R,16_KiB,128> { } ;
template<> struct traits<Type::S32K128R> : traits_helper<Type::S32K128R,32_KiB,128> { } ;

// MARK: Standard cartridge state

template <class State, Atari2600CartridgeState::Type type>
class StandardState :
public StateHelper<State,type>,
public traits<type>
{
public:
  using traits<type>::ramSize ;

  StandardState()
  : activeBank(0) { }

  bool operator==(StandardState<State,type> const& s) const {
    return super::operator==(s) && (ram == s.ram) && (activeBank == s.activeBank) ;
  }

  void reset() override {
    this->super::reset() ;
    activeBank = 0 ;
    if (ramSize) { fill(begin(ram),end(ram),0) ; }
  }

  void serialize(nlohmann::json& j) const override {
    this->super::serialize(j) ;
    jput(activeBank) ;
    if (ramSize) { jput(ram) ; }
  }

  void deserialize(const nlohmann::json& j) override {
    this->super::deserialize(j) ;
    jget(activeBank) ;
    if (ramSize) { jget(ram) ; }
  }

protected:
  int activeBank ;
  array<uint8_t,ramSize> ram ;

private:
  using super = StateHelper<State,type> ;
} ;

// MARK: Standard cartridge

template <class Cartridge, class State>
class Standard :
public CartridgeHelper<Cartridge,State,State::romSize>
{
private:
  using super = CartridgeHelper<Cartridge,State,State::romSize> ;

public:
  using Region = typename super::Region ;
  using super::romSize ;
  using super::ramSize ;
  using super::numBanks ;
  using super::activeBank ;
  using super::minBankStrobe ;
  using super::rom ;
  using super::ram ;

  uint32_t cycle(Atari2600 & machine, bool chipSelect) override
  {
    // Nothing to do if not chip select.
    if (!chipSelect) { return 0 ; }
    uint32_t naddress = machine.getCpu()->getAddressBus() & ((romSize == 2_KiB) ? 0x07ff :0x0fff) ;

    // RAM operation.
    if (naddress < 2 * ramSize) {
      if (naddress < ramSize) {
        // Write operation.
        ram[naddress] = machine.getCpu()->getDataBus() ;
      } else {
        // Read operation.
        machine.getCpu()->setDataBus(ram[naddress - ramSize]) ;
      }
      return naddress ;
    }

    // Bank switching operation.
    if (!((naddress ^ 0xfe0) & 0xfe0)) {
      int bank = (int)naddress - minBankStrobe ;
      if (0 <= bank && bank < numBanks) {
        activeBank = bank ;
        return naddress ;
      }
    }

    // Regular ROM operation.
    naddress = naddress + 4_KiB * activeBank ;
    if (machine.getCpu()->getRW()) {
      auto value = rom[naddress] ;
      machine.getCpu()->setDataBus(value) ;
    } else {
      // Pass.
    }
    return naddress ;
  }

  uint32_t decodeAddress(uint16_t pc) const override {
    if (!(pc & 0x1000)) { return 0 ; }
    uint32_t naddress = pc & (romSize == 2_KiB ? 0x07ff : 0x0fff) ;
    if (naddress < 2 * ramSize) {
      return ((numBanks + 1) << 16) | 0xf000 | (naddress & 0x7f) ;
    }
    return (activeBank << 16) | 0xf000 | naddress ;
  }

  Region getRegion(int number) const override {
    assert(number < getNumRegions()) ;
    if ((ramSize > 0) && number == getNumRegions() - 1) {
      Region r ;
      r.number = number ;
      r.name = "Bank RW" ;
      r.writable = true ;
      r.bytes = &this->ram[0] ;
      r.numBytes = this->ramSize ;
      r.virtualAddress = 0xf000 ;
      return r ;
    } else {
      return super::getRegion(number) ;
    }
  }

  int getNumBanks() const override {
    return numBanks ;
  }

  int getNumRegions() const override {
    return numBanks + (ramSize > 0) ;
  }
} ;

// Instantiate standard cartridge classes.
#define make(x) \
class Atari2600Cartridge ## x ## State : public StandardState<Atari2600Cartridge ## x ## State, Type:: x> { \
public:\
Atari2600Cartridge ## x ## State & operator= (Atari2600Cartridge ## x ## State const&) = default ; \
}; \
class Atari2600Cartridge ## x : public Standard<Atari2600Cartridge ## x, Atari2600Cartridge ## x ## State> { \
public:\
Atari2600Cartridge ## x& operator= (Atari2600Cartridge ## x const&) = default ; \
};

make(S2K);
make(S4K);
make(S8K);
make(S12K);
make(S16K);
make(S32K);

make(S2K128R);
make(S4K128R);
make(S8K128R);
make(S12K128R);
make(S16K128R);
make(S32K128R);

// -------------------------------------------------------------------
// MARK: - F0 cartridge
// -------------------------------------------------------------------

template<> struct traits<Type::F0> {
  static constexpr Type type = Type::F0 ;
  static constexpr int numBanks = 16 ;
  static constexpr int minBankStrobe = 0xff0 ;
  static constexpr int romSize = numBanks * 4_KiB ;
  static constexpr int ramSize = 0 ;
} ;

struct Atari2600CartridgeF0State :
public StandardState<Atari2600CartridgeF0State, Type::F0> {} ;

struct Atari2600CartridgeF0 :
public Standard<Atari2600Cartridge, Atari2600CartridgeF0State>
{
  uint32_t cycle(Atari2600 & machine, bool chipSelect) override {
    // Nothing to do if not chip select.
    if (!chipSelect) { return 0 ; }
    std::uint16_t naddress = machine.getCpu()->getAddressBus() & 0x0fff ;

    // Bank switching operation.
    if (naddress == 0xff0) {
      this->activeBank = (this->activeBank + 1) & 0xf ;
      return naddress ;
    }
    else if (naddress == 0x1fec) {
      if (machine.getCpu()->getRW()) {
        machine.getCpu()->setDataBus(this->activeBank) ;
      }
      return naddress ;
    }

    // Regular ROM operation.
    naddress += 4_KiB * this->activeBank ;
    if (machine.getCpu()->getRW()) {
      auto value = this->rom[naddress] ;
      machine.getCpu()->setDataBus(value) ;
    } else {
      // Pass.
    }
    return naddress ;
  }
} ;

// -------------------------------------------------------------------
// MARK: - E0 cartridge
// -------------------------------------------------------------------

template<> struct traits<Type::E0> {
  static constexpr Type type = Type::E0 ;
  static constexpr int numBanks = 8 ;
  static constexpr int minBankStrobe = 0xfe0 ;
  static constexpr int romSize = numBanks * 4_KiB ;
  static constexpr int ramSize = 0 ;
} ;

struct Atari2600CartridgeE0State :
public StateHelper<Atari2600CartridgeE0State,Type::E0,0>,
public traits<Type::E0>
{
public:
  Atari2600CartridgeE0State()
  : activeBanks {0,0,0} { }

  bool operator==(Atari2600CartridgeE0State const& s) const {
    return super::operator==(s) && (activeBanks == s.activeBanks) ;
  }

  void serialize(nlohmann::json& j) const override {
    this->super::serialize(j) ;
    jput(activeBanks) ;
  }

  void deserialize(const nlohmann::json& j) override {
    this->super::deserialize(j) ;
    jget(activeBanks) ;
  }

  Atari2600CartridgeE0State& operator= (Atari2600CartridgeE0State const&) = default ;

protected:
  std::array<int,3> activeBanks ;

private:
  using super = StateHelper<Atari2600CartridgeE0State,Type::E0,0> ;
} ;

class Atari2600CartridgeE0 :
public CartridgeHelper<Atari2600CartridgeE0,Atari2600CartridgeE0State,Atari2600CartridgeE0State::romSize>
{
public:
  void reset() override {
    this->super::reset() ;
    fill(begin(this->activeBanks),end(this->activeBanks),0) ;
  }

  uint32_t cycle(Atari2600 &machine, bool chipSelect) override {
    // Nothing to do if not chip select.
    if (!chipSelect) { return 0 ; }
    uint32_t naddress = machine.getCpu()->getAddressBus() & 0x0fff ;

    // Bank switching operation.
    if (!((naddress ^ 0xfe0) & 0xfe0)) {
      int bank = (int)naddress - minBankStrobe ;
      if (bank >= 0) {
        if (bank < 8) {
          this->activeBanks[0] = bank ;
          return naddress ;
        }
        else if (bank < 16) {
          this->activeBanks[1] = bank - 8 ;
          return naddress ;
        }
        else if (bank < 24) {
          this->activeBanks[2] = bank - 16 ;
          return naddress ;
        }
      }
    }

    // Regular ROM operation.
    int slice = naddress >> 10 ;
    int offset = naddress & 0x3ff ;
    if (slice == 3) {
      naddress = 7_KiB + offset ;
    } else {
      naddress = 1_KiB * this->activeBanks[slice] + offset ;
    }
    if (machine.getCpu()->getRW()) {
      auto value = rom[naddress] ;
      machine.getCpu()->setDataBus(value) ;
    } else {
      // Pass.
    }
    return naddress ;
  }

  // Debugging.
  uint32_t decodeAddress(uint16_t pc) const override {
    if ((pc & 0x1000) == 0) return 0 ; // Not a ROM address.
    uint32_t naddress = pc & 0x0fff ;
    auto slice = naddress >> 10 ;
    auto offset = naddress & 0x3ff ;
    if (slice == 3) {
      naddress = (7 << 16) | offset ;
    } else {
      naddress = (this->activeBanks[slice] << 16) | offset ;
    }
    return naddress | 0xf000 ;
  }

  ConcreteAddress decodeVirtualAddress(uint32_t address) const override {
    ConcreteAddress ca ;
    uint32_t naddress = address & 0xffff1fff ;
    ca.regionNumber = naddress >> 16 ;
    ca.regionOffset = naddress & 0x3ff ;
    ca.valid = (address & 0x1000) && (ca.regionNumber < getNumRegions()) ;
    return ca ;
  }

  int getNumBanks() const override {
    return this->numBanks ;
  }

  Region getRegion(int number) const override {
    // Unusual 1024 banks.
    assert(number < getNumBanks()) ;
    Region b ;
    b.number = number ;
    b.name = "Bank " + to_string(number) ;
    b.bytes = &this->rom[number * 1_KiB] ;
    b.numBytes = 1_KiB ;
    b.virtualAddress = 0xf000 + (number << 16) ;
    return b ;
  }

private:
  using super = CartridgeHelper<Atari2600CartridgeE0,Atari2600CartridgeE0State,Atari2600CartridgeE0State::romSize> ;
} ;

// -------------------------------------------------------------------
// MARK: - FE cartridge
// -------------------------------------------------------------------

template<> struct traits<Type::FE> {
  static constexpr Type type = Type::FE ;
  static constexpr int numBanks = 2 ;
  static constexpr int minBankStrobe = 0 ;
  static constexpr int romSize = numBanks * 4_KiB ;
  static constexpr int ramSize = 0 ;
} ;

struct Atari2600CartridgeFEState :
public StateHelper<Atari2600CartridgeFEState,Type::FE,0>,
public traits<Type::FE>
{ 
public:
  Atari2600CartridgeFEState()
  : activeBank {0}, feDetected {false} { }

  bool operator==(Atari2600CartridgeFEState const& s) const {
    return super::operator==(s) && (activeBank == s.activeBank) && (feDetected == s.feDetected);
  }

  void serialize(nlohmann::json& j) const override {
    this->super::serialize(j) ;
    jput(activeBank) ;
    jput(feDetected) ;
  }

  void deserialize(const nlohmann::json& j) override {
    this->super::deserialize(j) ;
    jget(activeBank) ;
    jget(feDetected) ;
  }

  Atari2600CartridgeFEState& operator= (Atari2600CartridgeFEState const&) = default ;

protected:
  int activeBank ;
  bool feDetected ;

private:
  using super = StateHelper<Atari2600CartridgeFEState,Type::FE,0> ;
} ;

class Atari2600CartridgeFE :
public CartridgeHelper<Atari2600CartridgeFE,Atari2600CartridgeFEState,Atari2600CartridgeE0State::romSize>
{
public:
  uint32_t cycle(Atari2600 & machine, bool chipSelect) override {
    uint16_t address = machine.getCpu()->getAddressBus() ;
    uint16_t naddress = 0 ;
    if (chipSelect) {
      naddress = (address & 0x0fff) + activeBank * 4_KiB ;
      if (machine.getCpu()->getRW()) {
        auto value = rom[naddress] ;
        machine.getCpu()->setDataBus(value) ;
      } else {
        // Pass.
      }
    }

    if (feDetected) {
      activeBank = (machine.getCpu()->getDataBus() & 0x20) ? 0 : 1 ;
    }

    feDetected = ((address & 0xfff) == 0x1fe) && !chipSelect ;

    return naddress ;
  }

  int getNumBanks() const  override {
    return 2 ;
  }

  uint32_t decodeAddress(uint16_t pc) const override {
    if ((pc & 0x1000)==0) return 0 ; // Not a ROM address.
    return (pc & 0x0fff) | (activeBank == 1 ? 0x1f000 : 0xf000) ;
  }
} ;

// -------------------------------------------------------------------
// MARK: - Utililty functions
// -------------------------------------------------------------------

template<typename T>
bool is_member(const vector<T>& v, const T& x)
{
  return find(v.begin(),v.end(),x) != v.end() ;
}

template<class C> unique_ptr<C> mk(const char* begin, const char* end) {
#if __cplusplus <= 201103L
  auto x = unique_ptr<C>(new C()) ;
#else
  auto x = make_unique<C>() ;
#endif
  x->loadFromBytes(begin, end) ;
  return move(x) ;
}

shared_ptr<Atari2600Cartridge>
jigo::makeCartridgeFromBytes(const char* begin, const char* end,
                            Atari2600Cartridge::Type type)
{
  using namespace jigo ;
  ptrdiff_t size = end - begin ;

  // Try to identify standard cartridges.
  if (type == Type::unknown || type == Type::standard) {
    // Detect RAM expansion.
    auto ramSize = 0 ;
    for (uint8_t pattern : {0x00, 0xff}) {
      auto fillerSize = find_if
      (begin, min(end, begin + 512),
       bind(not_equal_to<std::uint8_t>(), pattern, placeholders::_1)) - begin ;
      if (fillerSize >= 512) {
        ramSize = 256 ;
      } else if (fillerSize >= 256) {
        ramSize = 128 ;
      }
    }

    switch (ramSize) {
      case 0:
        switch (size) {
          case 2_KiB:  type = Type::S2K ; break ;
          case 4_KiB:  type = Type::S4K ; break ;
          case 8_KiB:  type = Type::S8K ; break ;
          case 12_KiB: type = Type::S12K ; break ;
          case 16_KiB: type = Type::S16K ; break ;
          case 32_KiB: type = Type::S32K ; break ;
        }
        break ;
      case 128:
        switch (size) {
          case 2_KiB:  type = Type::S2K128R ; break ;
          case 4_KiB:  type = Type::S4K128R ; break ;
          case 8_KiB:  type = Type::S8K128R ; break ;
          case 12_KiB: type = Type::S12K128R ; break ;
          case 16_KiB: type = Type::S16K128R ; break ;
          case 32_KiB: type = Type::S32K128R ; break ;
        }
        break ;
      default:
        break ;
    }
  }

  if (type == Type::unknown || type == Type::standard) {
    // Could not identify. Try a standard choice.
    type = Type::S4K ;
  }

  // Create cartridge of the required type.
#define m(x) case Type::x: cart = mk<Atari2600Cartridge ## x>(begin, end); break;
  unique_ptr<Atari2600Cartridge> cart ;
  switch (type) {
      m(S2K) ;
      m(S4K) ;
      m(S8K) ;
      m(S12K) ;
      m(S16K) ;
      m(S32K) ;
      m(S2K128R) ;
      m(S4K128R) ;
      m(S8K128R) ;
      m(S12K128R) ;
      m(S16K128R) ;
      m(S32K128R) ;
      m(F0) ;
      m(E0) ;
      m(FE) ;
    default: assert(false) ;
  }
  return move(cart) ;
}

shared_ptr<Atari2600Cartridge>
jigo::makeCartridgeFromBytes(const std::vector<char>& data,
                            Atari2600Cartridge::Type type)
{
  return jigo::makeCartridgeFromBytes(&*begin(data), &*end(data), type) ;
}


