// Draw TIA pinout

settings.outformat = "pdf";

int npins = 20 ;
real pinGap = 12 ;
real pinHeight = pinGap*.5 ;
real pinWidth = pinGap ;
real chipHeight = pinGap*npins ;
real chipWidth = pinGap*5 ;

string chipName = "\parbox{60pt}{\centering TIA\\ (NTSC)}";

include "Chip_pinout.asy";

string[] pinNames = {
         "Vss","CSYNC","RDY","$\Phi_0$","LUM1",
         "$\overline{\mathrm{BLK}}$","LUM2","LUM0",
         "COLOR","CADJ","OSC","AUD1","AUD0"};
for (int n = 0 ; n < 6 ; ++n) { pinNames.push(format("D%d",n)) ; }
pinNames.push("Vcc") ;
int[] pinDirs = {
      +1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,
      +1,-1,-1,+2,+2,
      +2,+2,+2,+2,+1};

drawPins(-1,pinNames,pinDirs) ;

string[] pinNames = {"P0","P1","P2","P3","T0","T1","D7","D6"};
for (int n = 0 ; n < 6 ; ++n) { pinNames.push(format("A%d",n)) ; }
pinNames.push("$\Phi_2$");
pinNames.push("RW");
pinNames.push("$\overline{\mathrm{CS0}}$");
pinNames.push("$\overline{\mathrm{CS1}}$");
pinNames.push("CS2");
pinNames.push("$\overline{\mathrm{CS3}}$");

int[] pinDirs = {
      +1,+1,+1,+1,+1,
      +1,+2,+2,+1,+1,
      +1,+1,+1,+1,+1,
      +1,+1,+1,+1,+1};

drawPins(+1,pinNames,pinDirs) ;
