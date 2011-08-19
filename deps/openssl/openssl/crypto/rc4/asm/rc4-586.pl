#!/usr/local/bin/perl

# At some point it became apparent that the original SSLeay RC4
# assembler implementation performs suboptimaly on latest IA-32
# microarchitectures. After re-tuning performance has changed as
# following:
#
# Pentium	+0%
# Pentium III	+17%
# AMD		+52%(*)
# P4		+180%(**)
#
# (*)	This number is actually a trade-off:-) It's possible to
#	achieve	+72%, but at the cost of -48% off PIII performance.
#	In other words code performing further 13% faster on AMD
#	would perform almost 2 times slower on Intel PIII...
#	For reference! This code delivers ~80% of rc4-amd64.pl
#	performance on the same Opteron machine.
# (**)	This number requires compressed key schedule set up by
#	RC4_set_key and therefore doesn't apply to 0.9.7 [option for
#	compressed key schedule is implemented in 0.9.8 and later,
#	see commentary section in rc4_skey.c for further details].
#
#					<appro@fy.chalmers.se>

push(@INC,"perlasm","../../perlasm");
require "x86asm.pl";

&asm_init($ARGV[0],"rc4-586.pl");

$x="eax";
$y="ebx";
$tx="ecx";
$ty="edx";
$in="esi";
$out="edi";
$d="ebp";

&RC4("RC4");

&asm_finish();

sub RC4_loop
	{
	local($n,$p,$char)=@_;

	&comment("Round $n");

	if ($char)
		{
		if ($p >= 0)
			{
			 &mov($ty,	&swtmp(2));
			&cmp($ty,	$in);
			 &jbe(&label("finished"));
			&inc($in);
			}
		else
			{
			&add($ty,	8);
			 &inc($in);
			&cmp($ty,	$in);
			 &jb(&label("finished"));
			&mov(&swtmp(2),	$ty);
			}
		}
	# Moved out
	# &mov(	$tx,		&DWP(0,$d,$x,4)) if $p < 0;

	&add(	&LB($y),	&LB($tx));
	&mov(	$ty,		&DWP(0,$d,$y,4));
	 # XXX
	&mov(	&DWP(0,$d,$x,4),$ty);
	 &add(	$ty,		$tx);
	&mov(	&DWP(0,$d,$y,4),$tx);
	 &and(	$ty,		0xff);
	 &inc(	&LB($x));			# NEXT ROUND
	&mov(	$tx,		&DWP(0,$d,$x,4)) if $p < 1; # NEXT ROUND
	 &mov(	$ty,		&DWP(0,$d,$ty,4));

	if (!$char)
		{
		#moved up into last round
		if ($p >= 1)
			{
			&add(	$out,	8)
			}
		&movb(	&BP($n,"esp","",0),	&LB($ty));
		}
	else
		{
		# Note in+=8 has occured
		&movb(	&HB($ty),	&BP(-1,$in,"",0));
		 # XXX
		&xorb(&LB($ty),		&HB($ty));
		 # XXX
		&movb(&BP($n,$out,"",0),&LB($ty));
		}
	}


sub RC4
	{
	local($name)=@_;

	&function_begin_B($name,"");

	&mov($ty,&wparam(1));		# len
	&cmp($ty,0);
	&jne(&label("proceed"));
	&ret();
	&set_label("proceed");

	&comment("");

	&push("ebp");
	 &push("ebx");
	&push("esi");
	 &xor(	$x,	$x);		# avoid partial register stalls
	&push("edi");
	 &xor(	$y,	$y);		# avoid partial register stalls
	&mov(	$d,	&wparam(0));	# key
	 &mov(	$in,	&wparam(2));

	&movb(	&LB($x),	&BP(0,$d,"",1));
	 &movb(	&LB($y),	&BP(4,$d,"",1));

	&mov(	$out,	&wparam(3));
	 &inc(	&LB($x));

	&stack_push(3);	# 3 temp variables
	 &add(	$d,	8);

	# detect compressed schedule, see commentary section in rc4_skey.c...
	# in 0.9.7 context ~50 bytes below RC4_CHAR label remain redundant,
	# as compressed key schedule is set up in 0.9.8 and later.
	&cmp(&DWP(256,$d),-1);
	&je(&label("RC4_CHAR"));

	 &lea(	$ty,	&DWP(-8,$ty,$in));

	# check for 0 length input

	 &mov(	&swtmp(2),	$ty);	# this is now address to exit at
	&mov(	$tx,	&DWP(0,$d,$x,4));

	 &cmp(	$ty,	$in);
	&jb(	&label("end")); # less than 8 bytes

	&set_label("start");

	# filling DELAY SLOT
	&add(	$in,	8);

	&RC4_loop(0,-1,0);
	&RC4_loop(1,0,0);
	&RC4_loop(2,0,0);
	&RC4_loop(3,0,0);
	&RC4_loop(4,0,0);
	&RC4_loop(5,0,0);
	&RC4_loop(6,0,0);
	&RC4_loop(7,1,0);
	
	&comment("apply the cipher text");
	# xor the cipher data with input

	#&add(	$out,	8); #moved up into last round

	&mov(	$tx,	&swtmp(0));
	 &mov(	$ty,	&DWP(-8,$in,"",0));
	&xor(	$tx,	$ty);
	 &mov(	$ty,	&DWP(-4,$in,"",0)); 
	&mov(	&DWP(-8,$out,"",0),	$tx);
	 &mov(	$tx,	&swtmp(1));
	&xor(	$tx,	$ty);
	 &mov(	$ty,	&swtmp(2));	# load end ptr;
	&mov(	&DWP(-4,$out,"",0),	$tx);
	 &mov(	$tx,		&DWP(0,$d,$x,4));
	&cmp($in,	$ty);
	 &jbe(&label("start"));

	&set_label("end");

	# There is quite a bit of extra crap in RC4_loop() for this
	# first round
	&RC4_loop(0,-1,1);
	&RC4_loop(1,0,1);
	&RC4_loop(2,0,1);
	&RC4_loop(3,0,1);
	&RC4_loop(4,0,1);
	&RC4_loop(5,0,1);
	&RC4_loop(6,1,1);

	&jmp(&label("finished"));

	&align(16);
	# this is essentially Intel P4 specific codepath, see rc4_skey.c,
	# and is engaged in 0.9.8 and later context...
	&set_label("RC4_CHAR");

	&lea	($ty,&DWP(0,$in,$ty));
	&mov	(&swtmp(2),$ty);
	&movz	($tx,&BP(0,$d,$x));

	# strangely enough unrolled loop performs over 20% slower...
	&set_label("RC4_CHAR_loop");
		&add	(&LB($y),&LB($tx));
		&movz	($ty,&BP(0,$d,$y));
		&movb	(&BP(0,$d,$y),&LB($tx));
		&movb	(&BP(0,$d,$x),&LB($ty));
		&add	(&LB($ty),&LB($tx));
		&movz	($ty,&BP(0,$d,$ty));
		&add	(&LB($x),1);
		&xorb	(&LB($ty),&BP(0,$in));
		&lea	($in,&DWP(1,$in));
		&movz	($tx,&BP(0,$d,$x));
		&cmp	($in,&swtmp(2));
		&movb	(&BP(0,$out),&LB($ty));
		&lea	($out,&DWP(1,$out));
	&jb	(&label("RC4_CHAR_loop"));

	&set_label("finished");
	&dec(	$x);
	 &stack_pop(3);
	&movb(	&BP(-4,$d,"",0),&LB($y));
	 &movb(	&BP(-8,$d,"",0),&LB($x));

	&function_end($name);
	}

