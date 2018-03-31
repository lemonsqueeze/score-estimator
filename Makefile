
all: 
	@cd src && make all

clean:
	@cd src && make clean

MAKEFLAGS += --no-print-directory
