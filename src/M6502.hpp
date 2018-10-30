// M6502.hpp
// M6502 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef M6502_hpp
#define M6502_hpp

#include "json.hpp"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace jigo {

/// The state of the M6502 CPU.
struct M6502State {
  // Lifecycle.
  virtual ~M6502State() = default;
  bool operator==(M6502State const&) const;

  // Manpiulate.
  bool getRW() const;
  std::uint16_t getAddressBus() const;
  void setAddressBus(std::uint16_t x);
  std::uint8_t getDataBus() const;
  std::uint8_t& getDataBus();
  void setDataBus(std::uint8_t data);
  bool getIRQLine() const;
  void setIRQLine(bool x);
  bool getNMILine() const;
  void setNMILine(bool x);
  bool getResetLine() const;
  void setResetLine(bool x);

  std::uint8_t getA() const;
  std::uint8_t getX() const;
  std::uint8_t getY() const;
  std::uint8_t getS() const;
  std::uint8_t getP(bool b = false) const;
  std::uint16_t getPC() const;
  std::uint16_t getPCP() const;
  std::uint16_t getPCIR() const;
  std::uint8_t getIR() const;
  std::uint16_t getAD() const;
  std::uint8_t getADD() const;
  int getT() const;
  int getTP() const;

  void setA(std::uint8_t A);
  void setX(std::uint8_t X);
  void setY(std::uint8_t Y);
  void setS(std::uint8_t S);
  void setP(std::uint8_t P);
  void setPC(std::uint16_t PC);
  void setPCP(std::uint16_t PCP);
  void setPCIR(std::uint16_t PCIR);
  void setIR(std::uint8_t IR);
  void setAD(std::uint16_t AD);
  void setADD(std::uint8_t ADD);
  void setT(int T);
  void setTP(int TP);

  size_t getNumCycles() const;
  void setNumCycles(size_t n);

protected:
  // Data lines.
  bool RW;                    /// Read-write flag.
  std::uint16_t addressBus{}; /// Adddress bus (16 bits).
  std::uint8_t dataBus{};     /// Data bus (8 bits).
  bool resetLine{true};       /// Reset line (on on reset, causes the reset vector to load).
  bool nmiLine{};             /// Non-maskable interupt line (off on reset).
  bool irqLine{};             /// Interrupt line (off on reset).

  // Programmable registers.
  std::uint8_t A{};     /// A register.
  std::uint8_t X{0xc0}; /// X register (initialized as visua6502).
  std::uint8_t Y{};     /// Y register.
  std::uint8_t S{};     /// S (stack pointer) register.

  struct PRegister : std::bitset<8> {
    enum { c = 0, z, i, d, b, v = 6, n };
    PRegister& operator=(std::uint8_t value);
    operator std::uint8_t() const;
  } P{}; /// P (status) register.
  std::uint16_t PC{
      (1 << PRegister::z) | // As visual6502.
      (1 << PRegister::i)   // IRQs are disabled on reset.
  };                        /// Program counter.

  // Internal CPU state.
  std::uint16_t PCIR{}; /// PC of the instruction in the IR.
  std::uint16_t PCP{};  /// Next value of PC.
  std::uint8_t IR{};    /// Instruction register.
  std::uint16_t AD{};   /// Address register.
  std::uint8_t ADD{};   /// Adder register.
  int T{};              /// Instruction micro-counter.
  int TP{1};            /// Next value of T.

  // Counters.
  size_t numCycles{}; /// Number of cycles simulated so far.

  friend void to_json(nlohmann::json& j, const M6502State& s);
  friend void from_json(const nlohmann::json& j, M6502State& state);
};

