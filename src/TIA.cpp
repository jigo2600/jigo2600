// TIA.cpp
// Atari 2600 TIA emulator.

// Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
// This file is part of Jigo2600 and is made available under
// the terms of the BSD license (see the COPYING file).

#include "TIA.hpp"

#include <iostream>
#include <cstring>

using namespace std ;
using namespace sim ;
using json = nlohmann::json ;

// -------------------------------------------------------------------
// MARK: - Helpers
// -------------------------------------------------------------------

#define C(x) (0x ## x | 0xff000000)
static uint32_t ntsc_palette [] =
{
  C(000000), C(404040), C(6C6C6C), C(909090), C(B0B0B0), C(C8C8C8), C(DCDCDC), C(ECECEC),
  C(444400), C(646410), C(848424), C(A0A034), C(B8B840), C(D0D050), C(E8E85C), C(FCFC68),
  C(702800), C(844414), C(985C28), C(AC783C), C(BC8C4C), C(CCA05C), C(DCB468), C(ECC878),
  C(841800), C(983418), C(AC5030), C(C06848), C(D0805C), C(E09470), C(ECA880), C(FCBC94),
  C(880000), C(9C2020), C(B03C3C), C(C05858), C(D07070), C(E08888), C(ECA0A0), C(FCB4B4),
  C(78005C), C(8C2074), C(A03C88), C(B0589C), C(C070B0), C(D084C0), C(DC9CD0), C(ECB0E0),
  C(480078), C(602090), C(783CA4), C(8C58B8), C(A070CC), C(B484DC), C(C49CEC), C(D4B0FC),
  C(140084), C(302098), C(4C3CAC), C(6858C0), C(7C70D0), C(9488E0), C(A8A0EC), C(BCB4FC),
  C(000088), C(1C209C), C(3840B0), C(505CC0), C(6874D0), C(7C8CE0), C(90A4EC), C(A4B8FC),
  C(00187C), C(1C3890), C(3854A8), C(5070BC), C(6888CC), C(7C9CDC), C(90B4EC), C(A4C8FC),
  C(002C5C), C(1C4C78), C(386890), C(5084AC), C(689CC0), C(7CB4D4), C(90CCE8), C(A4E0FC),
  C(003C2C), C(1C5C48), C(387C64), C(509C80), C(68B494), C(7CD0AC), C(90E4C0), C(A4FCD4),
  C(003C00), C(205C20), C(407C40), C(5C9C5C), C(74B474), C(8CD08C), C(A4E4A4), C(B8FCB8),
  C(143800), C(345C1C), C(507C38), C(6C9850), C(84B468), C(9CCC7C), C(B4E490), C(C8FCA4),
  C(2C3000), C(4C501C), C(687034), C(848C4C), C(9CA864), C(B4C078), C(CCD488), C(E0EC9C),
  C(442800), C(644818), C(846830), C(A08444), C(B89C58), C(D0B46C), C(E8CC7C), C(FCE08C),
} ;

