//  jigo2600.cpp
//  Jigo2600 emulator Python 3 binding.

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include <Atari2600.hpp>
#include <M6502Disassembler.hpp>
#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sstream>
#include <string>

namespace py = pybind11;
using namespace jigo;
using namespace std;
using namespace pybind11::literals;
using json = nlohmann::json;

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

template <typename T> string to_json(const T& state) {
  json j;
  to_json(j, state);
  return j.dump();
}

template <typename T> void from_json(T& state, const string& str) {
  auto j = json::parse(str);
  from_json(j, state);
}

struct VideoFrame {
  VideoFrame(std::shared_ptr<const Atari2600> simulator, int time)
   : simulator(simulator) {
    assert(simulator);
    if (time >= 0) {
      argb = simulator->getTia()->getCurrentScreen();
    } else {
      argb = simulator->getTia()->getLastScreen();
    }
  }
  std::shared_ptr<const Atari2600> simulator;
  uint32_t const* argb;
};

struct CartridgeTypeMismatchException : public std::exception {
  virtual const char* what() const noexcept override {
    return "Cartridge type mismatch.";
  }
};

// ------------------------------------------------------------------
// Enumerations
// ------------------------------------------------------------------

enum class Atari2600StoppingReason {
  frameDone = Atari2600::StoppingReason::frameDone,
  breakpoint = Atari2600::StoppingReason::breakpoint,
  numClocksReached = Atari2600::StoppingReason::numCyclesReached,
};

vector<Atari2600StoppingReason> to_vector(Atari2600::StoppingReason const& r) {
  auto r_ = vector<Atari2600StoppingReason>();
  using in = Atari2600::StoppingReason;
  using out = Atari2600StoppingReason;
  if (r[in::frameDone]) r_.push_back(out::frameDone);
  if (r[in::breakpoint]) r_.push_back(out::breakpoint);
  if (r[in::numCyclesReached]) r_.push_back(out::numClocksReached);
  return r_;
}

// ------------------------------------------------------------------
// MARK: - Definitions
// ------------------------------------------------------------------