class M6502 : public M6502State {
public:
  /// Instruction menmonics.
  enum InstructionType {
    // Official instructions.
    // clang-format off
      ADC, AND, ASL, BCC, BCS,
      BEQ, BIT, BMI, BNE, BPL,
      BRK, BVC, BVS, CLC, CLD,
      CLI, CLV, CMP, CPX, CPY,
      DEC, DEX, DEY, EOR, INC,
      INX, INY, JMP, JSR, LDA,
      LDX, LDY, LSR, NOP, ORA,
      PHA, PHP, PLA, PLP, ROL,
      ROR, RTI, RTS, SBC, SEC,
      SED, SEI, STA, STX, STY,
      TAX, TAY, TSX, TXA, TXS,
      TYA,
      // Undocumented instructions.
      AHX, ALR, ANC, ARR, AXS,
      DCP, ISC, KIL, LAS, LAX,
      RLA, RRA, SAX, SHX, SHY,
      SLO, SRE, TAS, XAA, UNKNOWN
    // clang-format on
  };

  /// How an instruction accesses memory.
  enum AccessType { noAccess, read, write, readWrite, branch, stack, misc };

  /// How an instruction computes a memory address.
  enum AddressingMode {
    implied,
    immediate,
    absolute,
    absoluteIndexed,
    absoluteIndirect,
    zeroPage,
    zeroPageIndexed,
    zeroPageIndexedIndirect,
    zeroPageIndirectIndexed,
    pcRelative,
    push,
    pull
  };

  /// Which register an insturction uses for indexed addresing.
  enum IndexingType { noIndexing, XIndexing, YIndexing };

  /// Descriptor for a CPU instruction.
  struct InstructionTraits {
    std::uint8_t opcode;
    int length;
    const char* mnemonic;
    InstructionType instructionType;
    AddressingMode addressingMode;
    AccessType accessType;
    IndexingType indexingType;
    bool illegal;
    bool addToA;
  };

  /// An instruction with its operand value.
  struct Instruction : public InstructionTraits {
    std::uint16_t operand;
  };

  static InstructionTraits const& decode(std::uint8_t opcode);
  static Instruction decode(std::array<std::uint8_t, 3> const& bytes);

  // Lifecycle.
  M6502();
  M6502& operator=(M6502State const&);
  M6502& operator=(M6502 const&) = delete;
  virtual ~M6502() = default;

  // Operation.
  void reset();
  void cycle(bool busWasReady);
  bool getVerbose() const;
  void setVerbose(bool x);

private:
  // Transient.
  InstructionTraits dc; // Can be deduced from IR.
  bool verbose;

  // Helpers.
  std::uint8_t setNZ(std::uint8_t value);
  std::uint8_t xADC(std::uint8_t value);
  std::uint8_t xANC(std::uint8_t value);
  std::uint8_t xAND(std::uint8_t value);
  std::uint8_t xALR(std::uint8_t value);
  std::uint8_t xARR(std::uint8_t value);
  std::uint8_t xAXS(std::uint8_t value);
  void xCMP(std::uint8_t value);
  std::uint8_t xEOR(std::uint8_t value);
  std::uint8_t xORA(std::uint8_t value);
  std::uint8_t xSBC(std::uint8_t value);
  void xSTA(std::uint8_t value);
  void xBIT(std::uint8_t value);
  void xCPX(std::uint8_t value);
  void xCPY(std::uint8_t value);
  std::uint8_t xASL(std::uint8_t value);
  std::uint8_t xLSR(std::uint8_t value);
  std::uint8_t xROL(std::uint8_t value);
  std::uint8_t xROR(std::uint8_t value);
  void readFrom(std::uint16_t addr);
  void writeTo(std::uint16_t addr, std::uint8_t value);
};

// -------------------------------------------------------------------
// MARK: - Getters & setters
// -------------------------------------------------------------------

inline M6502State::PRegister& M6502State::PRegister::operator=(std::uint8_t value) {
  std::bitset<8>::operator=(value);
  (*this)[4] = false;
  (*this)[5] = false;
  return *this;
}

inline M6502State::PRegister::operator std::uint8_t() const {
  return static_cast<std::uint8_t>(to_ulong());
}

inline bool M6502State::getRW() const {
  return RW;
}

inline std::uint16_t M6502State::getAddressBus() const {
  return addressBus;
}

inline void M6502State::setAddressBus(std::uint16_t x) {
  addressBus = x;
}