static uint32_t pal_palette [] =
{
  C(000000), C(282828), C(505050), C(747474), C(949494), C(B4B4B4), C(D0D0D0), C(ECECEC),
  C(000000), C(282828), C(505050), C(747474), C(949494), C(B4B4B4), C(D0D0D0), C(ECECEC),
  C(805800), C(947020), C(A8843C), C(BC9C58), C(CCAC70), C(DCC084), C(ECD09C), C(FCE0B0),
  C(445C00), C(5C7820), C(74903C), C(8CAC58), C(A0C070), C(B0D484), C(C4E89C), C(D4FCB0),
  C(703400), C(885020), C(A0683C), C(B48458), C(C89870), C(DCAC84), C(ECC09C), C(FCD4B0),
  C(006414), C(208034), C(3C9850), C(58B06C), C(70C484), C(84D89C), C(9CE8B4), C(B0FCC8),
  C(700014), C(882034), C(A03C50), C(B4586C), C(C87084), C(DC849C), C(EC9CB4), C(FCB0C8),
  C(005C5C), C(207474), C(3C8C8C), C(58A4A4), C(70B8B8), C(84C8C8), C(9CDCDC), C(B0ECEC),
  C(70005C), C(842074), C(943C88), C(A8589C), C(B470B0), C(C484C0), C(D09CD0), C(E0B0E0),
  C(003C70), C(1C5888), C(3874A0), C(508CB4), C(68A4C8), C(7CB8DC), C(90CCEC), C(A4E0FC),
  C(580070), C(6C2088), C(803CA0), C(9458B4), C(A470C8), C(B484DC), C(C49CEC), C(D4B0FC),
  C(002070), C(1C3C88), C(3858A0), C(5074B4), C(6888C8), C(7CA0DC), C(90B4EC), C(A4C8FC),
  C(3C0080), C(542094), C(6C3CA8), C(8058BC), C(9470CC), C(A884DC), C(B89CEC), C(C8B0FC),
  C(000088), C(20209C), C(3C3CB0), C(5858C0), C(7070D0), C(8484E0), C(9C9CEC), C(B0B0FC),
  C(000000), C(282828), C(505050), C(747474), C(949494), C(B4B4B4), C(D0D0D0), C(ECECEC),
  C(000000), C(282828), C(505050), C(747474), C(949494), C(B4B4B4), C(D0D0D0), C(ECECEC),
} ;

static uint32_t secam_palette [] =
{
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
  C(000000), C(2121FF), C(F03C79), C(FF50FF), C(7FFF00), C(7FFFFF), C(FFFF3F), C(FFFFFF),
} ;
#undef C

#define N(x) {TIA::x,# x}
static map<TIA::Register,char const *> tiaRegisterNames {
  N(VSYNC), N(VBLANK), N(WSYNC), N(RSYNC),
  N(NUSIZ0), N(NUSIZ1),
  N(COLUP0), N(COLUP1), N(COLUPF), N(COLUBK), N(CTRLPF),
  N(REFP0), N(REFP1),
  N(PF0), N(PF1), N(PF2),
  N(RESP0), N(RESP1), N(RESM0), N(RESM1), N(RESBL),
  N(AUDC0), N(AUDC1), N(AUDF0), N(AUDF1), N(AUDV0), N(AUDV1),
  N(GRP0), N(GRP1),
  N(ENAM0), N(ENAM1), N(ENABL),
  N(HMP0), N(HMP1), N(HMM0), N(HMM1), N(HMBL),
  N(VDELP0), N(VDELP1), N(VDELBL),
  N(RESMP0), N(RESMP1), N(HMOVE), N(HMCLR), N(CXCLR),
  N(CXM0P), N(CXM1P),
  N(CXP0FB), N(CXP1FB),
  N(CXM0FB), N(CXM1FB),
  N(CXBLPF), N(CXPPMM),
  N(INPT0), N(INPT1), N(INPT2),
  N(INPT3), N(INPT4), N(INPT5),
  N(NA1), N(NA2)
} ;
#undef N

enum TIAObject {
  PF = 0, BL, M0, M1, P0, P1
} ;

enum TIAColor {
  ColorBK = 0,
  ColorPF,
  ColorPM0,
  ColorPM1,
} ;

TIAPlayField::Tables TIAPlayField::tables ;
TIAPlayer::Tables TIAPlayer::tables ;
TIAMissile::Tables TIAMissile::tables ;
TIA::Tables TIA::tables ;

