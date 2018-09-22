path chip = box((-chipWidth/2,0),(chipWidth/2,chipHeight));
draw(chip);
draw(arc((0,chipHeight),chipWidth/8,180,360)) ;
label(chipName,(0,chipHeight/2)) ;

path pin = box((-pinWidth/2,-pinHeight/2+pinGap/2),
               (pinWidth/2,+pinHeight/2+pinGap/2));
pen pinLabelPen = fontsize(8pt) ;

void drawPins(real side, string[] pinNames, int[] pinDirs) {
  for (int n = 0 ; n < npins ; ++n) {
    int k = npins - n - 1 ;
    real midy = pinGap*n+pinGap/2 ;
    real axbeg = side*chipWidth/2+side*pinWidth*4.0 ;
    real axend = side*chipWidth/2+side*pinWidth*5.0 ;
    draw(shift(side*(chipWidth/2+pinWidth/2),pinGap*n)*pin) ;
    align labelAlign = W ;
    if (side > 0) {
      labelAlign = E ;
    }
    label(pinNames[k],
          (side*(chipWidth/2+pinWidth),pinGap*n+pinGap/2),
          p=pinLabelPen,align=labelAlign) ;
    if (pinDirs[k] == 0) { continue ; }
    if (pinDirs[k] == -1) {
      draw((axbeg,midy)--(axend,midy),Arrow(SimpleHead)) ;
    }
    if (pinDirs[k] == +1) {
      draw((axend,midy)--(axbeg,midy),Arrow(SimpleHead)) ;
    }
    if (pinDirs[k] == +2) {
      draw((axend,midy)--(axbeg,midy),Arrows(SimpleHead)) ;
    }
  }
}