inline std::uint8_t M6502State::getDataBus() const {
  return dataBus;
}

inline std::uint8_t& M6502State::getDataBus() {
  return dataBus;
}

inline void M6502State::setDataBus(std::uint8_t data) {
  dataBus = data;
}

inline std::uint8_t M6502State::getIR() const {
  return IR;
}

inline std::uint8_t M6502State::getS() const {
  return S;
}

inline std::uint16_t M6502State::getPC() const {
  return PC;
}

inline std::uint16_t M6502State::getPCP() const {
  return PCP;
}

inline std::uint16_t M6502State::getPCIR() const {
  return PCIR;
}

inline std::uint8_t M6502State::getA() const {
  return A;
}

inline std::uint8_t M6502State::getX() const {
  return X;
}

inline std::uint8_t M6502State::getY() const {
  return Y;
}

inline std::uint8_t M6502State::getP(bool b) const {
  return static_cast<uint8_t>(P) | (b << 4) | (1 << 5);
}

inline int M6502State::getT() const {
  return T;
}

inline std::uint16_t M6502State::getAD() const {
  return TP;
}

inline std::uint8_t M6502State::getADD() const {
  return T;
}

inline int M6502State::getTP() const {
  return TP;
}

inline bool M6502State::getNMILine() const {
  return nmiLine;
}

inline bool M6502State::getIRQLine() const {
  return irqLine;
}

inline bool M6502State::getResetLine() const {
  return resetLine;
}

inline void M6502State::setA(std::uint8_t A) {
  this->A = A;
}

inline void M6502State::setX(std::uint8_t X) {
  this->X = X;
}

inline void M6502State::setY(std::uint8_t Y) {
  this->Y = Y;
}

inline void M6502State::setS(std::uint8_t S) {
  this->S = S;
}

inline void M6502State::setPC(std::uint16_t PC) {
  PCP = this->PC = PC; // todo: review
}

inline void M6502State::setPCP(std::uint16_t PCP) {
  this->PCP = PCP;
}

inline void M6502State::setPCIR(std::uint16_t PCIR) {
  this->PCIR = PCIR;
}

inline void M6502State::setIR(std::uint8_t IR) {
  this->IR = IR;
}

inline void M6502State::setAD(std::uint16_t AD) {
  this->AD = AD;
}

inline void M6502State::setADD(std::uint8_t ADD) {
  this->ADD = ADD;
}

inline void M6502State::setT(int T) {
  TP = this->T = T;
}

inline void M6502State::setTP(int TP) {
  this->TP = TP;
}

inline void M6502State::setIRQLine(bool x) {
  irqLine = x;
}

inline void M6502State::setNMILine(bool x) {
  nmiLine = x;
}

inline void M6502State::setResetLine(bool x) {
  resetLine = x;
}

inline void M6502State::setP(std::uint8_t x) {
  P = x;
}

inline size_t M6502State::getNumCycles() const {
  return numCycles;
}

inline void M6502State::setNumCycles(size_t n) {
  numCycles = n;
}

inline bool M6502::getVerbose() const {
  return verbose;
}

inline void M6502::setVerbose(bool x) {
  verbose = x;
}

// -------------------------------------------------------------------
// MARK: - Input & Output
// -------------------------------------------------------------------

template <class T> struct asBytesWrapper {
  asBytesWrapper(T& object) : object(object) {}
  T& get() { return object; }
  T& object;
};

template <class T> asBytesWrapper<T const> asBytes(T const& object) {
  return asBytesWrapper<T const>(object);
}
} // namespace jigo

std::ostream& operator<<(std::ostream& os, jigo::M6502::InstructionTraits const& ins);
std::ostream& operator<<(std::ostream& os, jigo::M6502::Instruction const& ins);
std::ostream& operator<<(std::ostream& os, jigo::asBytesWrapper<jigo::M6502::InstructionTraits const> ins);
std::ostream& operator<<(std::ostream& os, jigo::asBytesWrapper<jigo::M6502::Instruction const> ins);

#endif /* M6502_hpp */
