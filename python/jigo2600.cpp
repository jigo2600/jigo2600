//  jigo2600.cpp
//  Jigo2600 emulator Python 3 binding.

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <Atari2600.hpp>
#include <string>
#include <cstdint>

namespace py = pybind11 ;
using namespace sim ;
using namespace std ;
using namespace pybind11::literals ;
using json = nlohmann::json ;

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

template<typename T>
string to_json(const T& state) {
  json j ;
  to_json(j, state) ;
  return j.dump() ;
}

template<typename T>
void from_json(T& state, const string& str) {
  auto j = json::parse(str) ;
  from_json(j, state) ;
}

struct VideoFrame {
  VideoFrame(std::shared_ptr<const Atari2600> simulator, int time)
  : simulator(simulator) { 
    assert(simulator) ;
    if (time >= 0) {
      argb = simulator->getCurrentScreen() ;
    } else {
      argb = simulator->getLastScreen() ;
    }
  }
  std::shared_ptr<const Atari2600> simulator ;
  uint32_t * argb ;
} ;


struct CartridgeTypeMismatchException : public std::exception {
    virtual const char * what() const noexcept override { return "Cartridge type mismatch."; }
} ;

// ------------------------------------------------------------------
// Enumerations
// ------------------------------------------------------------------

enum class Atari2600StoppingReason {
  frameDone = Atari2600::StoppingReason::frameDone,
  breakpoint = Atari2600::StoppingReason::breakpoint,
  numClocksReached = Atari2600::StoppingReason::numClocksReached,
} ;

vector<Atari2600StoppingReason> to_vector(Atari2600::StoppingReason const& r) {
  auto r_ = vector<Atari2600StoppingReason>() ;
  using in = Atari2600::StoppingReason ;
  using out = Atari2600StoppingReason ;
  if (r[in::frameDone])        r_.push_back(out::frameDone) ; 
  if (r[in::breakpoint])       r_.push_back(out::breakpoint) ;
  if (r[in::numClocksReached]) r_.push_back(out::numClocksReached) ;
  return r_ ;
}

enum class Atari2600Switch : int {
  reset           = static_cast<int>(Atari2600::Switch::reset),
  select          = static_cast<int>(Atari2600::Switch::select),
  colorMode       = static_cast<int>(Atari2600::Switch::colorMode),
  difficultyLeft  = static_cast<int>(Atari2600::Switch::difficultyLeft),
  difficultyRight = static_cast<int>(Atari2600::Switch::difficultyRight),
} ;

enum class Atari2600Joystick {
  fire  = Atari2600::Joystick::fire,
  up    = Atari2600::Joystick::up,
  down  = Atari2600::Joystick::down,
  left  = Atari2600::Joystick::left,
  right = Atari2600::Joystick::right,
} ;

vector<Atari2600Joystick> to_vector(Atari2600::Joystick const j) {
  using in = Atari2600::Joystick ;
  using out = Atari2600Joystick ;
  auto j_ = vector<out>() ;
  if (j[in::fire])  j_.push_back(out::fire) ;
  if (j[in::up])    j_.push_back(out::up) ;
  if (j[in::down])  j_.push_back(out::down) ;
  if (j[in::left])  j_.push_back(out::left) ;
  if (j[in::right]) j_.push_back(out::right) ;
  return j_ ;
}

Atari2600::Joystick from_vector(vector<Atari2600Joystick> const& j) {
  Atari2600::Joystick j_ {} ;
  for (auto x : j) { j_[static_cast<size_t>(x)] = true ; }
  return j_ ;
}

// ------------------------------------------------------------------
// Definitions
// ------------------------------------------------------------------