PYBIND11_MODULE(core, m) {
  m.doc() = "Jigo2600 emulator";

  static py::exception<CartridgeTypeMismatchException> exc(
      m, "CartridgeTypeMismatchException");

  // ----------------------------------------------------------------
  // MARK: M6502
  // ----------------------------------------------------------------

  py::class_<M6502State, shared_ptr<M6502State>> m6502state(m, "M6502State");
  m6502state.def_property("RW", &M6502State::getRW, &M6502State::getRW)
      .def_property("address_bus", &M6502State::getAddressBus, &M6502State::setAddressBus)
      .def_property("reset_line", &M6502State::getResetLine, &M6502State::setResetLine)
      .def_property(
          "data_bus",
          static_cast<std::uint8_t (M6502State::*)() const>(&M6502State::getDataBus),
          &M6502State::setDataBus)
      .def_property("irq_line", &M6502State::getIRQLine, &M6502State::setIRQLine)
      .def_property("nmi_line", &M6502State::getNMILine, &M6502State::setNMILine)
      .def_property("A", &M6502State::getA, &M6502State::setA)
      .def_property("X", &M6502State::getX, &M6502State::setX)
      .def_property("Y", &M6502State::getY, &M6502State::setY)
      .def_property("S", &M6502State::getS, &M6502State::setS)
      .def_property("P", &M6502State::getP, &M6502State::setP)
      .def_property("PC", &M6502State::getPC, &M6502State::setPC)
      .def_property("PCIR", &M6502State::getPCIR, &M6502State::setPCIR)
      .def_property("PCP", &M6502State::getPCP, &M6502State::setPCP)
      .def_property("IR", &M6502State::getIR, &M6502State::setIR)
      .def_property("AD", &M6502State::getAD, &M6502State::setAD)
      .def_property("ADD", &M6502State::getADD, &M6502State::setADD)
      .def_property("T", &M6502State::getT, &M6502State::setT)
      .def_property("TP", &M6502State::getT, &M6502State::setTP)
      .def_property("num_cycles", &M6502State::getNumCycles, &M6502State::setNumCycles);

  py::class_<M6502, shared_ptr<M6502>> m6502(m, "M6502", m6502state);
  m6502.def(py::init<>())
      .def("load_state", [](M6502& self, const M6502State& state) { self = state; })
      .def("reset", &M6502::reset)
      .def("cycle", &M6502::cycle)
      .def_property("verbose", &M6502::getVerbose, &M6502::setVerbose)
      .def_static(
          "decode",
          static_cast<M6502::InstructionTraits const& (*)(std::uint8_t)>(&M6502::decode))
      .def_static("decode_bytes",
                  static_cast<M6502::Instruction (*)(std::array<std::uint8_t, 3> const&)>(
                      &M6502::decode))
      .def_static("disassemble", [](py::buffer b) {
        auto info = b.request();
        // if (info.format != py::format_descriptor<uint8_t>::format()) {
        //   throw std::runtime_error("Incompatible format: expected a char buffer.") ;
        // }
        if (info.itemsize != 1 || info.ndim != 1 || info.strides[0] != 1) {
          throw std::runtime_error(
              "Incompatible format: expected a 1D linear buffer of bytes.");
        }
        auto begin = static_cast<uint8_t*>(info.ptr);
        auto end = begin + info.shape[0];
        return disassembleM6502memory(begin, end);
      });

#define it(x) .value(#x, M6502::InstructionType::x)
  py::enum_<M6502::InstructionType>(m6502, "InstructionType")
      // clang-format off
    it(ADC) it(AND) it(ASL) it(BCC) it(BCS)
    it(BEQ) it(BIT) it(BMI) it(BNE) it(BPL)
    it(BRK) it(BVC) it(BVS) it(CLC) it(CLD)
    it(CLI) it(CLV) it(CMP) it(CPX) it(CPY)
    it(DEC) it(DEX) it(DEY) it(EOR) it(INC)
    it(INX) it(INY) it(JMP) it(JSR) it(LDA)
    it(LDX) it(LDY) it(LSR) it(NOP) it(ORA)
    it(PHA) it(PHP) it(PLA) it(PLP) it(ROL)
    it(ROR) it(RTI) it(RTS) it(SBC) it(SEC)
    it(SED) it(SEI) it(STA) it(STX) it(STY)
    it(TAX) it(TAY) it(TSX) it(TXA) it(TXS)
    it(TYA)
    it(AHX) it(ALR) it(ANC) it(ARR) it(AXS)
    it(DCP) it(ISC) it(KIL) it(LAS) it(LAX)
    it(RLA) it(RRA) it(SAX) it(SHX) it(SHY)
    it(SLO) it(SRE) it(TAS) it(XAA) it(UNKNOWN)
      // clang-format on
      ;
#undef it

  py::enum_<M6502::AccessType>(m6502, "AccessType")
      .value("NO_ACCESS", M6502::AccessType::noAccess)
      .value("READ", M6502::AccessType::read)
      .value("WRITE", M6502::AccessType::write)
      .value("READ_WRITE", M6502::AccessType::readWrite)
      .value("BRANCH", M6502::AccessType::branch)
      .value("STACK", M6502::AccessType::stack)
      .value("MISC", M6502::AccessType::misc);

  py::enum_<M6502::AddressingMode>(m6502, "AddressingMode")
      .value("IMPLIED", M6502::AddressingMode::implied)
      .value("IMMEDIATE", M6502::AddressingMode::immediate)
      .value("ABSOLUTE", M6502::AddressingMode::absolute)
      .value("ABSOLUTE_INDEXED", M6502::AddressingMode::absoluteIndexed)
      .value("ABSOLUTE_INDIRECT", M6502::AddressingMode::absoluteIndirect)
      .value("ZERO_PAGE", M6502::AddressingMode::zeroPage)
      .value("ZERO_PAGE_INDEXED", M6502::AddressingMode::zeroPageIndexed)
      .value("ZERO_PAGE_INDEXED_INDIRECT", M6502::AddressingMode::zeroPageIndexedIndirect)
      .value("ZERO_PAGE_INDIRECT_INDEXED", M6502::AddressingMode::zeroPageIndirectIndexed)
      .value("PC_RELATIVE", M6502::AddressingMode::pcRelative)
      .value("PUSH", M6502::AddressingMode::push)
      .value("PULL", M6502::AddressingMode::pull);

  py::enum_<M6502::IndexingType>(m6502, "IndexingType")
      .value("NO_INDEXING", M6502::IndexingType::noIndexing)
      .value("X_INDEXING", M6502::IndexingType::XIndexing)
      .value("Y_INDEXING", M6502::IndexingType::YIndexing);

  py::class_<M6502::InstructionTraits> is(m6502, "InstructionTraits");
  is.def_readwrite("opcode", &M6502::InstructionTraits::opcode)
      .def_readwrite("length", &M6502::InstructionTraits::length)
      .def_readwrite("mnemonic", &M6502::InstructionTraits::mnemonic)
      .def_readwrite("instruction_type", &M6502::InstructionTraits::instructionType)
      .def_readwrite("addressing_mode", &M6502::InstructionTraits::addressingMode)
      .def_readwrite("access_type", &M6502::InstructionTraits::accessType)
      .def_readwrite("indexing_type", &M6502::InstructionTraits::indexingType)
      .def_readwrite("illegal", &M6502::InstructionTraits::illegal)
      .def_readwrite("add_to_A", &M6502::InstructionTraits::addToA)
      .def("__str__", [](const M6502::InstructionTraits& self) {
        std::ostringstream oss;
        oss << self;
        return oss.str();
      });

  py::class_<M6502::Instruction>(m6502, "Instruction", is)
      .def_readwrite("operand", &M6502::Instruction::operand)
      .def("__str__", [](const M6502::Instruction& self) {
        std::ostringstream oss;
        oss << self;
        return oss.str();
      });

  // ----------------------------------------------------------------
  // MARK: M6532
  // ----------------------------------------------------------------

  struct Memory {
    std::uint8_t* begin;
    std::uint8_t* end;
  };

  //  py::class_<Memory>
  py::class_<M6532State, shared_ptr<M6532State>> m6532state(m, "M6532State");
  m6532state.def(py::init<>())
      .def_property_readonly("ram",
                             [](M6532State& self) {
                               return py::buffer_info(
                                   begin(self.ram), self.ram.size(),
                                   py::format_descriptor<std::uint8_t>::format(),
                                   1,                 // ndims
                                   {self.ram.size()}, // dims
                                   {1}                // stides
                               );
                             })
      .def_readwrite("port_A", &M6532State::portA)
      .def_readwrite("port_B", &M6532State::portB)
      .def_readwrite("ORA", &M6532State::ORA)
      .def_readwrite("ORB", &M6532State::ORB)
      .def_readwrite("DDRA", &M6532State::DDRA)
      .def_readwrite("DDRB", &M6532State::DDRB)
      .def_readwrite("timer_interval", &M6532State::timerInterval)
      .def_readwrite("timer_counter", &M6532State::timerCounter)
      .def_readwrite("INTIM", &M6532State::INTIM)
      .def_readwrite("positive_edge_detect", &M6532State::positiveEdgeDetect)
      .def_readwrite("timer_interrupt", &M6532State::timerInterrupt)
      .def_readwrite("timer_interrupt_enabled", &M6532State::timerInterruptEnabled)
      .def_readwrite("pa7_interrupt", &M6532State::pa7Interrupt)
      .def_readwrite("pa7_interrupt_enabled", &M6532State::pa7InterruptEnabled)
      .def_static("decode_address", &M6532State::decodeAddress);

  py::enum_<M6532::Register>(m6532state, "Register")
      .value("RAM", M6532::Register::RAM)
      .value("ORA", M6532::Register::ORA)
      .value("DDRA", M6532::Register::DDRA)
      .value("ORB", M6532::Register::ORB)
      .value("DDRB", M6532::Register::DDRB)
      .value("EDGCTL", M6532::Register::EDGCTL)
      .value("TIM1T", M6532::Register::TIM1T)
      .value("TIM8T", M6532::Register::TIM8T)
      .value("TIM64T", M6532::Register::TIM64T)
      .value("T1024T", M6532::Register::T1024T)
      .value("INTIM", M6532::Register::INTIM)
      .value("INSTAT", M6532::Register::INSTAT);

  py::class_<M6532, shared_ptr<M6532>> m6532(m, "M6532", m6532state);
  m6532.def(py::init<>())
      .def("load_state", [](M6532& self, const M6532State& state) { self = state; })
      .def("write_port_A", &M6532::writePortA)
      .def("write_port_B", &M6532::writePortB)
      .def("reset", &M6532::reset)
      .def("cycle",
           [](M6532& self, bool CS, bool RSnot, bool RW, std::uint16_t address,
              std::uint8_t data) {
             auto portChanged = self.cycle(CS, RSnot, RW, address, data);
             return std::make_tuple(portChanged, data);
           })
      .def_property_readonly("irq", &M6532::getIRQ)
      .def_property("verbose", &M6532::getVerbose, &M6532::setVerbose);

  // ----------------------------------------------------------------
  // MARK: - TIA
  // ----------------------------------------------------------------

  py::class_<TIAState, shared_ptr<TIAState>> tiaState(m, "TIAState");
  tiaState.def(py::init<>())
      .def_readwrite("beam_x", &TIAState::beamX)
      .def_readwrite("beam_y", &TIAState::beamY)
      .def_static("decode_address", &TIAState::decodeAddress);

  py::class_<TIA, shared_ptr<TIA>> tia(m, "TIA", tiaState);
  tia.def(py::init<>()).def_readwrite("num_cycles", &TIA::numCycles);

  py::enum_<TIA::VideoStandard>(tiaState, "VideoStandard")
      .value("NTSC", TIA::VideoStandard::NTSC)
      .value("PAL", TIA::VideoStandard::PAL)
      .value("SECAM", TIA::VideoStandard::SECAM);

  py::enum_<TIA::Register>(tiaState, "Register")
      .value("VSYNC", TIA::Register::VSYNC)
      .value("VBLANK", TIA::Register::VBLANK)
      .value("WSYNC", TIA::Register::WSYNC)
      .value("RSYNC", TIA::Register::RSYNC)
      .value("NUSIZ0", TIA::Register::NUSIZ0)
      .value("NUSIZ1", TIA::Register::NUSIZ1)
      .value("COLUP0", TIA::Register::COLUP0)
      .value("COLUP1", TIA::Register::COLUP1)
      .value("COLUPF", TIA::Register::COLUPF)
      .value("COLUBK", TIA::Register::COLUBK)
      .value("CTRLPF", TIA::Register::CTRLPF)
      .value("REFP0", TIA::Register::REFP0)
      .value("REFP1", TIA::Register::REFP1)
      .value("PF0", TIA::Register::PF0)
      .value("PF1", TIA::Register::PF1)
      .value("PF2", TIA::Register::PF2)
      .value("RESP0", TIA::Register::RESP0)
      .value("RESP1", TIA::Register::RESP1)
      .value("RESM0", TIA::Register::RESM0)
      .value("RESM1", TIA::Register::RESM1)
      .value("RESBL", TIA::Register::RESBL)
      .value("AUDC0", TIA::Register::AUDC0)
      .value("AUDC1", TIA::Register::AUDC1)
      .value("AUDF0", TIA::Register::AUDF0)
      .value("AUDF1", TIA::Register::AUDF1)
      .value("AUDV0", TIA::Register::AUDV0)
      .value("AUDV1", TIA::Register::AUDV1)
      .value("GRP0", TIA::Register::GRP0)
      .value("GRP1", TIA::Register::GRP1)
      .value("ENAM0", TIA::Register::ENAM0)
      .value("ENAM1", TIA::Register::ENAM1)
      .value("ENABL", TIA::Register::ENABL)
      .value("HMP0", TIA::Register::HMP0)
      .value("HMP1", TIA::Register::HMP1)
      .value("HMM0", TIA::Register::HMM0)
      .value("HMM1", TIA::Register::HMM1)
      .value("HMBL", TIA::Register::HMBL)
      .value("VDELP0", TIA::Register::VDELP0)
      .value("VDELP1", TIA::Register::VDELP1)
      .value("VDELBL", TIA::Register::VDELBL)
      .value("RESMP0", TIA::Register::RESMP0)
      .value("RESMP1", TIA::Register::RESMP1)
      .value("HMOVE", TIA::Register::HMOVE)
      .value("HMCLR", TIA::Register::HMCLR)
      .value("CXCLR", TIA::Register::CXCLR)
      .value("CXM0P", TIA::Register::CXM0P)
      .value("CXM1P", TIA::Register::CXM1P)
      .value("CXP0FB", TIA::Register::CXP0FB)
      .value("CXP1FB", TIA::Register::CXP1FB)
      .value("CXM0FB", TIA::Register::CXM0FB)
      .value("CXM1FB", TIA::Register::CXM1FB)
      .value("CXBLPF", TIA::Register::CXBLPF)
      .value("CXPPMM", TIA::Register::CXPPMM)
      .value("INPT0", TIA::Register::INPT0)
      .value("INPT1", TIA::Register::INPT1)
      .value("INPT2", TIA::Register::INPT2)
      .value("INPT3", TIA::Register::INPT3)
      .value("INPT4", TIA::Register::INPT4)
      .value("INPT5", TIA::Register::INPT5)
      .value("NA1", TIA::Register::NA1)
      .value("NA2", TIA::Register::NA2)
      .value("VOID", TIA::Register::VOID);

  // ----------------------------------------------------------------
  // MARK: Cartridge
  // ----------------------------------------------------------------

  py::class_<Atari2600CartridgeState, shared_ptr<Atari2600CartridgeState>> cartState(
      m, "CartridgeState");
  cartState.def_property_readonly("type", &Atari2600CartridgeState::getType);

  py::class_<Atari2600Cartridge, shared_ptr<Atari2600Cartridge>> cart(m, "Cartridge",
                                                                      cartState);
  cart.def_property_readonly("size", &Atari2600Cartridge::getSize)
      .def_property("verbosity", &Atari2600Cartridge::getVerbosity,
                    &Atari2600Cartridge::setVerbosity)
      .def("to_json", [](const Atari2600Cartridge& self) {
        json j;
        self.serialize(j);
        return j.dump();
      });

  py::enum_<Atari2600Cartridge::Type>(cart, "Type")
      .value("UNKNOWN", Atari2600Cartridge::Type::unknown)
      .value("STANDARD", Atari2600Cartridge::Type::standard)
      .value("S2K", Atari2600Cartridge::Type::S2K)
      .value("S4K", Atari2600Cartridge::Type::S4K)
      .value("S8K", Atari2600Cartridge::Type::S8K)
      .value("S12K", Atari2600Cartridge::Type::S12K)
      .value("S16K", Atari2600Cartridge::Type::S16K)
      .value("S32K", Atari2600Cartridge::Type::S32K)
      .value("S2K128R", Atari2600Cartridge::Type::S2K128R)
      .value("S4K128R", Atari2600Cartridge::Type::S4K128R)
      .value("S12K128R", Atari2600Cartridge::Type::S12K128R)
      .value("S16K128R", Atari2600Cartridge::Type::S16K128R)
      .value("S32K128R", Atari2600Cartridge::Type::S32K128R)
      .value("E0", Atari2600Cartridge::Type::E0)
      .value("FE", Atari2600Cartridge::Type::FE)
      .value("F0", Atari2600Cartridge::Type::F0);

  m.def("make_cartridge_from_bytes",
        [](const py::bytes& data, Atari2600Cartridge::Type type) {
          auto str = string(data);
          return makeCartridgeFromBytes(&*begin(str), &*end(str), type);
        },
        "Make a new Atari2600 cartridge from a binary blob.", "bytes"_a,
        "type"_a = Atari2600Cartridge::Type::unknown);

  py::class_<Atari2600State, shared_ptr<Atari2600State>>(m, "Atari2600State")
      .def(py::init<>())
      .def("to_json", [](const Atari2600State& self) -> string { return to_json(self); })
      .def("from_json",
           [](Atari2600State& self, string const& str) { from_json(self, str); });

  // ----------------------------------------------------------------
  // MARK: Emulator
  // ----------------------------------------------------------------

  py::class_<Atari2600, shared_ptr<Atari2600>> atari2600(m, "Atari2600");
  atari2600.def(py::init<>())
      .def("cycle",
           [](Atari2600& self, size_t max_num_cpu_cycles) {
             auto r = self.cycle(max_num_cpu_cycles);
             return py::make_tuple(to_vector(r), max_num_cpu_cycles);
           })
      .def("get_current_frame",
           [](shared_ptr<const Atari2600> self) { return VideoFrame(self, 0); })
      .def("get_last_frame",
           [](shared_ptr<const Atari2600> self) { return VideoFrame(self, -1); })
      .def("get_audio_samples",
           [](Atari2600& self, py::buffer b, double nominalRate) {
             auto info = b.request();
             // if (info.format != py::format_descriptor<uint8_t>::format()) {
             //   throw std::runtime_error("Incompatible format: expected a char buffer.")
             //   ;
             // }
             if (info.itemsize != 1 || info.ndim != 1 || info.strides[0] != 1) {
               throw std::runtime_error(
                   "Incompatible format: expected a 1D linear buffer of bytes.");
             }
             auto begin = static_cast<uint8_t*>(info.ptr);
             auto end = begin + info.shape[0];
             self.getTia()->getSound(0).resample(begin, end, false, nominalRate);
             self.getTia()->getSound(1).resample(begin, end, true, nominalRate);
           })
      .def_property("video_standard", &Atari2600::getVideoStandard,
                    &Atari2600::setVideoStandard)
      .def_property("cartridge", &Atari2600::getCartridge, &Atari2600::setCartridge)
      .def_property_readonly("frame_number", &Atari2600::getFrameNumber)
      .def_property_readonly("color_cycle_number", &Atari2600::getColorCycleNumber)
      .def_property_readonly("color_clock_rate", &Atari2600::getColorClockRate)
      .def("reset", &Atari2600::reset)
      .def("load_state",
           [](Atari2600& self, const Atari2600State& state) {
             auto error = self.loadState(state);
             switch (error) {
             case Atari2600Error::cartridgeTypeMismatch:
               throw CartridgeTypeMismatchException();
             default: break;
             }
           })
      .def("save_state", &Atari2600::saveState)
      .def("make_state", &Atari2600::makeState)
      .def("get_panel", &Atari2600::getPanel)
      .def("set_panel", &Atari2600::setPanel)
      .def("set_joystick", &Atari2600::setJoystick)
      .def("set_paddle", &Atari2600::setPaddle)
      .def("virtualize_address", &Atari2600::virtualizeAddress)
      .def("set_breakpoint", &Atari2600::setBreakPoint)
      .def("clear_breakpoint", &Atari2600::clearBreakPoint)
      .def("set_breakpoint_on_next_instruction",
           &Atari2600::setBreakPointOnNextInstruction)
      .def("clear_break_on_next_instruction",
           &Atari2600::clearBreakPointOnNextInstruction)
      .def_property_readonly("cpu", [](const Atari2600& self) { return self.getCpu(); })
      .def_property_readonly("pia", [](const Atari2600& self) { return self.getPia(); })
      .def_property_readonly("tia", [](const Atari2600& self) { return self.getTia(); })
      //    .def("peek_virtual_address", &)
      ;

  py::enum_<Atari2600StoppingReason>(atari2600, "StoppingReason")
      .value("FRAME_DONE", Atari2600StoppingReason::frameDone)
      .value("BREAKPOINT", Atari2600StoppingReason::breakpoint)
      .value("NUM_CLOCKS_REACHED", Atari2600StoppingReason::numClocksReached);

  py::class_<VideoFrame, unique_ptr<VideoFrame>>(atari2600, "VideoFrame",
                                                 py::buffer_protocol())
      .def_property_readonly("width", [](py::object self) { return TIA::screenWidth; })
      .def_property_readonly("height", [](py::object self) { return TIA::screenHeight; })
      .def_buffer([](const VideoFrame& self) -> py::buffer_info {
        // TODO: pybind11 is going to support read only buffers and these should be used
        // here.
        return py::buffer_info(
            const_cast<uint32_t*>(self.argb), sizeof(char),
            py::format_descriptor<const char>::format(), 3,
            {TIA::screenHeight, TIA::screenWidth, 4},
            {sizeof(char) * 4 * TIA::screenWidth, sizeof(char) * 4, sizeof(char)});
      });

  // ----------------------------------------------------------------
  // MARK: Console panel
  // ----------------------------------------------------------------

  enum class Atari2600PanelSwitch : int {
    reset = Atari2600::Panel::reset,
    select = Atari2600::Panel::select,
    colorMode = Atari2600::Panel::colorMode,
    difficultyLeft = Atari2600::Panel::difficultyLeft,
    difficultyRight = Atari2600::Panel::difficultyRight,
  };

  py::class_<Atari2600::Panel> panel(atari2600, "Panel");
  panel.def(py::init<>())
      .def("get_switch",
           [](const Atari2600::Panel& self, Atari2600PanelSwitch sw) {
             return static_cast<bool>(self[static_cast<int>(sw)]);
           })
      .def("set_switch", [](Atari2600::Panel& self, Atari2600PanelSwitch sw,
                            bool value) { self[static_cast<int>(sw)] = value; })
      .def("get_value",
           [](const Atari2600::Panel& self) { return static_cast<int>(self.to_ulong()); })
      .def("set_value",
           [](Atari2600::Panel& self, int value) { self = Atari2600::Panel(value); });

  py::enum_<Atari2600PanelSwitch>(panel, "Switch")
      .value("RESET", Atari2600PanelSwitch::reset)
      .value("SELECT", Atari2600PanelSwitch::select)
      .value("COLOR_MODE", Atari2600PanelSwitch::colorMode)
      .value("DIFFICULTY_LEFT", Atari2600PanelSwitch::difficultyLeft)
      .value("DIFFICULTY_RIGHT", Atari2600PanelSwitch::difficultyRight);

  // ----------------------------------------------------------------
  // MARK: Joystick
  // ----------------------------------------------------------------

  enum class Atari2600JoystickSwitch {
    fire = Atari2600::Joystick::fire,
    up = Atari2600::Joystick::up,
    down = Atari2600::Joystick::down,
    left = Atari2600::Joystick::left,
    right = Atari2600::Joystick::right,
  };

  py::class_<Atari2600::Joystick> joystick(atari2600, "Joystick");
  joystick.def(py::init<>())
      .def("get_switch",
           [](const Atari2600::Joystick& self, Atari2600JoystickSwitch sw) {
             return static_cast<bool>(self[static_cast<int>(sw)]);
           })
      .def("set_switch", [](Atari2600::Joystick& self, Atari2600JoystickSwitch sw,
                            bool value) { self[static_cast<int>(sw)] = value; })
      .def("get_value",
           [](const Atari2600::Joystick& self) {
             return static_cast<int>(self.to_ulong());
           })
      .def("set_value", [](Atari2600::Joystick& self,
                           int value) { self = Atari2600::Joystick(value); })
      .def("reset_directions", [](Atari2600::Joystick& self) {
        self[Atari2600::Joystick::up] = false;
        self[Atari2600::Joystick::down] = false;
        self[Atari2600::Joystick::left] = false;
        self[Atari2600::Joystick::right] = false;
      });

  py::enum_<Atari2600JoystickSwitch>(joystick, "Switch")
      .value("FIRE", Atari2600JoystickSwitch::fire)
      .value("UP", Atari2600JoystickSwitch::up)
      .value("DOWN", Atari2600JoystickSwitch::down)
      .value("LEFT", Atari2600JoystickSwitch::left)
      .value("RIGHT", Atari2600JoystickSwitch::right);

  // ----------------------------------------------------------------
  // MARK: Paddle
  // ----------------------------------------------------------------

  py::class_<Atari2600::Paddle>(atari2600, "Paddle")
      .def(py::init<>())
      .def_readwrite("fire", &Atari2600::Paddle::fire)
      .def_readwrite("angle", &Atari2600::Paddle::angle);
}
