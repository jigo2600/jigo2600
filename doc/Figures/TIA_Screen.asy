// Draw TIA Screen

settings.outformat = "pdf";
//size(8cm,8cm) ;
unitsize(1.5pt);
defaultpen(fontsize(12pt));
usepackage("sfmath");
texpreamble("\renewcommand*\familydefault{\sfdefault}");

transform t0 = scale(1,-1) * shift(10,10) ;
transform dx = shift(-10,0) ;
transform mdx = shift(10+228,0) ;
transform dy = shift(0,10) ;

path screen = box((0,0),(228,262));
path visible = box((68,40),(228,262-30));
fill(t0*screen,rgb(.5,.5,.5));
draw(t0*screen);
fill(t0*visible,rgb(.9,.9,.9));
draw(t0*((68,0)--(68,262)));
draw(t0*((0,3)--(228,3)));
draw(t0*((0,37+3)--(228,37+3)));
draw(t0*((0,262-30)--(228,262-30)));

void segment(path x, string name, align a) {
draw(x,arrow=Arrows(),bar=Bars(),L=Label(name,position=MidPoint, align=a));
}

segment(t0*mdx*((0,0)--(0,3)),"3 (VSYNC)", 2E) ;
segment(t0*mdx*((0,3)--(0,3+37)),"37 (VBLANK)", 2E);
segment(t0*mdx*((0,3+37)--(0,3+37+192)),"192", 2E);
segment(t0*mdx*((0,3+37+192)--(0,262)),"30 (VBLANK)", 2E);
segment(t0*dx*((0,0)--(0,262)),"262", 2W);
segment(t0*dy*((0,262)--(68,262)),"68 (HBLANK)", 2S);
segment(t0*dy*((68,262)--(228,262)),"160", 2S);
segment(t0*dy*dy*dy*((0,262)--(228,262)),"228 pixels/TIA cycles; 76 CPU cycles", 2S);
