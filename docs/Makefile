all: state-machine.ps

%.ps: %.dot
	dot -Tps $< > $(patsubst %.dot,%.ps,$<)

%.eps: %.ps
	ps2epsi $< $(patsubst %.eps,%.ps,$<)
