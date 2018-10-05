// M6502.hpp
// M6502 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef M6502_hpp
#define M6502_hpp

#include <cstddef>
#include <cstdint>
#include <bitset>
#include <iostream>
#include "json.hpp"

namespace jigo {

  /// The state of the M6502 CPU.
  struct M6502State
  {
    // Lifecycle.
    virtual ~M6502State() = default ;
    bool operator==(M6502State const&) const ;

    // Data lines.
    bool RW ; /// Read-write flag.
    std::uint16_t addressBus ;
    std::uint8_t dataBus ;
    bool resetLine ;
    bool nmiLine ;
    bool irqLine ;

    // Programmable registers.
    std::uint8_t A ; /// A register.
    std::uint8_t X ; /// X register.
    std::uint8_t Y ; /// Y register.
    std::uint8_t S ; /// S register.
    struct PRegister : std::bitset<8> {
      enum {c=0,z,i,d,b,v=6,n} ;
      PRegister& operator= (std::uint8_t value) ;
      operator std::uint8_t() const ;
    } P ; /// P register.
    std::uint16_t PC ; /// Program counter.

    // Internal CPU state.
    std::uint16_t PCCurrent ; /// PC of the instruction in IR.
    std::uint16_t PCP ; /// Next value of PC.
    std::uint8_t IR ; /// Instruction register.
    std::uint16_t AD ; /// Address register.
    std::uint8_t ADD ; /// Adder register.
    int T ; /// Instruction micro-counter.
    int TP ; /// Next value of T.

    // Counters.
    size_t numCycles ; /// Number of cycles simulated so far.
  } ;

  class M6502 : public M6502State
  {
  public:
    /// Instruction menmonics.
    enum InstructionType
    {
      // Official instructions.
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
      // Unodcumented instructions.
      AHX, ALR, ANC, ARR, AXS,
      DCP, ISC, KIL, LAS, LAX,
      RLA, RRA, SAX, SHX, SHY,
      SLO, SRE, TAS, XAA, UNKNOWN
    } ;

    /// How an instruction accesses memory.
    enum AccessType
    {
      noAccess,
      read,
      write,
      readWrite,
      branch,
      stack,
      misc
    } ;

    /// How an instruction computes a memory address.
    enum AddressingMode
    {
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
    } ;

    /// Which register an insturction uses for indexed addresing.
    enum IndexingType
    {
      noIndexing,
      XIndexing,
      YIndexing
    } ;

    /// An instruction's details.
    struct InstructionTraits
    {
      std::uint8_t opcode ;
      int length ;
      const char * mnemonic ;
      InstructionType instructionType ;
      AddressingMode addressingMode ;
      AccessType accessType ;
      IndexingType indexingType ;
      bool illegal ;
      bool addToA ;
    } ;

    /// An instruction with its operand value.
    struct Instruction : public InstructionTraits
    {
      std::uint16_t operand ;
    } ;

    static InstructionTraits const& decode(std::uint8_t opcode) ;
    static Instruction decode(std::array<std::uint8_t,3> const& bytes) ;

    // Lifecycle.
    M6502() ;
    M6502& operator= (M6502State const&) ;
    M6502& operator= (M6502 const&) = delete ;
    virtual ~M6502() = default ;

    // Run the simulation.
    void reset() ;
    void cycle(bool busWasReady) ;
    size_t getNumCycles() const ;
    bool getVerbose() const ;
    void setVerbose(bool x) ;

    // Manpiulate the data lines.
    bool getRW() const ;
    std::uint16_t getAddressBus() const ;
    std::uint8_t getDataBus() const ;
    std::uint8_t& getDataBus() { return dataBus ; }
    void setDataBus(std::uint8_t data) ;
    bool getIRQLine() const ;
    void setIRQLine(bool x) ;
    bool getNMILine() const ;
    void setNMILine(bool x) ;
    bool getResetLine() const ;
    void setResetLine(bool x) ;

    // Read the registers.
    std::uint8_t getX() const ;
    std::uint8_t getY() const ;
    std::uint8_t getA() const ;
    std::uint8_t getS() const ;
    std::uint8_t getP(bool b = false) const ;
    std::uint16_t getPC() const ;
    std::uint16_t getPCForCurrentInstruction() const ;
    std::uint8_t getIR() const ;
    int getT() const ;

    // Write the registers: use at your own peril.
    void setA(std::uint8_t A) ;
    void setX(std::uint8_t X) ;
    void setY(std::uint8_t Y) ;
    void setS(std::uint8_t S) ;
    void setP(std::uint8_t P) ;
    void setPC(std::uint16_t addr) ;
    void setIR(std::uint8_t IR) ;
    void setT(int T) ;

  private:
    // Transient CPU state.
    InstructionTraits dc ; // Can be deduced from IR.
    bool verbose ;

