// M6502Disassembler.cpp

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "M6502Disassembler.hpp"
#include <limits>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <functional>

using namespace std ;
using namespace sim ;

constexpr auto inf = numeric_limits<float>::infinity() ;

struct TaggerState
{
  TaggerState() : instruction {0}, probability {0}, transition {0} { }
  std::uint8_t instruction [3] ;
  float probability [5] ;
  int transition [5] ;
} ;

/** Search for M6502 instruciton in a memory block.
 
 This function scans a memory block taggin bytes with their most likeley
 interpretation, as either data or the first, second, or third byte
 of an instruction.

 The tagger works using Viterbi decoding. Let q1...qT be the list of
 tags (byte types) and let b1...bT the bite values. We define a Markof
 probability model p(q1...qT,b1...bT). Then Viterbi's decoding forward
 pass is given by the recursion:

  S(qT) = max_{q1...qT-1} p(q1...qT,b1...bT)
  = max_{qT-1} p(bT|qT)p(qT|qT-1) max_{q1...qT-2} p(q1...qT-1,b1...bT-1)
  = max_{qT-1} p(bT|qT)p(qT|qT-1) S(qT-1)

  S(q2) = p(b1|q1)p(q1).

  The funcition S(qT) is maximized to get the tag for the last
  byte. Then, this is propagated backward to get all other tags.
 */

vector<M6502ByteType>
sim::tagM6502Memory(std::uint8_t const * begin, std::uint8_t const * end)
{
  auto const length = end - begin ;
  auto current = begin ;
  vector<TaggerState> states(length + 1) ;
  vector<M6502ByteType> tags(length + 1) ;

  if (length == 0) { return tags ; }

  // Viterbi forward pass.
  for (current = begin ; current != end + 1 ; ++current) {
    auto& state = states.at(current - begin) ;

    // Decode current byte (note that we could be one over for eof).
    if (current == end) {
      // Observation: p(bt|qt)
      fill(std::begin(state.probability),std::end(state.probability),-inf) ;
      state.probability[End] = 1 ;
      state.instruction[0] = 0 ; // na
    }
    else {
      std::uint8_t byte = *current ;
      state.instruction[0] = byte ;
      auto const& i0 = M6502::decode(state.instruction[0]) ;
      // Observation: p(bt|qt)
      for (int q = 0 ; q < 5 ; ++q) {
        if (q == Instruction0) {
          // Probability of observing a given byte value given that the
          // current byte is the beginning of an instruction. Note that
          // the state is *implicitly* multiplexed to represent specific opcodes,
          // and only the slot actually corresponding to this opcode needs to be
          // represented. Thus this probability is 1 for any valid opcode.
          state.probability[q] =
            (i0.instructionType == M6502::UNKNOWN ||
             i0.instructionType == M6502::KIL) ? -inf : 0 ;
        } else if (q == End) {
          state.probability[q] = - inf ;
        } else {
          // For any other byte types, all byte values are equally probable.
          state.probability[q] = log2(1/256.0f) ;
        }
      }
    }
    //for(auto& i : state.probability) cout << setw(10) << i ; cout << endl ;

    constexpr float numValidOpcodes = 240.0; //todo:check.

    // First byte: compute S(q1) = p(b1|q1) x p(q1).
    if (current == begin) {
      // We give a 50% prior probability that the first byte is Data.
      // Otherwise the first byte is one of the valid opcodes with
      // uniform probability (note that the states Instrunction0,1,2
      // are *implicitly* code specific).
      // Todo: allow the block to begin half-way through an instruction.
      for (int q = 0 ; q < 5 ; ++q) {
        switch (q) {
        case Data: state.probability[q] += log2(0.5f) ; break ;
        case Instruction0: state.probability[q] += log2(0.5f/numValidOpcodes) ; break ;
        default: state.probability[q] = -inf ; break ;
        }
      }
      continue ;
    }

    // Transitions from a previous byte:
    // compute S(qt) =  max_{qt-1} p(bt|qt) p(qt|qt-1) S(qt-1).
    auto const& statep = states.at(current - begin - 1) ;
    state.instruction[2] = statep.instruction[1] ;
    state.instruction[1] = statep.instruction[0] ;
    auto const& i1 = M6502::decode(state.instruction[1]) ;
    auto const& i2 = M6502::decode(state.instruction[2]) ;
    auto probs = std::array<float,5>{} ;

    // Given q=qt and qp=qt-1, compute max_{qp} p(q|qp) S(qp).
    for (int q = 0 ; q < 5 ; ++q) {
      for (int qp = 0 ; qp < 5 ; ++qp) {
        // Get transition probability p(qt|qt-1)=p(q|qp).
        float p = -inf ;
        if (qp == End) {
          if (q == End) { p = 0 ; }
        }
        else if (qp == Data) {
          if (q == End) { p = log2(0.0001f) ; }
          else if (q == Data) { p = log2(0.99f) ; }
          else if (q == Instruction0) { p = log2(0.0099f / numValidOpcodes) ; }
        }
        else  {
          if (qp == Instruction0 && i1.length >= 2) {
            if (q == Instruction1) { p = 0 ; }
          }
          else if (qp == Instruction1 && i2.length >= 3) {
            if (q == Instruction2) { p = 0 ; }
          }
          else {
            if (q == End) { p = log2(0.0001f) ; }
            else if (q == Data) { p = log2(0.0099f) ; }
            else if (q == Instruction0) { p = log2(0.99 / numValidOpcodes) ; }
          }
        }
        // Compute  S(qt-1) p(bt|qt) x p(qt|qt-1).
        probs[qp] = statep.probability[qp] + p ;
      }
      auto m = max_element(probs.begin(),probs.end()) ;
      state.transition[q] = (int)(m - probs.begin()) ;
      state.probability[q] += *m ;
    }
    //for(auto& i : state.probability) cout << setw(10) << i ; cout << endl ;
    //for(auto& i : state.transition) cout << setw(10) << i ; cout << endl ;
  } // Next byte.

  // Max out last element.
  {
    auto probs = std::array<float,5>{} ;
    for (int q = 0 ; q < 5 ; ++q) { probs[q] = states.back().probability[q] ; }
    auto m = max_element(probs.begin(),probs.end()) ;
    tags.back() = (M6502ByteType)(m - probs.begin()) ;
  }

  // Progpagate backward.
  auto tag = tags.end() - 1 ;
  for (auto state = states.rbegin() ; state != states.rend() - 1 ; ++state) {
    auto z = *tag-- ;
    *tag = (M6502ByteType)state->transition[z] ;
  }

  return tags ;
}

M6502Disassembly
sim::disassembleM6502memory(std::uint8_t const * begin, std::uint8_t const * end)
{
  auto tags = sim::tagM6502Memory(begin,end) ;
  auto lines = M6502Disassembly() ;
  for (auto curr = begin ; curr < end ; ) {
    array<uint8_t,3> threeBytes {
      *curr,
      *min(curr+1,end-1),
      *min(curr+2,end-1)} ;

    M6502::Instruction ins = M6502::decode(threeBytes) ;
    auto line = pair<uint16_t,M6502::Instruction>
    (curr - begin, ins) ;

    if (tags[curr - begin] == Instruction0) {
      // Valid instruction.
      curr += ins.length ;
      lines.push_back(line) ;
    } else {
      // Invalid instruction or data.
      ins.instructionType = M6502::UNKNOWN ;
      line.second = ins ;
      lines.push_back(line) ;
      curr += 1 ;
    }
  }
  return lines ;
}


