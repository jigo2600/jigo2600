// Draw M6507 CPU pinout

settings.outformat = "pdf";

int npins = 14 ;
real pinGap = 12 ;
real pinHeight = pinGap*.5 ;
real pinWidth = pinGap ;
real chipHeight = pinGap*npins ;
real chipWidth = pinGap*5 ;
string chipName = "M6507" ;

include "Chip_pinout.asy";

string[] pinNames = {"$\overline{\mathrm{RES}}$","VSS","RDY","VCC"};
for (int n = 0 ; n < 10 ; ++n) { pinNames.push(format("A%d",n)) ; }
int[] pinDirs = {1,0,1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} ;

drawPins(-1,pinNames,pinDirs) ;

string[] pinNames = {"$\Phi_2$","$\Phi_0$","$\mathrm{R}/\overline{\mathrm{W}}$"} ;
for (int n = 0 ; n < 8; ++n) { pinNames.push(format("D%d",n)) ; }
pinNames.push("A12") ;
pinNames.push("A11") ;
pinNames.push("A10") ;
int[] pinDirs = {-1,1,-1,2,2,2,2,2,2,2,2,-1,-1,-1} ;

drawPins(+1,pinNames,pinDirs) ;
