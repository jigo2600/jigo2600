// M6502.cpp
// M6502 emulator

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "M6502.hpp"
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#define LAS_LIKE_VISUAL6502 1
#define XAA_LIKE_VISUAL6502 1

using namespace std;
using namespace jigo;
using json = nlohmann::json;

// -------------------------------------------------------------------
// MARK: - Printing
// -------------------------------------------------------------------

template <typename T> static inline ostream& printHelper(ostream& os, M6502::InstructionTraits const& ins, T operand) {
  auto w = os.width();
  auto f = os.flags();
  auto l = os.fill();
  if (ins.instructionType == M6502::M6502::UNKNOWN) {
    os << ".byte " << hex << uppercase << " $" << setfill('0') << setw(2) << (int)ins.opcode;
  } else {
    os << ins.mnemonic << hex << uppercase;
    os << setfill(is_same<T, char const*>::value ? 'h' : '0');
    auto idx = (ins.indexingType == M6502::XIndexing) ? "X" : "Y";
    switch (ins.addressingMode) {
    case M6502::immediate: os << " #$" << setw(2) << operand; break;
    case M6502::absolute: os << " $" << setw(4) << operand; break;
    case M6502::absoluteIndexed: os << " $" << setw(4) << operand << "," << idx; break;
    case M6502::absoluteIndirect: os << " ($" << setw(4) << operand << ")"; break;
    case M6502::zeroPage: os << " $" << setw(2) << operand; break;
    case M6502::zeroPageIndexed: os << " $" << setw(2) << operand << "," << idx; break;
    case M6502::zeroPageIndexedIndirect: os << " ($" << setw(2) << operand << "," << idx << ")"; break;
    case M6502::zeroPageIndirectIndexed: os << " ($" << setw(2) << operand << ")," << idx; break;
    case M6502::pcRelative: os << " $" << setw(4) << operand; break;
    default: break;
    }
  }
  os.fill(l);
  os.flags(f);
  os.width(w);
  return os;
}

ostream& operator<<(ostream& os, M6502::InstructionTraits const& ins) {
  return printHelper(os, ins, "");
}

ostream& operator<<(ostream& os, M6502::Instruction const& ins) {
  return printHelper(os, ins, ins.operand);
}

std::ostream& operator<<(std::ostream& os, asBytesWrapper<M6502::InstructionTraits const> ins) {
  auto w = os.width();
  auto f = os.flags();
  auto l = os.fill();
  os << hex << setw(2) << setfill('0') << (unsigned)ins.get().opcode;
  if (ins.get().length >= 2) os << " hh";
  if (ins.get().length >= 3) os << " hh";
  os.fill(l);
  os.flags(f);
  os.width(w);
  return os;
}

std::ostream& operator<<(std::ostream& os, asBytesWrapper<M6502::Instruction const> ins) {
  auto w = os.width();
  auto f = os.flags();
  auto l = os.fill();
  os << hex << setw(2) << setfill('0') << (unsigned)ins.get().opcode;
  if (ins.get().length >= 2) os << " " << setw(2) << (ins.get().operand & 0xff);
  if (ins.get().length >= 3) os << " " << setw(2) << (ins.get().operand >> 8);
  os.fill(l);
  os.flags(f);
  os.width(w);
  return os;
}

// -------------------------------------------------------------------
// MARK: - Serialize & deserialize state
// -------------------------------------------------------------------

void jigo::to_json(json& j, const M6502State& s) {
#undef jput
#define jput(x) j[#x] = s.x
  jput(RW);
  jput(addressBus);
  jput(dataBus);
  jput(A);
  jput(X);
  jput(Y);
  jput(S);
  jput(PC);
  jput(IR);
  j["P"] = static_cast<uint8_t>(s.P);
  jput(PCIR);
  jput(PCP);
  jput(AD);
  jput(T);
  jput(TP);
  jput(numCycles);
  jput(nmiLine);
  jput(irqLine);
  jput(resetLine);
}

/// Throws `nlohmann::json::exception` on parsing errors.
void jigo::from_json(const json& j, M6502State& state) {
#undef jget
#define jget(x) state.x = j.at(#x)
  jget(RW);
  jget(addressBus);
  jget(dataBus);
  jget(A);
  jget(X);
  jget(Y);
  jget(S);
  jget(PC);
  jget(IR);
  state.P = static_cast<uint8_t>(j.at("P"));
  jget(PCIR);
  jget(PCP);
  jget(AD);
  jget(T);
  jget(TP);
  jget(numCycles);
  jget(nmiLine);
  jget(irqLine);
  jget(resetLine);
}

// -------------------------------------------------------------------
// MARK: - Helpers
// -------------------------------------------------------------------

const char* instructionNames[] = {
    // Official instructions.
    // clang-format off
  "ADC", "AND", "ASL", "BCC", "BCS",
  "BEQ", "BIT", "BMI", "BNE", "BPL",
  "BRK", "BVC", "BVS", "CLC", "CLD",
  "CLI", "CLV", "CMP", "CPX", "CPY",
  "DEC", "DEX", "DEY", "EOR", "INC",
  "INX", "INY", "JMP", "JSR", "LDA",
  "LDX", "LDY", "LSR", "NOP", "ORA",
  "PHA", "PHP", "PLA", "PLP", "ROL",
  "ROR", "RTI", "RTS", "SBC", "SEC",
  "SED", "SEI", "STA", "STX", "STY",
  "TAX", "TAY", "TSX", "TXA", "TXS",
  "TYA",
  // Unodcumented instructions.
  "AHX", "ALR", "ANC", "ARR", "AXS",
  "DCP", "ISC", "KIL", "LAS", "LAX",
  "RLA", "RRA", "SAX", "SHX", "SHY",
  "SLO", "SRE", "TAS", "XAA", "???"
    // clang-format on
};

struct OpcodeTable {
  M6502::InstructionTraits data[256];

