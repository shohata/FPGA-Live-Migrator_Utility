# COMPILE: make all
# DEBUG: make debug=1 all

CC := gcc
ifdef debug
	CFLAG := -Wall -DDEBUG=${debug}
else
	CFLAG := -Wall
endif
SOURCE := migration_controller.c bitstream_server.c fpga.c pc0.c pc1.c
TARGET := $(SOURCE:.c=)

all: $(TARGET);

%: %.c
	$(CC) $(CFLAG) $< -o $@

clean:
	rm -rf $(TARGET)
	rm vivado*
	cd bin && rm *.bin *.prm

convert:
	bash convert_bit_to_bin.sh

convertv:
	vivado -mode batch -source convert_bit_to_bin.tcl
