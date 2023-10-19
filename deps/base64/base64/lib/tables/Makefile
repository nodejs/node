.PHONY: all clean

TARGETS := table_dec_32bit.h table_enc_12bit.h table_generator

all: $(TARGETS)

clean:
	$(RM) $(TARGETS)

table_dec_32bit.h: table_generator
	./$^ > $@

table_enc_12bit.h: table_enc_12bit.py
	./$^ > $@

table_generator: table_generator.c
	$(CC) $(CFLAGS) -o $@ $^
