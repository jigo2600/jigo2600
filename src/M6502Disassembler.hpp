// M6502Disassembler.hpp

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#ifndef M6502Disassembler_hpp
#define M6502Disassembler_hpp

#include "M6502.hpp"
#include <iostream>

namespace sim {

  enum M6502ByteType {
    End = 0,
    Data = 1,
    Instruction0 = 2,
    Instruction1,
    Instruction2,
  } ;

  using M6502Disassembly = std::vector<std::pair<std::uint16_t, M6502::Instruction>> ;
  
  M6502Disassembly disassembleM6502memory(std::uint8_t const * begin, std::uint8_t const * end) ;
  std::vector<M6502ByteType> tagM6502Memory(std::uint8_t const * begin, std::uint8_t const * end) ;
}

#endif /* M6502Disassembler_hpp */