PYBIND11_MODULE(jigo2600, m) {
  m.doc() = "Atari2600 simulator" ;

  static py::exception<CartridgeTypeMismatchException> exc(m, "CartridgeTypeMismatchException");

  // Cartridge
  py::class_<Atari2600Cartridge, shared_ptr<Atari2600Cartridge> > cart (m, "Cartridge") ;
  cart
    .def_property_readonly("type",
      [](const Atari2600Cartridge& self) { return self.getType() ; })
    .def_property_readonly("size",
      [](const Atari2600Cartridge& self) { return self.getSize() ; })
    .def_property("verbosity", 
      [](const Atari2600Cartridge& self) { return self.getVerbosity() ; }, 
      [](Atari2600Cartridge& self, int x) { self.setVerbosity(x) ; })
    .def("to_json",
      [](const Atari2600Cartridge& self) {
        json j ;
        self.serialize(j) ;
        return j.dump() ;
      })
    ;
  
  py::enum_<Atari2600Cartridge::Type>(cart, "Type")
    .value("UNKNOWN",  Atari2600Cartridge::Type::unknown)
    .value("STANDARD", Atari2600Cartridge::Type::standard)
    .value("S2K",      Atari2600Cartridge::Type::S2K)
    .value("S4K",      Atari2600Cartridge::Type::S4K)
    .value("S8K",      Atari2600Cartridge::Type::S8K)
    .value("S12K",     Atari2600Cartridge::Type::S12K)
    .value("S16K",     Atari2600Cartridge::Type::S16K)
    .value("S32K",     Atari2600Cartridge::Type::S32K)
    .value("S2K128R",  Atari2600Cartridge::Type::S2K128R)
    .value("S4K128R",  Atari2600Cartridge::Type::S4K128R)
    .value("S12K128R", Atari2600Cartridge::Type::S12K128R)
    .value("S16K128R", Atari2600Cartridge::Type::S16K128R)
    .value("S32K128R", Atari2600Cartridge::Type::S32K128R)
    .value("E0",       Atari2600Cartridge::Type::E0)
    .value("FE",       Atari2600Cartridge::Type::FE)
    .value("F0",       Atari2600Cartridge::Type::F0)
    ;

  m.def("make_cartridge_from_bytes", 
    [](const py::bytes &data, Atari2600Cartridge::Type type) {
      auto str = string(data) ;
      return makeCartridgeFromBytes(&*begin(str), &*end(str), type) ;
    },
    "Make a new Atari2600 cartridge from a binary blob.",
    "bytes"_a,
    "type"_a=Atari2600Cartridge::Type::unknown) ;

  py::class_<Atari2600State, shared_ptr<Atari2600State> >(m, "Atari2600State")
    .def(py::init<>())
    .def("to_json", [](const Atari2600State& self) -> string {
      return to_json(self) ;
    })
    .def("from_json", [](Atari2600State& self, string const& str) {
      from_json(self, str) ;
    })
    ;

  // Simulator
  py::class_<Atari2600, shared_ptr<Atari2600> > atari2600 (m, "Atari2600") ;
  atari2600
    .def(py::init<>())
    .def("cycle", [](Atari2600 & self, size_t max_num_cpu_cycles) {
      auto r = self.cycle(max_num_cpu_cycles) ;
      return py::make_tuple(to_vector(r), max_num_cpu_cycles) ;
    })
    .def("get_current_frame", [](shared_ptr<const Atari2600> self) {
      return VideoFrame(self, 0) ;
    })
    .def("get_last_frame", [](shared_ptr<const Atari2600> self) {
      return VideoFrame(self, -1) ;
    })
    .def("get_audio_samples", [](Atari2600 &self, py::buffer b, double nominalRate){
      auto info = b.request() ;
      // if (info.format != py::format_descriptor<uint8_t>::format()) {
      //   throw std::runtime_error("Incompatible format: expected a char buffer.") ;
      // }
      if (info.itemsize != 1 || info.ndim != 1 || info.strides[0] != 1) {
        throw std::runtime_error("Incompatible format: expected a 1D linear buffer of bytes.") ;
      }
      auto begin = static_cast<uint8_t*>(info.ptr) ;
      auto end = begin + info.shape[0] ;
      self.getTia()->sound[0].resample(begin, end, false, nominalRate) ;
      self.getTia()->sound[1].resample(begin, end, true, nominalRate) ;
    })
    .def_property("video_standard", &Atari2600::getVideoStandard, &Atari2600::setVideoStandard)
    .def_property("cartridge", &Atari2600::getCartridge, &Atari2600::setCartridge)
    .def("set_joystick", [](Atari2600 &self, int num, vector<Atari2600Joystick> const& j) {
      auto j_ = from_vector(j) ;
      self.setJoystick(num, j_) ;
    })
    .def_property_readonly("frame_number", &Atari2600::getFrameNumber)
    .def_property_readonly("color_cycle_number", &Atari2600::getColorCycleNumber)
    .def_property_readonly("color_clock_rate", &Atari2600::getColorClockRate)
    .def("reset", &Atari2600::reset)  
    .def("load_state", [](Atari2600& self, const Atari2600State& state) {
      auto error = self.loadState(state) ;
      switch (error) {
        case Atari2600Error::cartridgeTypeMismatch: throw  CartridgeTypeMismatchException() ;
        default: break ;
      }      
    })
    .def("save_state", &Atari2600::saveState)
    .def("make_state", &Atari2600::makeState)
    .def("get_switch", [](Atari2600 &self, Atari2600Switch sw) {
      return self.getSwitch(static_cast<Atari2600::Switch>(sw)) ;
    })
    .def("set_switch", [](Atari2600 &self, Atari2600Switch sw, bool x) {
      self.setSwitch(static_cast<Atari2600::Switch>(sw), x) ;
    })
    ;

  py::enum_<Atari2600StoppingReason>(atari2600, "StoppingReason")
    .value("FRAME_DONE", Atari2600StoppingReason::frameDone)
    .value("BREAKPOINT", Atari2600StoppingReason::breakpoint)
    .value("NUM_CLOCKS_REACHED", Atari2600StoppingReason::numClocksReached)
    ;

  py::enum_<Atari2600Joystick>(atari2600, "Joystick")
    .value("FIRE",  Atari2600Joystick::fire)
    .value("UP",    Atari2600Joystick::up)
    .value("DOWN",  Atari2600Joystick::down)
    .value("LEFT",  Atari2600Joystick::left)
    .value("RIGHT", Atari2600Joystick::right)
    ;

  py::enum_<Atari2600Switch>(atari2600, "Switch")
    .value("SELECT",           Atari2600Switch::select)
    .value("RESET",            Atari2600Switch::reset)
    .value("COLOR_MODE",       Atari2600Switch::colorMode)
    .value("DIFFICULTY_LEFT",  Atari2600Switch::difficultyLeft)
    .value("DIFFICULTY_RIGHT", Atari2600Switch::difficultyRight)
    ;

  py::enum_<TIAState::VideoStandard>(atari2600, "VideoStandard")
    .value("NTSC", TIAState::VideoStandard::NTSC)
    .value("PAL", TIAState::VideoStandard::PAL)
    .value("SECAM", TIAState::VideoStandard::SECAM)
    ;
 
  py::class_<VideoFrame, unique_ptr<VideoFrame> >(atari2600, "VideoFrame", py::buffer_protocol())
    .def_property_readonly("width", [](py::object self) { return Atari2600::screenWidth ; })
    .def_property_readonly("height", [](py::object self) { return Atari2600::screenHeight ; })
    .def_buffer([](const VideoFrame &self) -> py::buffer_info {
        return py::buffer_info(
            self.argb,                             
            sizeof(char), py::format_descriptor<const char>::format(),
            3, {Atari2600::screenHeight, Atari2600::screenWidth, 4},
            { sizeof(char) * 4 * Atari2600::screenWidth,
              sizeof(char) * 4,
              sizeof(char) }
        );
    })
    ;  

} 
