blast: blast.c blast.h
	cc -DTEST -o blast blast.c

test: blast
	blast < test.pk | cmp - test.txt

clean:
	rm -f blast blast.o