  OpcodeTable() {
#define instr(op, len, ins, ty, addr, ind, ill, ata) \
  case 0x##op:                                       \
    dc.opcode = 0x##op;                              \
    dc.mnemonic = #ins;                              \
    dc.length = len;                                 \
    dc.instructionType = M6502::ins;                 \
    dc.accessType = M6502::AccessType::ty;           \
    dc.addressingMode = M6502::AddressingMode::addr; \
    dc.indexingType = M6502::IndexingType::ind;      \
    dc.illegal = ill;                                \
    dc.addToA = ata;                                 \
    break;
    for (int opcode = 0; opcode < 256; ++opcode) {
      M6502::InstructionTraits dc;
      switch (opcode) {
        instr(00, 1, BRK, misc, implied, noIndexing, 0, 0);
        instr(01, 2, ORA, read, zeroPageIndexedIndirect, XIndexing, 0, 1);
        instr(02, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(03, 2, SLO, readWrite, zeroPageIndexedIndirect, XIndexing, 1, 1);
        instr(04, 2, NOP, read, zeroPage, noIndexing, 1, 0);
        instr(05, 2, ORA, read, zeroPage, noIndexing, 0, 1);
        instr(06, 2, ASL, readWrite, zeroPage, noIndexing, 0, 0);
        instr(07, 2, SLO, readWrite, zeroPage, noIndexing, 1, 1);
        instr(08, 1, PHP, stack, push, noIndexing, 0, 0);
        instr(09, 2, ORA, read, immediate, noIndexing, 0, 1);
        instr(0A, 1, ASL, noAccess, implied, noIndexing, 0, 1);
        instr(0B, 2, ANC, read, immediate, noIndexing, 1, 1);
        instr(0C, 3, NOP, read, absolute, noIndexing, 1, 0);
        instr(0D, 3, ORA, read, absolute, noIndexing, 0, 1);
        instr(0E, 3, ASL, readWrite, absolute, noIndexing, 0, 0);
        instr(0F, 3, SLO, readWrite, absolute, noIndexing, 1, 1);
        instr(10, 2, BPL, branch, pcRelative, noIndexing, 0, 0);
        instr(11, 2, ORA, read, zeroPageIndirectIndexed, YIndexing, 0, 1);
        instr(12, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(13, 2, SLO, readWrite, zeroPageIndirectIndexed, YIndexing, 1, 1);
        instr(14, 2, NOP, read, zeroPageIndexed, XIndexing, 1, 0);
        instr(15, 2, ORA, read, zeroPageIndexed, XIndexing, 0, 1);
        instr(16, 2, ASL, readWrite, zeroPageIndexed, XIndexing, 0, 0);
        instr(17, 2, SLO, readWrite, zeroPageIndexed, XIndexing, 1, 1);
        instr(18, 1, CLC, noAccess, implied, noIndexing, 0, 0);
        instr(19, 3, ORA, read, absoluteIndexed, YIndexing, 0, 1);
        instr(1A, 1, NOP, noAccess, implied, noIndexing, 1, 0);
        instr(1B, 3, SLO, readWrite, absoluteIndexed, YIndexing, 1, 1);
        instr(1C, 3, NOP, read, absoluteIndexed, XIndexing, 1, 0);
        instr(1D, 3, ORA, read, absoluteIndexed, XIndexing, 0, 1);
        instr(1E, 3, ASL, readWrite, absoluteIndexed, XIndexing, 0, 0);
        instr(1F, 3, SLO, readWrite, absoluteIndexed, XIndexing, 1, 1);
        instr(20, 3, JSR, misc, absolute, noIndexing, 0, 0);
        instr(21, 2, AND, read, zeroPageIndexedIndirect, XIndexing, 0, 1);
        instr(22, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(23, 2, RLA, readWrite, zeroPageIndexedIndirect, XIndexing, 1, 1);
        instr(24, 2, BIT, read, zeroPage, noIndexing, 0, 0);
        instr(25, 2, AND, read, zeroPage, noIndexing, 0, 1);
        instr(26, 2, ROL, readWrite, zeroPage, noIndexing, 0, 0);
        instr(27, 2, RLA, readWrite, zeroPage, noIndexing, 1, 1);
        instr(28, 1, PLP, stack, pull, noIndexing, 0, 0);
        instr(29, 2, AND, read, immediate, noIndexing, 0, 1);
        instr(2A, 1, ROL, noAccess, implied, noIndexing, 0, 1);
        instr(2B, 2, ANC, read, immediate, noIndexing, 1, 1);
        instr(2C, 3, BIT, read, absolute, noIndexing, 0, 0);
        instr(2D, 3, AND, read, absolute, noIndexing, 0, 1);
        instr(2E, 3, ROL, readWrite, absolute, noIndexing, 0, 0);
        instr(2F, 3, RLA, readWrite, absolute, noIndexing, 1, 1);
        instr(30, 2, BMI, branch, pcRelative, noIndexing, 0, 0);
        instr(31, 2, AND, read, zeroPageIndirectIndexed, YIndexing, 0, 1);
        instr(32, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(33, 2, RLA, readWrite, zeroPageIndirectIndexed, YIndexing, 1, 1);
        instr(34, 2, NOP, read, zeroPageIndexed, XIndexing, 1, 0);
        instr(35, 2, AND, read, zeroPageIndexed, XIndexing, 0, 1);
        instr(36, 2, ROL, readWrite, zeroPageIndexed, XIndexing, 0, 0);
        instr(37, 2, RLA, readWrite, zeroPageIndexed, XIndexing, 1, 1);
        instr(38, 1, SEC, noAccess, implied, noIndexing, 0, 0);
        instr(39, 3, AND, read, absoluteIndexed, YIndexing, 0, 1);
        instr(3A, 1, NOP, noAccess, implied, noIndexing, 1, 0);
        instr(3B, 3, RLA, readWrite, absoluteIndexed, YIndexing, 1, 1);
        instr(3C, 3, NOP, read, absoluteIndexed, XIndexing, 1, 0);
        instr(3D, 3, AND, read, absoluteIndexed, XIndexing, 0, 1);
        instr(3E, 3, ROL, readWrite, absoluteIndexed, XIndexing, 0, 0);
        instr(3F, 3, RLA, readWrite, absoluteIndexed, XIndexing, 1, 1);
        instr(40, 1, RTI, misc, implied, noIndexing, 0, 0);
        instr(41, 2, EOR, read, zeroPageIndexedIndirect, XIndexing, 0, 1);
        instr(42, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(43, 2, SRE, readWrite, zeroPageIndexedIndirect, XIndexing, 1, 1);
        instr(44, 2, NOP, read, zeroPage, noIndexing, 1, 0);
        instr(45, 2, EOR, read, zeroPage, noIndexing, 0, 1);
        instr(46, 2, LSR, readWrite, zeroPage, noIndexing, 0, 0);
        instr(47, 2, SRE, readWrite, zeroPage, noIndexing, 1, 1);
        instr(48, 1, PHA, stack, push, noIndexing, 0, 0);
        instr(49, 2, EOR, read, immediate, noIndexing, 0, 1);
        instr(4A, 1, LSR, noAccess, implied, noIndexing, 0, 1);
        instr(4B, 2, ALR, read, immediate, noIndexing, 1, 1);
        instr(4C, 3, JMP, misc, absolute, noIndexing, 0, 0);
        instr(4D, 3, EOR, read, absolute, noIndexing, 0, 1);
        instr(4E, 3, LSR, readWrite, absolute, noIndexing, 0, 0);
        instr(4F, 3, SRE, readWrite, absolute, noIndexing, 1, 1);
        instr(50, 2, BVC, branch, pcRelative, noIndexing, 0, 0);
        instr(51, 2, EOR, read, zeroPageIndirectIndexed, YIndexing, 0, 1);
        instr(52, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(53, 2, SRE, readWrite, zeroPageIndirectIndexed, YIndexing, 1, 1);
        instr(54, 2, NOP, read, zeroPageIndexed, XIndexing, 1, 0);
        instr(55, 2, EOR, read, zeroPageIndexed, XIndexing, 0, 1);
        instr(56, 2, LSR, readWrite, zeroPageIndexed, XIndexing, 0, 0);
        instr(57, 2, SRE, readWrite, zeroPageIndexed, XIndexing, 1, 1);
        instr(58, 1, CLI, noAccess, implied, noIndexing, 0, 0);
        instr(59, 3, EOR, read, absoluteIndexed, YIndexing, 0, 1);
        instr(5A, 1, NOP, noAccess, implied, noIndexing, 1, 0);
        instr(5B, 3, SRE, readWrite, absoluteIndexed, YIndexing, 1, 1);
        instr(5C, 3, NOP, read, absoluteIndexed, XIndexing, 1, 0);
        instr(5D, 3, EOR, read, absoluteIndexed, XIndexing, 0, 1);
        instr(5E, 3, LSR, readWrite, absoluteIndexed, XIndexing, 0, 0);
        instr(5F, 3, SRE, readWrite, absoluteIndexed, XIndexing, 1, 1);
        instr(60, 1, RTS, misc, implied, noIndexing, 0, 0);
        instr(61, 2, ADC, read, zeroPageIndexedIndirect, XIndexing, 0, 1);
        instr(62, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(63, 2, RRA, readWrite, zeroPageIndexedIndirect, XIndexing, 1, 1);
        instr(64, 2, NOP, read, zeroPage, noIndexing, 1, 0);
        instr(65, 2, ADC, read, zeroPage, noIndexing, 0, 1);
        instr(66, 2, ROR, readWrite, zeroPage, noIndexing, 0, 0);
        instr(67, 2, RRA, readWrite, zeroPage, noIndexing, 1, 1);
        instr(68, 1, PLA, stack, pull, noIndexing, 0, 0);
        instr(69, 2, ADC, read, immediate, noIndexing, 0, 1);
        instr(6A, 1, ROR, noAccess, implied, noIndexing, 0, 1);
        instr(6B, 2, ARR, read, immediate, noIndexing, 1, 1);
        instr(6C, 3, JMP, misc, absoluteIndirect, noIndexing, 0, 0);
        instr(6D, 3, ADC, read, absolute, noIndexing, 0, 1);
        instr(6E, 3, ROR, readWrite, absolute, noIndexing, 0, 0);
        instr(6F, 3, RRA, readWrite, absolute, noIndexing, 1, 1);
        instr(70, 2, BVS, branch, pcRelative, noIndexing, 0, 0);
        instr(71, 2, ADC, read, zeroPageIndirectIndexed, YIndexing, 0, 1);
        instr(72, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(73, 2, RRA, readWrite, zeroPageIndirectIndexed, YIndexing, 1, 1);
        instr(74, 2, NOP, read, zeroPageIndexed, XIndexing, 1, 0);
        instr(75, 2, ADC, read, zeroPageIndexed, XIndexing, 0, 1);
        instr(76, 2, ROR, readWrite, zeroPageIndexed, XIndexing, 0, 0);
        instr(77, 2, RRA, readWrite, zeroPageIndexed, XIndexing, 1, 1);
        instr(78, 1, SEI, noAccess, implied, noIndexing, 0, 0);
        instr(79, 3, ADC, read, absoluteIndexed, YIndexing, 0, 1);
        instr(7A, 1, NOP, noAccess, implied, noIndexing, 1, 0);
        instr(7B, 3, RRA, readWrite, absoluteIndexed, YIndexing, 1, 1);
        instr(7C, 3, NOP, read, absoluteIndexed, XIndexing, 1, 0);
        instr(7D, 3, ADC, read, absoluteIndexed, XIndexing, 0, 1);
        instr(7E, 3, ROR, readWrite, absoluteIndexed, XIndexing, 0, 0);
        instr(7F, 3, RRA, readWrite, absoluteIndexed, XIndexing, 1, 1);
        instr(80, 2, NOP, read, immediate, noIndexing, 1, 0);
        instr(81, 2, STA, write, zeroPageIndexedIndirect, XIndexing, 0, 0);
        instr(82, 2, NOP, read, immediate, noIndexing, 1, 0);
        instr(83, 2, SAX, write, zeroPageIndexedIndirect, XIndexing, 1, 0);
        instr(84, 2, STY, write, zeroPage, noIndexing, 0, 0);
        instr(85, 2, STA, write, zeroPage, noIndexing, 0, 0);
        instr(86, 2, STX, write, zeroPage, noIndexing, 0, 0);
        instr(87, 2, SAX, write, zeroPage, noIndexing, 1, 0);
        instr(88, 1, DEY, noAccess, implied, noIndexing, 0, 0);
        instr(89, 2, NOP, read, immediate, noIndexing, 1, 0);
        instr(8A, 1, TXA, noAccess, implied, noIndexing, 0, 0);
        instr(8B, 2, XAA, read, immediate, noIndexing, 1, 0);
        instr(8C, 3, STY, write, absolute, noIndexing, 0, 0);
        instr(8D, 3, STA, write, absolute, noIndexing, 0, 0);
        instr(8E, 3, STX, write, absolute, noIndexing, 0, 0);
        instr(8F, 3, SAX, write, absolute, noIndexing, 1, 0);
        instr(90, 2, BCC, branch, pcRelative, noIndexing, 0, 0);
        instr(91, 2, STA, write, zeroPageIndirectIndexed, YIndexing, 0, 0);
        instr(92, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(93, 2, AHX, write, zeroPageIndirectIndexed, YIndexing, 1, 0);
        instr(94, 2, STY, write, zeroPageIndexed, XIndexing, 0, 0);
        instr(95, 2, STA, write, zeroPageIndexed, XIndexing, 0, 0);
        instr(96, 2, STX, write, zeroPageIndexed, YIndexing, 0, 0);
        instr(97, 2, SAX, write, zeroPageIndexed, YIndexing, 1, 0);
        instr(98, 1, TYA, noAccess, implied, noIndexing, 0, 0);
        instr(99, 3, STA, write, absoluteIndexed, YIndexing, 0, 0);
        instr(9A, 1, TXS, noAccess, implied, noIndexing, 0, 0);
        instr(9B, 3, TAS, write, absoluteIndexed, YIndexing, 1, 0);
        instr(9C, 3, SHY, write, absoluteIndexed, XIndexing, 1, 0);
        instr(9D, 3, STA, write, absoluteIndexed, XIndexing, 0, 0);
        instr(9E, 3, SHX, write, absoluteIndexed, YIndexing, 1, 0);
        instr(9F, 3, AHX, write, absoluteIndexed, YIndexing, 1, 0);
        instr(A0, 2, LDY, read, immediate, noIndexing, 0, 0);
        instr(A1, 2, LDA, read, zeroPageIndexedIndirect, XIndexing, 0, 0);
        instr(A2, 2, LDX, read, immediate, noIndexing, 0, 0);
        instr(A3, 2, LAX, read, zeroPageIndexedIndirect, XIndexing, 1, 0);
        instr(A4, 2, LDY, read, zeroPage, noIndexing, 0, 0);
        instr(A5, 2, LDA, read, zeroPage, noIndexing, 0, 0);
        instr(A6, 2, LDX, read, zeroPage, noIndexing, 0, 0);
        instr(A7, 2, LAX, read, zeroPage, noIndexing, 1, 0);
        instr(A8, 1, TAY, noAccess, implied, noIndexing, 0, 0);
        instr(A9, 2, LDA, read, immediate, noIndexing, 0, 0);
        instr(AA, 1, TAX, noAccess, implied, noIndexing, 0, 0);
        instr(AB, 2, LAX, read, immediate, noIndexing, 1, 0);
        instr(AC, 3, LDY, read, absolute, noIndexing, 0, 0);
        instr(AD, 3, LDA, read, absolute, noIndexing, 0, 0);
        instr(AE, 3, LDX, read, absolute, noIndexing, 0, 0);
        instr(AF, 3, LAX, read, absolute, noIndexing, 1, 0);
        instr(B0, 2, BCS, branch, pcRelative, noIndexing, 0, 0);
        instr(B1, 2, LDA, read, zeroPageIndirectIndexed, YIndexing, 0, 0);
        instr(B2, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(B3, 2, LAX, read, zeroPageIndirectIndexed, YIndexing, 1, 0);
        instr(B4, 2, LDY, read, zeroPageIndexed, XIndexing, 0, 0);
        instr(B5, 2, LDA, read, zeroPageIndexed, XIndexing, 0, 0);
        instr(B6, 2, LDX, read, zeroPageIndexed, YIndexing, 0, 0);
        instr(B7, 2, LAX, read, zeroPageIndexed, YIndexing, 1, 0);
        instr(B8, 1, CLV, noAccess, implied, noIndexing, 0, 0);
        instr(B9, 3, LDA, read, absoluteIndexed, YIndexing, 0, 0);
        instr(BA, 1, TSX, noAccess, implied, noIndexing, 0, 0);
        instr(BB, 3, LAS, read, absoluteIndexed, YIndexing, 1, 0);
        instr(BC, 3, LDY, read, absoluteIndexed, XIndexing, 0, 0);
        instr(BD, 3, LDA, read, absoluteIndexed, XIndexing, 0, 0);
        instr(BE, 3, LDX, read, absoluteIndexed, YIndexing, 0, 0);
        instr(BF, 3, LAX, read, absoluteIndexed, YIndexing, 1, 0);
        instr(C0, 2, CPY, read, immediate, noIndexing, 0, 0);
        instr(C1, 2, CMP, read, zeroPageIndexedIndirect, XIndexing, 0, 0);
        instr(C2, 2, NOP, read, immediate, noIndexing, 1, 0);
        instr(C3, 2, DCP, readWrite, zeroPageIndexedIndirect, XIndexing, 1, 0);
        instr(C4, 2, CPY, read, zeroPage, noIndexing, 0, 0);
        instr(C5, 2, CMP, read, zeroPage, noIndexing, 0, 0);
        instr(C6, 2, DEC, readWrite, zeroPage, noIndexing, 0, 0);
        instr(C7, 2, DCP, readWrite, zeroPage, noIndexing, 1, 0);
        instr(C8, 1, INY, noAccess, implied, noIndexing, 0, 0);
        instr(C9, 2, CMP, read, immediate, noIndexing, 0, 0);
        instr(CA, 1, DEX, noAccess, implied, noIndexing, 0, 0);
        instr(CB, 2, AXS, read, immediate, noIndexing, 1, 0);
        instr(CC, 3, CPY, read, absolute, noIndexing, 0, 0);
        instr(CD, 3, CMP, read, absolute, noIndexing, 0, 0);
        instr(CE, 3, DEC, readWrite, absolute, XIndexing, 0, 0);
        instr(CF, 3, DCP, readWrite, absolute, noIndexing, 1, 0);
        instr(D0, 2, BNE, branch, pcRelative, noIndexing, 0, 0);
        instr(D1, 2, CMP, read, zeroPageIndirectIndexed, YIndexing, 0, 0);
        instr(D2, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(D3, 2, DCP, readWrite, zeroPageIndirectIndexed, YIndexing, 1, 0);
        instr(D4, 2, NOP, read, zeroPageIndexed, XIndexing, 1, 0);
        instr(D5, 2, CMP, read, zeroPageIndexed, XIndexing, 0, 0);
        instr(D6, 2, DEC, readWrite, zeroPageIndexed, XIndexing, 0, 0);
        instr(D7, 2, DCP, readWrite, zeroPageIndexed, XIndexing, 1, 0);
        instr(D8, 1, CLD, noAccess, implied, noIndexing, 0, 0);
        instr(D9, 3, CMP, read, absoluteIndexed, YIndexing, 0, 0);
        instr(DA, 1, NOP, noAccess, implied, noIndexing, 1, 0);
        instr(DB, 3, DCP, readWrite, absoluteIndexed, YIndexing, 1, 0);
        instr(DC, 3, NOP, read, absoluteIndexed, XIndexing, 1, 0);
        instr(DD, 3, CMP, read, absoluteIndexed, XIndexing, 0, 0);
        instr(DE, 3, DEC, readWrite, absoluteIndexed, XIndexing, 0, 0);
        instr(DF, 3, DCP, readWrite, absoluteIndexed, XIndexing, 1, 0);
        instr(E0, 2, CPX, read, immediate, noIndexing, 0, 0);
        instr(E1, 2, SBC, read, zeroPageIndexedIndirect, XIndexing, 0, 1);
        instr(E2, 2, NOP, read, immediate, noIndexing, 1, 0);
        instr(E3, 2, ISC, readWrite, zeroPageIndexedIndirect, XIndexing, 1, 1);
        instr(E4, 2, CPX, read, zeroPage, noIndexing, 0, 0);
        instr(E5, 2, SBC, read, zeroPage, noIndexing, 0, 1);
        instr(E6, 2, INC, readWrite, zeroPage, noIndexing, 0, 0);
        instr(E7, 2, ISC, readWrite, zeroPage, noIndexing, 1, 1);
        instr(E8, 1, INX, noAccess, implied, noIndexing, 0, 0);
        instr(E9, 2, SBC, read, immediate, noIndexing, 0, 1);
        instr(EA, 1, NOP, noAccess, implied, noIndexing, 0, 0);
        instr(EB, 2, SBC, read, immediate, noIndexing, 1, 1);
        instr(EC, 3, CPX, read, absolute, noIndexing, 0, 0);
        instr(ED, 3, SBC, read, absolute, noIndexing, 0, 1);
        instr(EE, 3, INC, readWrite, absolute, XIndexing, 0, 0);
        instr(EF, 3, ISC, readWrite, absolute, noIndexing, 1, 1);
        instr(F0, 2, BEQ, branch, pcRelative, noIndexing, 0, 0);
        instr(F1, 2, SBC, read, zeroPageIndirectIndexed, YIndexing, 0, 1);
        instr(F2, 1, KIL, noAccess, implied, noIndexing, 1, 0);
        instr(F3, 2, ISC, readWrite, zeroPageIndirectIndexed, YIndexing, 1, 1);
        instr(F4, 2, NOP, read, zeroPageIndexed, XIndexing, 1, 0);
        instr(F5, 2, SBC, read, zeroPageIndexed, XIndexing, 0, 1);
        instr(F6, 2, INC, readWrite, zeroPageIndexed, XIndexing, 0, 0);
        instr(F7, 2, ISC, readWrite, zeroPageIndexed, XIndexing, 1, 1);
        instr(F8, 1, SED, noAccess, implied, noIndexing, 0, 0);
        instr(F9, 3, SBC, read, absoluteIndexed, YIndexing, 0, 1);
        instr(FA, 1, NOP, noAccess, implied, noIndexing, 1, 0);
        instr(FB, 3, ISC, readWrite, absoluteIndexed, YIndexing, 1, 1);
        instr(FC, 3, NOP, read, absoluteIndexed, XIndexing, 1, 0);
        instr(FD, 3, SBC, read, absoluteIndexed, XIndexing, 0, 1);
        instr(FE, 3, INC, readWrite, absoluteIndexed, XIndexing, 0, 0);
        instr(FF, 3, ISC, readWrite, absoluteIndexed, XIndexing, 1, 1);
      }
      data[opcode] = dc;
    }
  }
} opcodeTable;

#define cmp(x) (x == s.x)
bool M6502State::operator==(M6502State const& s) const {
  return cmp(RW) && cmp(addressBus) && cmp(dataBus) && cmp(resetLine) && cmp(nmiLine) && cmp(irqLine) && cmp(A) &&
         cmp(X) && cmp(Y) && cmp(S) && cmp(P) && cmp(PC) && cmp(PCIR) && cmp(PCP) && cmp(IR) && cmp(AD) && cmp(ADD) &&
         cmp(T) && cmp(TP) && cmp(numCycles);
}
#undef cmp

M6502::InstructionTraits const& M6502::decode(uint8_t opcode) {
  return opcodeTable.data[opcode];
}

M6502::Instruction M6502::decode(array<uint8_t, 3> const& bytes) {
  Instruction ins;
  ins.InstructionTraits::operator=(M6502::decode(bytes[0]));
  ins.operand = 0;
  if (ins.length >= 2) {
    ins.operand += bytes[1];
  }
  if (ins.length >= 3) {
    ins.operand += static_cast<uint16_t>(bytes[2]) << 8;
  }
  return ins;
}

inline void M6502::readFrom(uint16_t addr) {
  addressBus = addr;
  RW = true;
}

inline void M6502::writeTo(uint16_t addr, uint8_t value) {
  addressBus = addr;
  dataBus = value;
  RW = false;
}

inline uint8_t M6502::setNZ(uint8_t value) {
  P[P.n] = (value & 0x80);
  P[P.z] = (value == 0);
  return value;
}

// -------------------------------------------------------------------
// MARK: - Lifecycle
// -------------------------------------------------------------------

M6502::M6502() {
  verbose = false;
  reset();
}

void M6502::reset() {
  *this = M6502State{};
}

M6502& M6502::operator=(M6502State const& s) {
  this->M6502State::operator=(s);
  // Restore transient state.
  dc = decode(IR);
  return *this;
}

// -------------------------------------------------------------------
// MARK: - Instruction set
// -------------------------------------------------------------------

// MARK: Arithmetic instruction

uint8_t M6502::xADC(uint8_t operand) {
  int16_t sum = A;
  sum = sum + operand + P[P.c];
  P[P.z] = ((sum & 0xff) == 0);
  if (P[P.d] == false) {
    // Binary mode.
    P[P.n] = (sum & 0x80);
    P[P.v] = ~(A ^ operand) & (A ^ sum) & 0x80;
    P[P.c] = (sum >= 0x100);
  } else {
    // BCD mode.
    uint16_t nibble0sum = ((sum & 0x1f) ^ (((A ^ operand)) & 0x10));
    if (nibble0sum >= 0xa) {
      sum += 0x06;
      if (nibble0sum >= 0x1a) {
        sum -= 0x10;
      } // For non-valid BCD.
    }
    P[P.n] = (sum & 0x80);
    P[P.v] = ~(A ^ operand) & (A ^ sum) & 0x80;
    if ((sum & 0x1f0) >= 0xa0) {
      sum += 0x60;
    }
    P[P.c] = (sum >= 0x100);
  }
  return static_cast<uint8_t>(sum);
}

uint8_t M6502::xSBC(uint8_t operand) {
  int16_t diff = A;
  diff = diff - operand + P[P.c] - 1;
  P[P.z] = ((diff & 0xff) == 0);
  if (P[P.d] == false) {
    // Binary mode.
    P[P.n] = (diff & 0x80);
    P[P.v] = (A ^ operand) & (A ^ diff) & 0x80;
    P[P.c] = (diff >= 0); // Complement of carry.
  } else {
    // BCD mode.
    uint16_t nibble0diff = ((diff & 0x1f) ^ ((A ^ operand) & 0x10));
    if (nibble0diff >= 0x10) {
      diff -= 0x6;
      if (nibble0diff <= 0x15) {
        diff += 0x10;
      } // For non-valid BCD.
    }
    P[P.n] = (diff & 0x80);
    P[P.v] = (A ^ operand) & (A ^ diff) & 0x80;
    if ((diff & 0x1f0) >= 0x100) {
      diff -= 0x60;
    }
    P[P.c] = (diff >= 0); // Complement of carry.
  }
  return static_cast<uint8_t>(diff);
}

// MARK: Comparison-like instructions

inline void M6502::xCMP(uint8_t value) {
  // The carry is set if there is *no* borrow in (A - value).
  P[P.c] = (A >= value);
  setNZ(A - value);
}

inline void M6502::xCPX(uint8_t value) {
  P[P.c] = (X >= value);
  setNZ(X - value);
}

inline void M6502::xCPY(uint8_t value) {
  P[P.c] = (Y >= value);
  setNZ(Y - value);
}

inline uint8_t M6502::xAXS(uint8_t value) {
  // Variant of xCMP.
  uint8_t tmp = X & A;
  P[P.c] = (tmp >= value); // c = 1 if there is no borrow in ((X&A) - value).
  return setNZ(tmp - value);
}

inline void M6502::xBIT(uint8_t value) {
  P[P.n] = (value >> 7) & 0x01;
  P[P.v] = (value >> 6) & 0x01;
  P[P.z] = ((value & A) == 0);
}

// MARK: Logic instructions

inline uint8_t M6502::xANC(uint8_t value) {
  value = xAND(value);
  P[P.c] = (value & 0x80);
  return value;
}

inline uint8_t M6502::xALR(uint8_t value) {
  value = xAND(value);
  value = xLSR(value);
  return value;
}

inline uint8_t M6502::xARR(uint8_t value) {
  value &= A;
  setNZ(value = (value >> 1) | (P[P.c] << 7));
  if (P[P.d] == false) {
    P[P.c] = (value & 0x40);
    P[P.v] = bool(value & 0x20) ^ P[P.c];
  } else {
    P[P.v] = (value ^ A) & 0x40;
    if ((A & 0x0f) >= 0x05) {
      value = ((value + 6) & 0x0f) | (value & 0xf0);
    }
    if ((A & 0xf0) >= 0x50) {
      value += 0x60;
      P[P.c] = 1;
    } else {
      P[P.c] = 0;
    }
  }
  return value;
}

inline uint8_t M6502::xAND(uint8_t value) {
  return setNZ(value &= A);
}

inline uint8_t M6502::xEOR(uint8_t value) {
  return setNZ(value ^= A);
}

inline uint8_t M6502::xORA(uint8_t value) {
  return setNZ(value |= A);
}

uint8_t M6502::xASL(uint8_t value) {
  P[P.c] = value >> 7;
  return setNZ(value <<= 1);
}

inline uint8_t M6502::xLSR(uint8_t value) {
  P[P.c] = value & 0x01;
  return setNZ(value >>= 1);
}

inline uint8_t M6502::xROL(uint8_t value) {
  uint8_t bit = P[P.c];
  P[P.c] = (value >> 7);
  return setNZ(value = (value << 1) | bit);
}

inline uint8_t M6502::xROR(uint8_t value) {
  uint8_t bit = P[P.c];
  P[P.c] = value & 0x01;
  return setNZ(value = (value >> 1) | (bit << 7));
}

// -------------------------------------------------------------------
// MARK: - Simulation core
// -------------------------------------------------------------------

/// Simulate one clock cycle. By the end of the call, the state
/// is the one stable at the *end* of the cycle, but before the
/// following clock edge.

/// Simulate either the first or second half cycle, depending on
/// the template parameter pre. We do this so that the code
/// sequence is still logical, as well as to avoid code
/// duplication in implementing the two halves.

#define fetch()   \
  {               \
    readFrom(PC); \
    PCP = PC + 1; \
  }

void M6502::cycle(bool busWasReady) {
  int currentT = T;
  uint16_t currentPC = PC;
  uint8_t currentDataBus = dataBus;

  if (!busWasReady && RW) {
    // The previous cycle was a stalled READ. We need to repeat
    // the read.
    ++numCycles;
    if (verbose) {
      cout << "M6502: Repeating cycle because of stall." << endl;
    }
    return;
  }

  // On Phi1 load PC from PCP and T from TP.
  PC = PCP;
  T = TP;

  // The T=1 cycle starts executing a new instruction.
  if (T == 1) {
    // Some instruction executiong ``spill'' to T = 1 despite the fact that the
    // opcode changes during this period. This is used to move the internal ADD
    // to another registes as needed.
    if (dc.addToA) {
      A = ADD;
    } else if (dc.instructionType == DEX) {
      X = ADD;
    } else if (dc.instructionType == INX) {
      X = ADD;
    } else if (dc.instructionType == DEY) {
      Y = ADD;
    } else if (dc.instructionType == INY) {
      Y = ADD;
    } else if (dc.instructionType == AXS) {
      X = ADD;
    }

    // Load a new instruction in IR. This is either the instruction in
    // the data bus or the BRK opcode if an interrupt occurs.
    if (resetLine || nmiLine || (irqLine & !P[P.i])) {
      IR = 0x00; // BRK
      // TODO: recovery from KIL on reset.
    } else {
      IR = dataBus;
      PCIR = addressBus;
    }
    dc = decode(IR);
  }

  if (dc.accessType == noAccess) {
    if (T == 1) {
      readFrom(PC); // Discarded.
      if (dc.instructionType == KIL) {
        PCP = PC + 1;
      } else {
        TP = -1;
      }
    } else if (T == 2) {
      readFrom(0xfffe);
      TP = 1;
    } // KIL.
    else if (T == 0) {
      switch (dc.instructionType) {
      case ASL: ADD = xASL(A); break;
      case DEX: ADD = setNZ(X - 1); break;
      case DEY: ADD = setNZ(Y - 1); break;
      case INX: ADD = setNZ(X + 1); break;
      case INY: ADD = setNZ(Y + 1); break;
      case LSR: ADD = xLSR(A); break;
      case ROL: ADD = xROL(A); break;
      case ROR: ADD = xROR(A); break;
      case CLC: P[P.c] = 0; break;
      case CLD: P[P.d] = 0; break;
      case CLI: P[P.i] = 0; break;
      case CLV: P[P.v] = 0; break;
      case NOP:; break;
      case SEC: P[P.c] = 1; break;
      case SED: P[P.d] = 1; break;
      case SEI: P[P.i] = 1; break;
      case TAX: setNZ(X = A); break;
      case TAY: setNZ(Y = A); break;
      case TSX: setNZ(X = S); break;
      case TXA: setNZ(A = X); break;
      case TXS: S = X; break; // Does not affect flags.
      case TYA: setNZ(A = Y); break;
      case KIL: break;
      default: break;
      }
      fetch();
    } else {
      assert(false);
    }
  }

  else if (dc.accessType == read || dc.accessType == write || dc.accessType == readWrite) {
    int Tx;
    switch (dc.addressingMode) {
      // On T==Tx this code is completed below using AD.
    case immediate:
      Tx = 1;
      if (T == 1) {
        AD = PC;
        PCP = PC + 1;
      }
      break;

    case zeroPage:
      Tx = 2;
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        AD = dataBus;
      }
      break;

    case zeroPageIndexed:
      Tx = 3;
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        AD = dataBus;
        readFrom(AD); // Discarded.
        AD = (AD + ((dc.indexingType == XIndexing) ? X : Y)) & 0x00ff;
      } else if (T == 3) {
      }
      break;

    case zeroPageIndexedIndirect: // (opcode,X)
      assert(dc.indexingType == XIndexing);
      Tx = 5;
      if (T == 1) {
        fetch();
      }
      if (T == 2) {
        readFrom(ADD = dataBus); // Discarded.
        ADD += X;
      } else if (T == 3) {
        readFrom(ADD);
      } else if (T == 4) {
        readFrom(uint8_t(ADD + 1));
        AD = dataBus;
      } else if (T == 5) {
        AD |= uint16_t(dataBus) << 8;
      }
      break;

    case zeroPageIndirectIndexed:
      Tx = 5;
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        readFrom(AD = dataBus);
      } else if (T == 3) {
        assert(dc.indexingType == YIndexing);
        readFrom((AD + 1) & 0xff);
        AD = uint16_t(dataBus) + Y;
      } else if (T == 4) {
        bool carry = (AD >= 0x100);
        AD = (AD & 0xff) | uint16_t(dataBus) << 8;
        if (dc.accessType == read && !carry) {
          T = ++TP;
        } // Skip step.
        else {
          readFrom(AD); // Discarded.
          if (dc.instructionType == AHX) {
            ADD = A & X & ((AD >> 8) + 1);
          }
          if (carry) {
            AD += 0x100;
            if (dc.instructionType == AHX) {
              AD = (AD & 0xff) | (uint16_t(ADD) << 8);
            }
          }
        }
      } else if (T == 5) {
      }
      break;

    case absolute:
      Tx = 3;
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        AD = dataBus;
        fetch();
      } else if (T == 3) {
        AD |= uint16_t(dataBus) << 8;
      }
      break;

    case absoluteIndexed:
      Tx = 4;
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        fetch();
        AD = uint16_t(dataBus) + ((dc.indexingType == XIndexing) ? X : Y);
      } else if (T == 3) {
        bool carry = (AD >= 0x100);
        AD = (AD & 0xff) | (uint16_t(dataBus) << 8);
        if (dc.accessType == read && !carry) {
          T = ++TP;
        } // Skip step.
        else {
          readFrom(AD); // Discarded.
          if (dc.instructionType == AHX || dc.instructionType == TAS) {
            ADD = A & X & ((AD >> 8) + 1);
          } else if (dc.instructionType == SHX) {
            ADD = X & ((AD >> 8) + 1);
          } else if (dc.instructionType == SHY) {
            ADD = Y & ((AD >> 8) + 1);
          }
          if (carry) {
            AD += 0x100;
            if (dc.instructionType == AHX || dc.instructionType == SHX || dc.instructionType == SHY ||
                dc.instructionType == TAS) {
              AD = (AD & 0xff) | (uint16_t(ADD) << 8);
            }
          }
        }
      } else if (T == 4) {
      }
      break;

    default: assert(false);
    }

    if (dc.accessType == read) {
      if (T == Tx) {
        readFrom(AD);
        TP = -1;
      } else if (T == 0) {
        fetch();
        switch (dc.instructionType) {
        case ADC: ADD = xADC(dataBus); break;
        case ALR: ADD = xALR(dataBus); break;
        case ANC: ADD = xANC(dataBus); break;
        case AND: ADD = xAND(dataBus); break;
        case ARR: ADD = xARR(dataBus); break;
        case AXS: ADD = xAXS(dataBus); break;
        case BIT: xBIT(dataBus); break;
        case CMP: xCMP(dataBus); break;
        case CPX: xCPX(dataBus); break;
        case CPY: xCPY(dataBus); break;
        case EOR: ADD = xEOR(dataBus); break;
        case LAS:
#ifdef LAS_LIKE_VISUAL6502
          X = S;
          setNZ(S & dataBus);
          A = (S & (dataBus | 0x11));
#else
          setNZ(X = (A = (SP &= dataBus)));
#endif
          break;
        case LAX: setNZ(X = (A = dataBus)); break;
        case LDA: setNZ(A = dataBus); break;
        case LDX: setNZ(X = dataBus); break;
        case LDY: setNZ(Y = dataBus); break;
        case NOP: break;
        case ORA: ADD = xORA(dataBus); break;
        case SBC: ADD = xSBC(dataBus); break;
        case XAA:
#ifdef XAA_LIKE_VISUAL6502
          setNZ(A = A & X & dataBus);
#else
          setNZ(A = (A | 0xee) & X & dataBus);
#endif
          break;
        default: assert(false);
        }
      }
    } else if (dc.accessType == write) {
      if (T == Tx) {
        switch (dc.instructionType) {
        case STA: writeTo(AD, A); break;
        case STX: writeTo(AD, X); break;
        case STY: writeTo(AD, Y); break;
        case SAX: writeTo(AD, A & X); break;
        case TAS:
        case SHX:
        case SHY:
        case AHX: writeTo(AD, ADD); break;
        default: assert(false);
        }
        TP = -1;
      } else if (T == 0) {
        fetch();
        // Illegal opcode.
        if (dc.instructionType == TAS) {
          S = A & X;
        }
      }
    } else if (dc.accessType == readWrite) {
      if (T == Tx) {
        readFrom(AD);
      } else if (T == Tx + 1) {
        switch (dc.instructionType) {
        case ASL:
        case SLO: ADD = xASL(dataBus); break;
        case DEC:
        case DCP: ADD = setNZ(dataBus - 1); break;
        case INC:
        case ISC: ADD = setNZ(dataBus + 1); break;
        case LSR:
        case SRE: ADD = xLSR(dataBus); break;
        case ROL:
        case RLA: ADD = xROL(dataBus); break;
        case ROR:
        case RRA: ADD = xROR(dataBus); break;
        default: assert(false);
        }
        writeTo(AD, dataBus); // Discarded.
      } else if (T == Tx + 2) {
        writeTo(AD, ADD);
        TP = -1;
      } else if (T == 0) {
        fetch();
        // Illegal opcodes.
        switch (dc.instructionType) {
        case DCP: xCMP(ADD); break;
        case ISC: ADD = xSBC(ADD); break;
        case RLA: ADD = xAND(ADD); break;
        case RRA: ADD = xADC(ADD); break;
        case SLO: ADD = xORA(ADD); break;
        case SRE: ADD = xEOR(ADD); break;
        default: break;
        }
      }
    }
  }

  else if (dc.accessType == branch) {
    if (T == 1) {
      fetch();
    } else if (T == 2) {
      bool takeBranch;
      switch (dc.instructionType) {
      case BCC: takeBranch = (P[P.c] == 0); break;
      case BCS: takeBranch = (P[P.c] == 1); break;
      case BNE: takeBranch = (P[P.z] == 0); break;
      case BEQ: takeBranch = (P[P.z] == 1); break;
      case BPL: takeBranch = (P[P.n] == 0); break;
      case BMI: takeBranch = (P[P.n] == 1); break;
      case BVC: takeBranch = (P[P.v] == 0); break;
      case BVS: takeBranch = (P[P.v] == 1); break;
      default: assert(false);
      }
      if (takeBranch) {
        readFrom(PC); // Discarded.
        uint16_t rel = (unsigned)static_cast<int8_t>(dataBus);
        AD = PC + rel;
        PCP = (PC & 0xff00) | (AD & 0x00ff);
      } else {
        fetch();
        TP = 0;
      }
    } else if (T == 3) {
      if (PC != AD) {
        readFrom(PC); // Discarded.
        PCP = AD;
      } else {
        fetch();
        TP = 0;
      }
    } else if (T == 4) {
      fetch();
      TP = 0;
    }
  }

  else if (dc.instructionType == JMP) {
    if (dc.addressingMode == absolute) {
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        AD = dataBus;
        fetch();
        TP = -1;
      } else if (T == 0) {
        PC = (AD |= uint16_t(dataBus) << 8);
        fetch();
      }
    } else if (dc.addressingMode == absoluteIndirect) {
      if (T == 1) {
        fetch();
      } else if (T == 2) {
        AD = dataBus;
        fetch();
      } else if (T == 3) {
        AD |= uint16_t(dataBus) << 8;
        readFrom(AD);
      } else if (T == 4) {
        // Bug in most 6502: carry not propagated in summation.
        readFrom((AD & 0xff00) | ((AD + 1) & 0x00ff));
        AD = dataBus;
        TP = -1;
      } else if (T == 0) {
        PC = (AD |= uint16_t(dataBus) << 8);
        fetch();
      }
    }
  }

  else if (dc.instructionType == JSR) {
    if (T == 1) {
      fetch();
    } else if (T == 2) {
      AD = dataBus;
      readFrom(0x100 + S);
    } else if (T == 3) {
      writeTo(0x100 + S--, (PC >> 8) & 0xff);
    } else if (T == 4) {
      writeTo(0x100 + S--, PC & 0xff);
    } else if (T == 5) {
      fetch();
      TP = -1;
    } else if (T == 0) {
      PC = (AD |= uint16_t(dataBus) << 8);
      fetch();
    }
  }

  else if (dc.accessType == stack && dc.addressingMode == push) {
    if (T == 1) {
      readFrom(PC);
    } // Discarded.
    else if (T == 2) {
      uint8_t value = (dc.instructionType == PHA) ? A : getP(true);
      writeTo(0x100 + S, value);
      TP = -1;
    } else if (T == 0) {
      --S;
      fetch();
    }
  }

  else if (dc.accessType == stack && dc.addressingMode == pull) {
    if (T == 1) {
      readFrom(PC);
    } // Discarded.
    else if (T == 2) {
      readFrom(0x100 + S);
    } // Discarded.
    else if (T == 3) {
      readFrom(0x100 + ++S);
      TP = -1;
    } else if (T == 0) {
      if (dc.instructionType == PLA) {
        setNZ(A = dataBus);
      } else {
        P = dataBus;
      }
      fetch();
    }
  }

  else if (dc.instructionType == BRK) {
    uint16_t low, high;
    bool b = true; // b flag = software interrupt
    if (resetLine) {
      low = 0xfffc;
      high = 0xfffd;
      b = false;
    } else if (irqLine) {
      low = 0xfffe;
      high = 0xffff;
      b = false;
    } else if (nmiLine) {
      low = 0xfffa;
      high = 0xfffb;
      b = false;
    } else {
      low = 0xfffe;
      high = 0xffff;
    }
    //
    if (T == 1) {
      fetch();
    } // Discarded.
    if (T == 2 && !resetLine) {
      writeTo(0x100 + S, (PC >> 8) & 0xff);
    } else if (T == 2) {
      readFrom(0x100 + S);
    } else if (T == 3 && !resetLine) {
      writeTo(0x100 + uint8_t(S - 1), PC & 0xff);
    } else if (T == 3) {
      readFrom(0x100 + uint8_t(S - 1));
    } else if (T == 4 && !resetLine) {
      writeTo(0x100 + uint8_t(S - 2), getP(b));
    } else if (T == 4) {
      readFrom(0x100 + uint8_t(S - 2));
    } else if (T == 5) {
      S -= 3;
      readFrom(low);
      P[P.i] = true;
    } else if (T == 6) {
      AD = dataBus;
      readFrom(high);
      resetLine = false; // Abuse: this shoudl be the outer circuit business}
      TP = -1;
    } else if (T == 0) {
      PC = (AD |= uint16_t(dataBus) << 8);
      fetch();
    }
  }

  else if (dc.instructionType == RTS) {
    if (T == 1) {
      fetch();
    } // Discarded.
    else if (T == 2) {
      readFrom(0x100 + S++);
    } // Discarded.
    else if (T == 3) {
      readFrom(0x100 + S++);
    } else if (T == 4) {
      AD = dataBus;
      readFrom(0x100 + S);
    } else if (T == 5) {
      PC = (AD |= uint16_t(dataBus) << 8);
      fetch(); // Discarded.
      TP = -1;
    } else if (T == 0) {
      fetch();
    }
  }

  else if (dc.instructionType == RTI) {
    if (T == 1) {
      fetch();
    } // Discarded.
    else if (T == 2) {
      readFrom(0x100 + S++);
    } // Discarded.
    else if (T == 3) {
      readFrom(0x100 + S++);
    } else if (T == 4) {
      P = dataBus;
      readFrom(0x100 + S++);
    } else if (T == 5) {
      AD = dataBus;
      readFrom(0x100 + S);
      TP = -1;
    } else if (T == 0) {
      PC = (AD |= uint16_t(dataBus) << 8);
      fetch();
    }
  }

  ++TP;

  if (verbose) {
    cout << "M6502: @"
         // << setfill('0') << setw(5) << dec << cycles << " "
         << setfill('0') << setw(4) << hex << currentPC << " " << setfill('0') << setw(2) << hex << (int)IR << "/"
         << setfill('0') << setw(2) << dec << currentT << " " << setfill('0') << setw(2) << hex << (int)currentDataBus
         << "," << setfill('0') << setw(2) << hex << (int)dataBus << (RW ? " R" : " W") << setfill('0') << setw(4)
         << hex << (int)addressBus << "  " << setfill(' ') << left << setw(15) << dc << " " << setfill('0') << right
         << "[A:" << setw(2) << hex << (int)A << " X:" << setw(2) << hex << (int)X << " Y:" << setw(2) << hex << (int)Y
         << " " << (P[P.c] ? "C" : "c") << (P[P.z] ? "Z" : "z") << (P[P.i] ? "I" : "i") << (P[P.d] ? "D" : "d") << "-"
         << (P[P.v] ? "V" : "v") << (P[P.n] ? "N" : "n") << "]" << endl;
  }

  // One more cycle completed.
  numCycles++;
}