    // Helpers.
    std::uint8_t setNZ(std::uint8_t value) ;
    std::uint8_t xADC(std::uint8_t value) ;
    std::uint8_t xANC(std::uint8_t value) ;
    std::uint8_t xAND(std::uint8_t value) ;
    std::uint8_t xALR(std::uint8_t value) ;
    std::uint8_t xARR(std::uint8_t value) ;
    std::uint8_t xAXS(std::uint8_t value) ;
    void xCMP(std::uint8_t value) ;
    std::uint8_t xEOR(std::uint8_t value) ;
    std::uint8_t xORA(std::uint8_t value) ;
    std::uint8_t xSBC(std::uint8_t value) ;
    void xSTA(std::uint8_t value) ;
    void xBIT(std::uint8_t value) ;
    void xCPX(std::uint8_t value) ;
    void xCPY(std::uint8_t value) ;
    std::uint8_t xASL(std::uint8_t value) ;
    std::uint8_t xLSR(std::uint8_t value) ;
    std::uint8_t xROL(std::uint8_t value) ;
    std::uint8_t xROR(std::uint8_t value) ;
    void readFrom(std::uint16_t addr) ;
    void writeTo(std::uint16_t addr, std::uint8_t value) ;
  } ;

  // -------------------------------------------------------------------
  // MARK: - Getters & setters
  // -------------------------------------------------------------------

  inline
  M6502State::PRegister&
  M6502State::PRegister::operator= (std::uint8_t value)
  {
    std::bitset<8>::operator=(value) ;
    (*this)[4]=false ;
    (*this)[5]=false ;
    return *this;
  }

  inline
  M6502State::PRegister::operator std::uint8_t() const
  {
    return to_ulong() ;
  }

  /** Get the RW line.

   The function returns @c true if the CPU is performing a @em read
   operation.
   */

  inline bool M6502::getRW() const
  {
    return RW ;
  }

  /** Get the current value on the address bus.

   The CPU places a new address duing each cycle.
   */

  inline std::uint16_t M6502::getAddressBus() const
  {
    return addressBus;
  }

  /** Get the current value placed by the CPU on the data bus.

   This operation is only valid when the CPU is performing a write
   operation, which can be determined by using `getRW()`.
   */

  inline std::uint8_t M6502::getDataBus() const
  {
    return dataBus;
  }

  /** Writes data to the data bus.

   This operation is only valid during a read cycle.

   \sa getRW().
   */

  inline void M6502::setDataBus(std::uint8_t data)
  {
    dataBus = data;
  }


  inline std::uint8_t M6502::getIR() const
  {
    return IR ;
  }

  inline std::uint8_t M6502::getS() const
  {
    return S ;
  }

  inline std::uint16_t M6502::getPC() const
  {
    return PC ;
  }

  inline std::uint16_t M6502::getPCForCurrentInstruction() const
  {
    return PCCurrent ;
  }

  inline std::uint8_t M6502::getA() const
  {
    return A ;
  }

  inline std::uint8_t M6502::getX() const
  {
    return X ;
  }

  inline std::uint8_t M6502::getY() const
  {
    return Y ;
  }

  inline std::uint8_t M6502::getP(bool b) const
  {
    return static_cast<uint8_t>(P) | (b << 4) | (1 << 5) ;
  }

  inline int M6502::getT() const
  {
    return T ;
  }

  inline bool M6502::getNMILine() const
  {
    return nmiLine ;
  }

  inline bool M6502::getIRQLine() const
  {
    return irqLine ;
  }

  inline bool M6502::getResetLine() const
  {
    return resetLine ;
  }

  inline size_t M6502::getNumCycles() const
  {
    return numCycles ;
  }

  inline bool M6502::getVerbose() const
  {
    return verbose ;
  }

  inline void M6502::setA(std::uint8_t A)
  {
    this->A = A ;
  }

  inline void M6502::setX(std::uint8_t X)
  {
    this->X = X ;
  }

  inline void M6502::setY(std::uint8_t Y)
  {
    this->Y = Y ;
  }

  inline void M6502::setPC(std::uint16_t addr)
  {
    PCP = PC = addr ;
  }

  inline void M6502::setIR(std::uint8_t IR)
  {
    this->IR = IR ;
  }

  inline void M6502::setT(int T)
  {
    TP = this->T = T ;
  }

  inline void M6502::setIRQLine(bool x)
  {
    irqLine = x ;
  }

  inline void M6502::setNMILine(bool x)
  {
    nmiLine = x ;
  }

  inline void M6502::setResetLine(bool x)
  {
    resetLine = x ;
  }

  inline void M6502::setP(std::uint8_t x)
  {
    P = x ;
  }

  inline void M6502::setVerbose(bool x)
  {
    verbose = x ;
  }

  // -------------------------------------------------------------------
  // MARK: - Input & Output
  // -------------------------------------------------------------------

  void to_json(nlohmann::json& j, M6502State const& state) ;
  void from_json(nlohmann::json const& j, M6502State& state) ;

  template<class T> struct asBytesWrapper
  {
    asBytesWrapper(T& object) : object(object) { }
    T& get() { return object ;}
    T& object ;
  } ;

  template<class T> asBytesWrapper<T const> asBytes(T const& object) {
    return asBytesWrapper<T const>(object) ;
  }
}

std::ostream& operator<<(std::ostream & os, jigo::M6502::InstructionTraits const &ins) ;
std::ostream& operator<<(std::ostream & os, jigo::M6502::Instruction const &ins) ;
std::ostream& operator<<(std::ostream & os, jigo::asBytesWrapper<jigo::M6502::InstructionTraits const> ins) ;
std::ostream& operator<<(std::ostream & os, jigo::asBytesWrapper<jigo::M6502::Instruction const> ins) ;

#endif /* M6502_hpp */