TIA::Tables::Tables()
{
  // Initialize collision and visibility tables.
  for (int v = 0 ; v < 64 ; ++v) {
    // Decode visibility bits.
    bool p0 = v & (1 << TIAObject::P0) ;
    bool p1 = v & (1 << TIAObject::P1) ;
    bool m0 = v & (1 << TIAObject::M0) ;
    bool m1 = v & (1 << TIAObject::M1) ;
    bool bl = v & (1 << TIAObject::BL) ;
    bool pf = v & (1 << TIAObject::PF) ;

    // Collision mask.
    unsigned int collision  =
    (m0 && p0) * (1 << 8 ) | // CXM0P
    (m0 && p1) * (1 << 9 ) |
    (m1 && p1) * (1 << 10) | // CXM1P
    (m1 && p0) * (1 << 11) |
    (p0 && bl) * (1 << 12) | // CXP0FB
    (p0 && pf) * (1 << 13) |
    (p1 && bl) * (1 << 14) | // CXP1FB
    (p1 && pf) * (1 << 15) |
    (m0 && bl) * (1 << 16) | // CXM0FB
    (m0 && pf) * (1 << 17) |
    (m1 && bl) * (1 << 18) | // CXM1FB
    (m1 && pf) * (1 << 19) |
    (bl && pf) * (1 << 20) | // CXBLPF
    (m0 && m1) * (1 << 21) | // CXPPMM
    (p0 && p1) * (1 << 22) ;

    // Color prioritiy table.
    for (int p = 0 ; p < 2 ; ++p) { // PF priority (0: no, 1: yes)
      for (int s = 0 ; s < 3 ; ++s) { // PF score mode (0: none, 1: left, 2: right)
        // The logic reproduces the TIA circuit.
        bool pfp = p > 0 ;
        bool leftScore = (s == 1) ;
        bool rightScore = (s == 2) ;
        bool p0c = !((pf | bl) & pfp) & (p0 | m0 | (leftScore & pf)) ;
        bool p1c = !p0c & !((pf | bl) & pfp) & (p1 | m1 | (rightScore & pf)) ;
        bool pfc = !p0c & !p1c & (pf | bl) ;
        bool bgc = !p0c & !p1c & !pfc ;
        int color = 0 ;
        if (p0c) { color = ColorPM0 ; }
        if (p1c) { color = ColorPM1 ; }
        if (pfc) { color = ColorPF ; }
        if (bgc) { color = ColorBK ; }
        collisionAndColorTable[3*64*p + 64*s + v] = color | collision ;
      }
    }
  }
}

std::ostream & operator<< (std::ostream& os, TIAState::Register r)
{
  auto n = tiaRegisterNames.find(r) ;
  if (n != tiaRegisterNames.end()) {
    os << n->second ;
  } else {
    os << hex << setfill('0') << setw(2) << (int)r << " (TIA?)" ;
  }
  return os ;
}

#define cmp(x) (x == s.x)
bool TIAState::operator==(const sim::TIAState &s) const {
  return
  cmp(numCycles) && cmp(numFrames) && cmp(strobe) && cmp(D) &&
  cmp(RDY) && cmp(beamX) && cmp(beamY) && cmp(Hphasec) &&
  cmp(HBnot) && cmp(SEC) && cmp(SECL) &&
  cmp(VB) && cmp(VS) && cmp(HMC) && cmp(BEC) &&
  cmp(MEC) && cmp(PEC) && cmp(PF) && cmp(B) &&
  cmp(M) && cmp(P) && cmp(collisions) && cmp(ports) ;
}
#undef cmp

inline uint32_t TIA::getColor(uint8_t value) const
{
  int color = (value >> 4) & 0xf ; ;
  int luminance = (value >> 1) & 0x7 ;
  int offset = color * 8 + luminance ;

  switch (videoStandard) {
    case VideoStandard::NTSC: return ntsc_palette[offset] ;
    case VideoStandard::PAL: return pal_palette[offset] ;
    case VideoStandard::SECAM: default: return secam_palette[offset] ;
  }
}

void TIA::reset() {
  numCycles = -1 ;
  numFrames = 0 ;

  // Data bus.
  strobe = VOID ;
  D = 0 ;
  RDY = true ;

  // Display.
  beamX = 227 ;
  beamY = 0 ;
  Hphasec.reset(1,false,56,true) ;
  HBnot.reset() ;
  SEC.reset() ;
  VB = false ;
  VS = false ;

  // Extra motion clocks.
  HMC = false ;
  BEC.reset() ;
  for (int k = 0 ; k < 2 ; ++k) {
    MEC[k].reset() ;
    PEC[k].reset() ;
  }

  // Visual objects.
  PF.reset() ;
  B.reset() ;
  for (int k = 0 ; k < 2 ; ++k) {
    M[k].reset() ;
    P[k].reset() ;
  }
  collisions = 0 ;

  // Ports.
  ports.reset() ;

  // Sound.
  for (int k = 0 ; k < 2 ; ++k) {sound[k].reset();}
}

