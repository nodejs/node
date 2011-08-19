First up, let me say I don't like writing in assembler.  It is not portable,
dependant on the particular CPU architecture release and is generally a pig
to debug and get right.  Having said that, the x86 architecture is probably
the most important for speed due to number of boxes and since
it appears to be the worst architecture to to get
good C compilers for.  So due to this, I have lowered myself to do
assembler for the inner DES routines in libdes :-).

The file to implement in assembler is des_enc.c.  Replace the following
4 functions
des_encrypt1(DES_LONG data[2],des_key_schedule ks, int encrypt);
des_encrypt2(DES_LONG data[2],des_key_schedule ks, int encrypt);
des_encrypt3(DES_LONG data[2],des_key_schedule ks1,ks2,ks3);
des_decrypt3(DES_LONG data[2],des_key_schedule ks1,ks2,ks3);

They encrypt/decrypt the 64 bits held in 'data' using
the 'ks' key schedules.   The only difference between the 4 functions is that
des_encrypt2() does not perform IP() or FP() on the data (this is an
optimization for when doing triple DES and des_encrypt3() and des_decrypt3()
perform triple des.  The triple DES routines are in here because it does
make a big difference to have them located near the des_encrypt2 function
at link time..

Now as we all know, there are lots of different operating systems running on
x86 boxes, and unfortunately they normally try to make sure their assembler
formating is not the same as the other peoples.
The 4 main formats I know of are
Microsoft	Windows 95/Windows NT
Elf		Includes Linux and FreeBSD(?).
a.out		The older Linux.
Solaris		Same as Elf but different comments :-(.

Now I was not overly keen to write 4 different copies of the same code,
so I wrote a few perl routines to output the correct assembler, given
a target assembler type.  This code is ugly and is just a hack.
The libraries are x86unix.pl and x86ms.pl.
des586.pl, des686.pl and des-som[23].pl are the programs to actually
generate the assembler.

So to generate elf assembler
perl des-som3.pl elf >dx86-elf.s
For Windows 95/NT
perl des-som2.pl win32 >win32.asm

[ update 4 Jan 1996 ]
I have added another way to do things.
perl des-som3.pl cpp >dx86-cpp.s
generates a file that will be included by dx86unix.cpp when it is compiled.
To build for elf, a.out, solaris, bsdi etc,
cc -E -DELF asm/dx86unix.cpp | as -o asm/dx86-elf.o
cc -E -DSOL asm/dx86unix.cpp | as -o asm/dx86-sol.o
cc -E -DOUT asm/dx86unix.cpp | as -o asm/dx86-out.o
cc -E -DBSDI asm/dx86unix.cpp | as -o asm/dx86bsdi.o
This was done to cut down the number of files in the distribution.

Now the ugly part.  I acquired my copy of Intels
"Optimization's For Intel's 32-Bit Processors" and found a few interesting
things.  First, the aim of the exersize is to 'extract' one byte at a time
from a word and do an array lookup.  This involves getting the byte from
the 4 locations in the word and moving it to a new word and doing the lookup.
The most obvious way to do this is
xor	eax,	eax				# clear word
movb	al,	cl				# get low byte
xor	edi	DWORD PTR 0x100+des_SP[eax] 	# xor in word
movb	al,	ch				# get next byte
xor	edi	DWORD PTR 0x300+des_SP[eax] 	# xor in word
shr	ecx	16
which seems ok.  For the pentium, this system appears to be the best.
One has to do instruction interleaving to keep both functional units
operating, but it is basically very efficient.

Now the crunch.  When a full register is used after a partial write, eg.
mov	al,	cl
xor	edi,	DWORD PTR 0x100+des_SP[eax]
386	- 1 cycle stall
486	- 1 cycle stall
586	- 0 cycle stall
686	- at least 7 cycle stall (page 22 of the above mentioned document).

So the technique that produces the best results on a pentium, according to
the documentation, will produce hideous results on a pentium pro.

To get around this, des686.pl will generate code that is not as fast on
a pentium, should be very good on a pentium pro.
mov	eax,	ecx				# copy word 
shr	ecx,	8				# line up next byte
and	eax,	0fch				# mask byte
xor	edi	DWORD PTR 0x100+des_SP[eax] 	# xor in array lookup
mov	eax,	ecx				# get word
shr	ecx	8				# line up next byte
and	eax,	0fch				# mask byte
xor	edi	DWORD PTR 0x300+des_SP[eax] 	# xor in array lookup

Due to the execution units in the pentium, this actually works quite well.
For a pentium pro it should be very good.  This is the type of output
Visual C++ generates.

There is a third option.  instead of using
mov	al,	ch
which is bad on the pentium pro, one may be able to use
movzx	eax,	ch
which may not incur the partial write penalty.  On the pentium,
this instruction takes 4 cycles so is not worth using but on the
pentium pro it appears it may be worth while.  I need access to one to
experiment :-).

eric (20 Oct 1996)

22 Nov 1996 - I have asked people to run the 2 different version on pentium
pros and it appears that the intel documentation is wrong.  The
mov al,bh is still faster on a pentium pro, so just use the des586.pl
install des686.pl

3 Dec 1996 - I added des_encrypt3/des_decrypt3 because I have moved these
functions into des_enc.c because it does make a massive performance
difference on some boxes to have the functions code located close to
the des_encrypt2() function.

9 Jan 1997 - des-som2.pl is now the correct perl script to use for
pentiums.  It contains an inner loop from
Svend Olaf Mikkelsen <svolaf@inet.uni-c.dk> which does raw ecb DES calls at
273,000 per second.  He had a previous version at 250,000 and the best
I was able to get was 203,000.  The content has not changed, this is all
due to instruction sequencing (and actual instructions choice) which is able
to keep both functional units of the pentium going.
We may have lost the ugly register usage restrictions when x86 went 32 bit
but for the pentium it has been replaced by evil instruction ordering tricks.

13 Jan 1997 - des-som3.pl, more optimizations from Svend Olaf.
raw DES at 281,000 per second on a pentium 100.

