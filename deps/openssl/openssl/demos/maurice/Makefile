CC=cc
CFLAGS= -g -I../../include -Wall
LIBS=  -L../.. -lcrypto
EXAMPLES=example1 example2 example3 example4

all: $(EXAMPLES) 

example1: example1.o loadkeys.o 
	$(CC) -o example1 example1.o loadkeys.o $(LIBS)

example2: example2.o loadkeys.o
	$(CC) -o example2 example2.o loadkeys.o $(LIBS)

example3: example3.o 
	$(CC) -o example3 example3.o $(LIBS)

example4: example4.o
	$(CC) -o example4 example4.o $(LIBS)

clean:	
	rm -f $(EXAMPLES) *.o

test: all
	@echo
	@echo Example 1 Demonstrates the sealing and opening APIs
	@echo Doing the encrypt side...
	./example1 <README >t.t
	@echo Doing the decrypt side...
	./example1 -d <t.t >t.2
	diff t.2 README
	rm -f t.t t.2
	@echo  example1 is OK

	@echo
	@echo Example2 Demonstrates rsa encryption and decryption
	@echo   and it should just print \"This the clear text\"
	./example2

	@echo
	@echo Example3 Demonstrates the use of symmetric block ciphers
	@echo in this case it uses EVP_des_ede3_cbc
	@echo i.e. triple DES in Cipher Block Chaining mode
	@echo Doing the encrypt side...
	./example3 ThisIsThePassword <README >t.t
	@echo Doing the decrypt side...
	./example3 -d ThisIsThePassword <t.t >t.2
	diff t.2 README
	rm -f t.t t.2
	@echo  example3 is OK

	@echo
	@echo Example4 Demonstrates base64 encoding and decoding
	@echo Doing the encrypt side...
	./example4 <README >t.t
	@echo Doing the decrypt side...
	./example4 -d <t.t >t.2
	diff t.2 README
	rm -f t.t t.2
	@echo example4 is OK