// -------------------------------------------------------------------
// MARK: - TIA Simulate cycle
// -------------------------------------------------------------------

void TIA::cycle(bool CS, bool Rw, uint16_t address, uint8_t& data)
{
  for (int cycle = 0 ; cycle < 3 ; ++cycle) {

    // As we are starting a new cycle, the current cycle number is increased
    // by one.
    ++numCycles ;

    // Most of the times the electron beam swipes right, but
    // HSYNC (same as HB here) and VSYNC affect that.
    ++beamX ;

    // When the CPU writes to the TIA, a corresponding register strobe
    // is triggered. The strobe is cleared in the middle of cycle=1,
    // but, due to the details of the circuitry,
    // we can pretend it is only cleared at the beginning of cycle=2.
    if (cycle == 2) { strobe = VOID ; }

    // -----------------------------------------------------------------
    // CLK raising edge
    // -----------------------------------------------------------------

    // Advance (or potentially reset) the horizontal dual phase clock and counter.
    Hphasec.cycle(true, strobe == RSYNC) ;

    // -----------------------------------------------------------------
    // CLK first half period
    // -----------------------------------------------------------------

    // Counter decoder logic
    // RHB  = Hphasec.get() == 16 // pattern: 016
    // LRHB = Hphasec.get() == 17 // pattern: 072
    // SHS  = Hphasec.get() == 4  // pattern: 017
    // RHS  = Hphasec.get() == 8  // pattern: 073
    // SHB  ~ Hphasec.get() == 0  // pattern: 000
    // SHB is actually the same as the RES signal.
    auto const SHB = Hphasec.getRES() ;

    // RHS_delayed = TIADelay(RHS)
    // HS = TIADualPhaseLatch(Hphasec, SHS, RHS_delayed)
    if (SHB && Hphasec.getPhi2()) { beamX = 0 ; ++beamY ; } // <= revisit

    // HM logic
    // There should be a dual-phase delay between
    // SEC and HMC switching to one. We optimize this out by
    // updating HMC *before* SEC is udpdated (instead as after
    // as we do for the rest of the dependency chains in level-sensitive
    // logic).
    if (Hphasec.getPhi2() && ((HMC > 0) | SEC.get())) {
      HMC = (HMC + 1) & 0xf ;
    }

    // SEC and SECL logic
    // The SECL latch is set on SEC and reset on SHB. Corner case:
    // it is possible to hit HMOVE in such a way to cause
    // SHB and SEC to turn on exactly at the same time at the beginning
    // of a line (colour clocks 0,1,2,3). This is a race condition and SEC
    // appears to prevail. Inverting the following lines
    // causes the `Bermuda' game to glitch.
    SEC.cycle(Hphasec, strobe == HMOVE) ;
    SECL &= !SHB ;
    SECL |= SEC.get() ;

    // Extra clocks logic
    // This logic updates the enable signals for the extra clocks,
    // not the clock signal directly.
    BEC.cycle(Hphasec, SEC.get(), HMC) ;
    MEC[0].cycle(Hphasec, SEC.get(), HMC) ;
    MEC[1].cycle(Hphasec, SEC.get(), HMC) ;
    PEC[0].cycle(Hphasec, SEC.get(), HMC) ;
    PEC[1].cycle(Hphasec, SEC.get(), HMC) ;

    // HBnot logic
    // Delayed dual-phase latch with:
    // * Set at (SECLnot & RHB) | (SECL & LRHB).
    // * Reset at SHB.
    HBnot.cycle(Hphasec, (Hphasec.get() == 16 + 2 * SECL), SHB) ;

    // RDY logic
    // Asynchronous latch with:
    // * Set on SHB.
    // * Reset on WSYNC strobe.
    // The circuit is designed such that SHB has priority on WSYNC.
    RDY &= (strobe != WSYNC) ;
    RDY |= SHB ;

    // IO ports logic
    ports.cycle(Hphasec) ;

    // Audio logic
    if (Hphasec.getPhi2() && (Hphasec.get() == 9 || Hphasec.get() == 37)) {
      for (int k = 0 ; k < 2 ; ++k) {
        sound[k].cycle(int(numCycles)) ;
      }
    }

    // A mask with the list of visible objects at this color clock.
    bitset<6> visibility {0} ;

    // Playfield logic
    PF.cycle(visibility[TIAObject::PF], Hphasec) ;

    // -----------------------------------------------------------------
    // EC raising edge (somewhere in between CLK and CLKP raising edge)
    // -----------------------------------------------------------------
    // Extra clock udpate the graphics before it is latched. This is used
    // by the `Cosmic Ark' cart.

    if (BEC.get(Hphasec)) { B.cycle(true, strobe == RESBL) ; }
    if (MEC[0].get(Hphasec)) { M[0].cycle(true, strobe == RESM0, P[0]) ; }
    if (MEC[1].get(Hphasec)) { M[1].cycle(true, strobe == RESM1, P[1]) ; }
    if (PEC[0].get(Hphasec)) { P[0].cycle(true, strobe == RESP0) ; }
    if (PEC[1].get(Hphasec)) { P[1].cycle(true, strobe == RESP1) ; }

    // -----------------------------------------------------------------
    // CLK falling edge, CKLP raising edge
    // -----------------------------------------------------------------

    visibility[TIAObject::BL] = B.get() ;
    visibility[TIAObject::M0] = M[0].get() ;
    visibility[TIAObject::M1] = M[1].get() ;
    visibility[TIAObject::P0] = P[0].get() ;
    visibility[TIAObject::P1] = P[1].get() ;

    // Todo: Color updates at the last pixel should be reflected? Why?
    if (!VB)
    {
      int x = beamX - 68 ;
      int y = beamY - (40 - 3) ;
      bool right = (x >= 80) ;

      // Collisions and color. Collisions are deteced during HBLANK,
      // but not during VBLANK.
      int collisionAndColor = tables.collisionAndColorTable
      [64*3 * PF.getPFP() +
       64 * (PF.getSCORE() * (1 + right)) +
       visibility.to_ulong()] ;
      collisions |= collisionAndColor ;

      if (HBnot.get()) {
        if (0 <= x && x < screenWidth &&
            0 <= y && y < screenHeight) {
          screen[currentScreen][screenWidth * y + x] = colors[collisionAndColor & 0xf] ;
        }
      }
    }

    // -----------------------------------------------------------------
    // CKLP raising edge and CLK second half period
    // -----------------------------------------------------------------
    // The visual objects are also sensitive to the raising edge of CLKP,
    // but the update is implemented in-place. The pixel latches that
    // come last in the chain of dependency are thus updated first, just
    // above.

    auto const MOTCK = HBnot.get() ;

    // TODO: not so sure if M should be updated before or after P
    // due to the RESMP depenency
    B.cycle(MOTCK & !BEC.get(Hphasec), strobe == RESBL) ;
    M[0].cycle(MOTCK & !MEC[0].get(Hphasec), strobe == RESM0, P[0]) ;
    M[1].cycle(MOTCK & !MEC[1].get(Hphasec), strobe == RESM1, P[1]) ;
    P[0].cycle(MOTCK & !PEC[0].get(Hphasec), strobe == RESP0) ;
    P[1].cycle(MOTCK & !PEC[1].get(Hphasec), strobe == RESP1) ;

    // -----------------------------------------------------------------
    // Strobes.
    // -----------------------------------------------------------------

    if (cycle == 0) {
      switch (strobe) {
        case NUSIZ0: case NUSIZ1:
          P[strobe - NUSIZ0].setNUSIZ(D) ;
          M[strobe - NUSIZ0].setSIZ(D) ;
          break ;
        case GRP0: {
          P[0].setGRP(D) ;
          P[1].shiftGRP() ;
          break ;
        }
        case GRP1: {
          P[1].setGRP(D) ;
          P[0].shiftGRP() ;
          B.shiftBLEN() ;
          break ;
        }
        case HMP0: PEC[0].setHM(D) ; break ;
        case HMP1: PEC[1].setHM(D) ; break ;
        case HMM0: MEC[0].setHM(D) ; break ;
        case HMM1: MEC[1].setHM(D) ; break ;
        case HMBL: BEC.setHM(D) ; break ;
        case VBLANK: {
          // This is also late (see the new DK JR cartridge,
          // which uses VBLANK to draw in the middle of the picture
          // the player score).
          VB = D & 0x02 ;
          ports.setINPT(D) ;
          break ;
        }
          //case HMOVE: HMOVEL = true ; break ;
        default: break ;
      }
    }
  } // Next cycle.

  // -----------------------------------------------------------------
  // Update registers.
  // -----------------------------------------------------------------
  // When the CPU writes to the TIA, the whole cycle (corresponding
  // to the 3 color cycles just performed) is used to update the
  // internal data and address buffers. These become available
  // only at the *next* TIA cycle, which is when the strobe is 
  // finally raised. We thus set it here for the next cycle. Likewise,
  // we immediately update the required registers, so they are ready
  // for the next iteration. However, empirically, there are a few registers
  // that must be updated one CLK cycle later, maybe due to internal
  // delays in the TIA; why this is so is unclear.
  //
  // In a read operation, the data written on the bus is the
  // one available in the third TIA cycle. 

  if (CS) {
    auto reg = decodeAddress(Rw,address) ;
    if (Rw) {
      // The CPU reads from TIA.
      // The TIA drives only a few bits (the last one or last two)
      // on the data bus, so the other are likely to stay the same as they are
      // due to capacitive effects.
      auto set2 = [&] (uint8_t x) { data = (x & 0xc0) | (data & 0x3f) ;};
      // It seems that the TIA always sets the last two bits, defaulting
      // to zero for those registers that set only one (or the NA1, NA2
      // unutilized addresses that set none).
      //auto set1 = [&] (uint8_t x) { value = (x & 0x80) | (data & 0x7f) ;};
      auto set1 = [&] (uint8_t x) { data = (x & 0x80) | (data & 0x3f) ;};
      switch (reg) {
        case INPT0: set1(ports.getINPT(0)) ; break ;
        case INPT1: set1(ports.getINPT(1)) ; break ;
        case INPT2: set1(ports.getINPT(2)) ; break ;
        case INPT3: set1(ports.getINPT(3)) ; break ;
        case INPT4: set1(ports.getINPT(4)) ; break ;
        case INPT5: set1(ports.getINPT(5)) ; break ;
        case CXM0P:  set2(collisions >>  2) ; break ;
        case CXM1P:  set2(collisions >>  4) ; break ;
        case CXP0FB: set2(collisions >>  6) ; break ;
        case CXP1FB: set2(collisions >>  8) ; break ;
        case CXM0FB: set2(collisions >> 10) ; break ;
        case CXM1FB: set2(collisions >> 12) ; break ;
        case CXBLPF: set1(collisions >> 13) ; break ;
        case CXPPMM: set2(collisions >> 15) ; break ;
        case NA1: case NA2: data = 0 ; /* todo: should be set2(0), but other emu get this wrong? */ break ;
        default: break ;
      }
    } else {
      // The CPU writes to the TIA.
      strobe = reg ;
      D = data ;
      switch (strobe) {
        case VSYNC: {
          if (D & 0x02) {
            VS = true ;
          } else {
            if (VS == true) {
              // VSYNC switches off
              beamY = 0 ;
              startNewFrame = true ;
              currentScreen = (currentScreen + 1) % numScreenBuffers ;
              int memorySize = screenWidth*screenHeight*sizeof(uint32_t) ;
              memset(screen[currentScreen], 0, memorySize) ;
            }
            VS = false ;
          }
          break ;
        }
        case RSYNC: break ;
        case PF0: PF.setPF0(D) ; break ;
        case PF1: PF.setPF1(D) ; break ;
        case PF2: PF.setPF2(D) ; break ;
        case CTRLPF : {
          PF.setCTRLPF(D) ;
          B.setBLSIZ(D) ;
          break ;
        }
        case ENAM0: case ENAM1: M[strobe - ENAM0].setENAM(D) ; break ;
        case ENABL: B.setBLEN(D) ; break ;
        case REFP0: case REFP1: P[strobe - REFP0].setREFL(D) ; break ;
        case VDELP0: case VDELP1: P[strobe - VDELP0].setVDELP(D) ; break ;
        case VDELBL: B.setBLVD(data) ; break ;
        case RESMP0: case RESMP1: M[strobe - RESMP0].setRESMP(D) ; break ;
        //case HMOVE: HMOVEL = true ; break ;
        case HMCLR: {
          BEC.clearHM() ;
          #pragma unroll
          for (int k = 0 ; k < 2 ; ++k) {
            PEC[k].clearHM() ;
            MEC[k].clearHM() ;
          }
          break ;
        }
          // Todo: serialize colors.
        case COLUP0: colors[ColorPM0] = getColor(D) ; break ;
        case COLUP1: colors[ColorPM1] = getColor(D) ; break ;
        case COLUPF: colors[ColorPF] = getColor(D) ; break ;
        case COLUBK: colors[ColorBK] = getColor(D) ; break ;
        case AUDV0: sound[0].setAUDV(D) ; break ;
        case AUDV1: sound[1].setAUDV(D) ; break ;
        case AUDF0: sound[0].setAUDF(D) ; break ;
        case AUDF1: sound[1].setAUDF(D) ; break ;
        case AUDC0: sound[0].setAUDC(D) ; break ;
        case AUDC1: sound[1].setAUDC(D) ; break ;
        case CXCLR: collisions = 0 ; break ;
        default: break ;
      }
    }
  }
}

