CFLAGS=-O

puff: puff.o pufftest.o

puff.o: puff.h

pufftest.o: puff.h

test: puff
	puff zeros.raw

puft: puff.c puff.h pufftest.o
	cc -fprofile-arcs -ftest-coverage -o puft puff.c pufftest.o

# puff full coverage test (should say 100%)
cov: puft
	@rm -f *.gcov *.gcda
	@puft -w zeros.raw 2>&1 | cat > /dev/null
	@echo '04' | xxd -r -p | puft 2> /dev/null || test $$? -eq 2
	@echo '00' | xxd -r -p | puft 2> /dev/null || test $$? -eq 2
	@echo '00 00 00 00 00' | xxd -r -p | puft 2> /dev/null || test $$? -eq 254
	@echo '00 01 00 fe ff' | xxd -r -p | puft 2> /dev/null || test $$? -eq 2
	@echo '01 01 00 fe ff 0a' | xxd -r -p | puft -f 2>&1 | cat > /dev/null
	@echo '02 7e ff ff' | xxd -r -p | puft 2> /dev/null || test $$? -eq 246
	@echo '02' | xxd -r -p | puft 2> /dev/null || test $$? -eq 2
	@echo '04 80 49 92 24 49 92 24 0f b4 ff ff c3 04' | xxd -r -p | puft 2> /dev/null || test $$? -eq 2
	@echo '04 80 49 92 24 49 92 24 71 ff ff 93 11 00' | xxd -r -p | puft 2> /dev/null || test $$? -eq 249
	@echo '04 c0 81 08 00 00 00 00 20 7f eb 0b 00 00' | xxd -r -p | puft 2> /dev/null || test $$? -eq 246
	@echo '0b 00 00' | xxd -r -p | puft -f 2>&1 | cat > /dev/null
	@echo '1a 07' | xxd -r -p | puft 2> /dev/null || test $$? -eq 246
	@echo '0c c0 81 00 00 00 00 00 90 ff 6b 04' | xxd -r -p | puft 2> /dev/null || test $$? -eq 245
	@puft -f zeros.raw 2>&1 | cat > /dev/null
	@echo 'fc 00 00' | xxd -r -p | puft 2> /dev/null || test $$? -eq 253
	@echo '04 00 fe ff' | xxd -r -p | puft 2> /dev/null || test $$? -eq 252
	@echo '04 00 24 49' | xxd -r -p | puft 2> /dev/null || test $$? -eq 251
	@echo '04 80 49 92 24 49 92 24 0f b4 ff ff c3 84' | xxd -r -p | puft 2> /dev/null || test $$? -eq 248
	@echo '04 00 24 e9 ff ff' | xxd -r -p | puft 2> /dev/null || test $$? -eq 250
	@echo '04 00 24 e9 ff 6d' | xxd -r -p | puft 2> /dev/null || test $$? -eq 247
	@gcov -n puff.c

clean:
	rm -f puff puft *.o *.gc*
