all: state-machine.ps

%.ps: %.dot
	dot -Tps -o $@ $<

%.eps: %.ps
	ps2epsi $< $@
