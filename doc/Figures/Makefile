svgs=\
M6507_Pinout.svg \
TIA_Pinout.svg \
TIA_Screen.svg
pdfs=$(svgs:%.svg=%.pdf)

.PHONY: all
all: $(svgs) 

%.pdf : %.asy
	asy $< -o $@

%.svg : %.pdf Makefile
	pdf2svg $< $@
	#pdf2svg $< $*.tmp.svg
	#svgo --multipass --pretty $*.tmp.svg $@
	#rm -f $*.tmp.svg

clean:
	rm -f $(pdfs)

distclean: clean
	rm -f $(svgs)

info:
	@echo svgs=$(svgs)
	@echo pdfs=$(pdfs)
