#FQBN := "attiny:avr:ATtinyX5:cpu=attiny85,clock=internal8"
FQBN := "ATTinyCore:avr:attinyx5"
PORT := "/dev/ttyACM0"
PROGRAMMER :="usbtinyisp"

.DEFAULT_GOAL := all

.PHONY: all
all: compile upload

.PHONY: install-core
install-core:
	arduino-cli core install arduino:avr

.PHONY: compile
compile:
	arduino-cli compile --fqbn $(FQBN)

.PHONY: upload
upload:
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) --programmer $(PROGRAMMER)

.PHONY: programmers
programmers:
	 arduino-cli board details --fqbn $(FQBN) --list-programmers