// -------------------------------------------------------------------
// MARK: - Serialize & deserialize state
// -------------------------------------------------------------------

namespace sim {
  static void to_json(nlohmann::json& j, const TIAState::VideoStandard& p)
  {
    switch (p) {
      case TIA::VideoStandard::NTSC: j = "NTSC" ; break ;
      case TIA::VideoStandard::PAL: j = "PAL" ; break ;
      case TIA::VideoStandard::SECAM: j = "SECAM" ; break ;
      default: assert(false) ;
    }
  }

  /// Throws `nlohmann::json::exception` on parsing errors.
  static void from_json(const nlohmann::json& j, TIAState::VideoStandard& p)
  {
    if (j.is_null()) {
      throw std::invalid_argument
      (std::string("Video standard standard specifier is null")) ;
    }
    else {
      string str = j.get<string>() ;
      if (str == "NTSC") { p = TIA::VideoStandard::NTSC ; }
      else if (str == "PAL") { p = TIA::VideoStandard::PAL ; }
      else if (str == "SECAM") { p = TIA::VideoStandard::SECAM ; }
      else { throw std::invalid_argument
        (std::string("Unknown cartridge standard specifier " + str)) ; }
    }
  }

  void to_json(nlohmann::json& j, TIAState const& state)
  {
#undef jput
#define jput(x) j[# x] = state.x
    jput(videoStandard) ;
    jput(numCycles) ;
    jput(numFrames) ;
    jput(strobe) ;
    jput(RDY) ;
    jput(beamX) ;
    jput(beamY) ;
    jput(Hphasec) ;
    jput(SEC) ;
    jput(SECL) ;
    //jput(HMOVEL) ;
    jput(VB) ;
    jput(VS) ;
    jput(HMC) ;
    jput(BEC) ;
    jput(MEC) ;
    jput(PEC) ;
    jput(PF);
    jput(B);
    jput(M);
    jput(P);
    jput(collisions);
    jput(ports);
  }

  /// Throws `nlohmann::json::exception` on parsing errors.
  void from_json(nlohmann::json const& j, TIAState& state)
  {
#undef jget
#define jget(m) state.m = j[# m].get<decltype(state.m)>()
    jget(videoStandard) ;
    jget(numCycles) ;
    jget(numFrames) ;
    jget(strobe) ;
    jget(RDY) ;
    jget(beamX) ;
    jget(beamY) ;
    jget(Hphasec) ;
    jget(SEC) ;
    jget(SECL) ;
    //jget(HMOVEL) ;
    jget(VB) ;
    jget(VS) ;
    jget(HMC) ;
    jget(BEC) ;
    jget(MEC) ;
    jget(PEC) ;
    jget(PF);
    jget(B);
    jget(M);
    jget(P);
    jget(collisions);
    jget(ports);
  }
}
