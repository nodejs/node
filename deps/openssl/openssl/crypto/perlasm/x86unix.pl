#!/usr/local/bin/perl

package x86unix;	# GAS actually...

$label="L000";
$const="";
$constl=0;

$align=($main'aout)?"4":"16";
$under=($main'aout or $main'coff)?"_":"";
$dot=($main'aout)?"":".";
$com_start="#" if ($main'aout or $main'coff);

sub main'asm_init_output { @out=(); }
sub main'asm_get_output { return(@out); }
sub main'get_labels { return(@labels); }
sub main'external_label { push(@labels,@_); }

if ($main'cpp)
	{
	$align="ALIGN";
	$under="";
	$com_start='/*';
	$com_end='*/';
	}

%lb=(	'eax',	'%al',
	'ebx',	'%bl',
	'ecx',	'%cl',
	'edx',	'%dl',
	'ax',	'%al',
	'bx',	'%bl',
	'cx',	'%cl',
	'dx',	'%dl',
	);

%hb=(	'eax',	'%ah',
	'ebx',	'%bh',
	'ecx',	'%ch',
	'edx',	'%dh',
	'ax',	'%ah',
	'bx',	'%bh',
	'cx',	'%ch',
	'dx',	'%dh',
	);

%regs=(	'eax',	'%eax',
	'ebx',	'%ebx',
	'ecx',	'%ecx',
	'edx',	'%edx',
	'esi',	'%esi',
	'edi',	'%edi',
	'ebp',	'%ebp',
	'esp',	'%esp',

	'mm0',	'%mm0',
	'mm1',	'%mm1',
	'mm2',	'%mm2',
	'mm3',	'%mm3',
	'mm4',	'%mm4',
	'mm5',	'%mm5',
	'mm6',	'%mm6',
	'mm7',	'%mm7',

	'xmm0',	'%xmm0',
	'xmm1',	'%xmm1',
	'xmm2',	'%xmm2',
	'xmm3',	'%xmm3',
	'xmm4',	'%xmm4',
	'xmm5',	'%xmm5',
	'xmm6',	'%xmm6',
	'xmm7',	'%xmm7',
	);

%reg_val=(
	'eax',	0x00,
	'ebx',	0x03,
	'ecx',	0x01,
	'edx',	0x02,
	'esi',	0x06,
	'edi',	0x07,
	'ebp',	0x05,
	'esp',	0x04,
	);

sub main'LB
	{
	(defined($lb{$_[0]})) || die "$_[0] does not have a 'low byte'\n";
	return($lb{$_[0]});
	}

sub main'HB
	{
	(defined($hb{$_[0]})) || die "$_[0] does not have a 'high byte'\n";
	return($hb{$_[0]});
	}

sub main'DWP
	{
	local($addr,$reg1,$reg2,$idx)=@_;

	$ret="";
	$addr =~ s/(^|[+ \t])([A-Za-z_]+[A-Za-z0-9_]+)($|[+ \t])/$1$under$2$3/;
	$reg1="$regs{$reg1}" if defined($regs{$reg1});
	$reg2="$regs{$reg2}" if defined($regs{$reg2});
	$ret.=$addr if ($addr ne "") && ($addr ne 0);
	if ($reg2 ne "")
		{
		if($idx ne "" && $idx != 0)
		    { $ret.="($reg1,$reg2,$idx)"; }
		else
		    { $ret.="($reg1,$reg2)"; }
	        }
	elsif ($reg1 ne "")
		{ $ret.="($reg1)" }
	return($ret);
	}

sub main'QWP
	{
	return(&main'DWP(@_));
	}

sub main'BP
	{
	return(&main'DWP(@_));
	}

sub main'BC
	{
	return @_;
	}

sub main'DWC
	{
	return @_;
	}

#sub main'BP
#	{
#	local($addr,$reg1,$reg2,$idx)=@_;
#
#	$ret="";
#
#	$addr =~ s/(^|[+ \t])([A-Za-z_]+)($|[+ \t])/$1$under$2$3/;
#	$reg1="$regs{$reg1}" if defined($regs{$reg1});
#	$reg2="$regs{$reg2}" if defined($regs{$reg2});
#	$ret.=$addr if ($addr ne "") && ($addr ne 0);
#	if ($reg2 ne "")
#		{ $ret.="($reg1,$reg2,$idx)"; }
#	else
#		{ $ret.="($reg1)" }
#	return($ret);
#	}

sub main'mov	{ &out2("movl",@_); }
sub main'movb	{ &out2("movb",@_); }
sub main'and	{ &out2("andl",@_); }
sub main'or	{ &out2("orl",@_); }
sub main'shl	{ &out2("sall",@_); }
sub main'shr	{ &out2("shrl",@_); }
sub main'xor	{ &out2("xorl",@_); }
sub main'xorb	{ &out2("xorb",@_); }
sub main'add	{ &out2($_[0]=~/%[a-d][lh]/?"addb":"addl",@_); }
sub main'adc	{ &out2("adcl",@_); }
sub main'sub	{ &out2("subl",@_); }
sub main'sbb	{ &out2("sbbl",@_); }
sub main'rotl	{ &out2("roll",@_); }
sub main'rotr	{ &out2("rorl",@_); }
sub main'exch	{ &out2($_[0]=~/%[a-d][lh]/?"xchgb":"xchgl",@_); }
sub main'cmp	{ &out2("cmpl",@_); }
sub main'lea	{ &out2("leal",@_); }
sub main'mul	{ &out1("mull",@_); }
sub main'imul	{ &out2("imull",@_); }
sub main'div	{ &out1("divl",@_); }
sub main'jmp	{ &out1("jmp",@_); }
sub main'jmp_ptr { &out1p("jmp",@_); }
sub main'je	{ &out1("je",@_); }
sub main'jle	{ &out1("jle",@_); }
sub main'jne	{ &out1("jne",@_); }
sub main'jnz	{ &out1("jnz",@_); }
sub main'jz	{ &out1("jz",@_); }
sub main'jge	{ &out1("jge",@_); }
sub main'jl	{ &out1("jl",@_); }
sub main'ja	{ &out1("ja",@_); }
sub main'jae	{ &out1("jae",@_); }
sub main'jb	{ &out1("jb",@_); }
sub main'jbe	{ &out1("jbe",@_); }
sub main'jc	{ &out1("jc",@_); }
sub main'jnc	{ &out1("jnc",@_); }
sub main'jno	{ &out1("jno",@_); }
sub main'dec	{ &out1("decl",@_); }
sub main'inc	{ &out1($_[0]=~/%[a-d][hl]/?"incb":"incl",@_); }
sub main'push	{ &out1("pushl",@_); $stack+=4; }
sub main'pop	{ &out1("popl",@_); $stack-=4; }
sub main'pushf	{ &out0("pushfl"); $stack+=4; }
sub main'popf	{ &out0("popfl"); $stack-=4; }
sub main'not	{ &out1("notl",@_); }
sub main'call	{	my $pre=$under;
			foreach $i (%label)
			{ if ($label{$i} eq $_[0]) { $pre=''; last; } }
			&out1("call",$pre.$_[0]);
		}
sub main'call_ptr { &out1p("call",@_); }
sub main'ret	{ &out0("ret"); }
sub main'nop	{ &out0("nop"); }
sub main'test	{ &out2("testl",@_); }
sub main'bt	{ &out2("btl",@_); }
sub main'leave	{ &out0("leave"); }
sub main'cpuid	{ &out0(".byte\t0x0f,0xa2"); }
sub main'rdtsc	{ &out0(".byte\t0x0f,0x31"); }
sub main'halt	{ &out0("hlt"); }
sub main'movz	{ &out2("movzbl",@_); }
sub main'neg	{ &out1("negl",@_); }
sub main'cld	{ &out0("cld"); }

# SSE2
sub main'emms	{ &out0("emms"); }
sub main'movd	{ &out2("movd",@_); }
sub main'movdqu	{ &out2("movdqu",@_); }
sub main'movdqa	{ &out2("movdqa",@_); }
sub main'movdq2q{ &out2("movdq2q",@_); }
sub main'movq2dq{ &out2("movq2dq",@_); }
sub main'paddq	{ &out2("paddq",@_); }
sub main'pmuludq{ &out2("pmuludq",@_); }
sub main'psrlq	{ &out2("psrlq",@_); }
sub main'psllq	{ &out2("psllq",@_); }
sub main'pxor	{ &out2("pxor",@_); }
sub main'por	{ &out2("por",@_); }
sub main'pand	{ &out2("pand",@_); }
sub main'movq	{
	local($p1,$p2,$optimize)=@_;
	if ($optimize && $p1=~/^mm[0-7]$/ && $p2=~/^mm[0-7]$/)
		# movq between mmx registers can sink Intel CPUs
		{	push(@out,"\tpshufw\t\$0xe4,%$p2,%$p1\n");	}
	else	{	&out2("movq",@_);				}
	}

# The bswapl instruction is new for the 486. Emulate if i386.
sub main'bswap
	{
	if ($main'i386)
		{
		&main'comment("bswapl @_");
		&main'exch(main'HB(@_),main'LB(@_));
		&main'rotr(@_,16);
		&main'exch(main'HB(@_),main'LB(@_));
		}
	else
		{
		&out1("bswapl",@_);
		}
	}

sub out2
	{
	local($name,$p1,$p2)=@_;
	local($l,$ll,$t);
	local(%special)=(	"roll",0xD1C0,"rorl",0xD1C8,
				"rcll",0xD1D0,"rcrl",0xD1D8,
				"shll",0xD1E0,"shrl",0xD1E8,
				"sarl",0xD1F8);
	
	if ((defined($special{$name})) && defined($regs{$p1}) && ($p2 == 1))
		{
		$op=$special{$name}|$reg_val{$p1};
		$tmp1=sprintf(".byte %d\n",($op>>8)&0xff);
		$tmp2=sprintf(".byte %d\t",$op     &0xff);
		push(@out,$tmp1);
		push(@out,$tmp2);

		$p2=&conv($p2);
		$p1=&conv($p1);
		&main'comment("$name $p2 $p1");
		return;
		}

	push(@out,"\t$name\t");
	$t=&conv($p2).",";
	$l=length($t);
	push(@out,$t);
	$ll=4-($l+9)/8;
	$tmp1=sprintf("\t" x $ll);
	push(@out,$tmp1);
	push(@out,&conv($p1)."\n");
	}

sub out1
	{
	local($name,$p1)=@_;
	local($l,$t);
	local(%special)=("bswapl",0x0FC8);

	if ((defined($special{$name})) && defined($regs{$p1}))
		{
		$op=$special{$name}|$reg_val{$p1};
		$tmp1=sprintf(".byte %d\n",($op>>8)&0xff);
		$tmp2=sprintf(".byte %d\t",$op     &0xff);
		push(@out,$tmp1);
		push(@out,$tmp2);

		$p2=&conv($p2);
		$p1=&conv($p1);
		&main'comment("$name $p2 $p1");
		return;
		}

	push(@out,"\t$name\t".&conv($p1)."\n");
	}

sub out1p
	{
	local($name,$p1)=@_;
	local($l,$t);

	push(@out,"\t$name\t*".&conv($p1)."\n");
	}

sub out0
	{
	push(@out,"\t$_[0]\n");
	}

sub conv
	{
	local($p)=@_;

#	$p =~ s/0x([0-9A-Fa-f]+)/0$1h/;

	$p=$regs{$p} if (defined($regs{$p}));

	$p =~ s/^(-{0,1}[0-9A-Fa-f]+)$/\$$1/;
	$p =~ s/^(0x[0-9A-Fa-f]+)$/\$$1/;
	return $p;
	}

sub main'file
	{
	local($file)=@_;

	local($tmp)=<<"EOF";
	.file	"$file.s"
EOF
	push(@out,$tmp);
	}

sub main'function_begin
	{
	local($func)=@_;

	&main'external_label($func);
	$func=$under.$func;

	local($tmp)=<<"EOF";
.text
.globl	$func
EOF
	push(@out,$tmp);
	if ($main'cpp)
		{ $tmp=push(@out,"TYPE($func,\@function)\n"); }
	elsif ($main'coff)
		{ $tmp=push(@out,".def\t$func;\t.scl\t2;\t.type\t32;\t.endef\n"); }
	elsif ($main'aout and !$main'pic)
		{ }
	else	{ $tmp=push(@out,".type\t$func,\@function\n"); }
	push(@out,".align\t$align\n");
	push(@out,"$func:\n");
	$tmp=<<"EOF";
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

EOF
	push(@out,$tmp);
	$stack=20;
	}

sub main'function_begin_B
	{
	local($func,$extra)=@_;

	&main'external_label($func);
	$func=$under.$func;

	local($tmp)=<<"EOF";
.text
.globl	$func
EOF
	push(@out,$tmp);
	if ($main'cpp)
		{ push(@out,"TYPE($func,\@function)\n"); }
	elsif ($main'coff)
		{ $tmp=push(@out,".def\t$func;\t.scl\t2;\t.type\t32;\t.endef\n"); }
	elsif ($main'aout and !$main'pic)
		{ }
	else	{ push(@out,".type	$func,\@function\n"); }
	push(@out,".align\t$align\n");
	push(@out,"$func:\n");
	$stack=4;
	}

sub main'function_end
	{
	local($func)=@_;

	$func=$under.$func;

	local($tmp)=<<"EOF";
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
${dot}L_${func}_end:
EOF
	push(@out,$tmp);

	if ($main'cpp)
		{ push(@out,"SIZE($func,${dot}L_${func}_end-$func)\n"); }
	elsif ($main'coff or $main'aout)
                { }
	else	{ push(@out,".size\t$func,${dot}L_${func}_end-$func\n"); }
	push(@out,".ident	\"$func\"\n");
	$stack=0;
	%label=();
	}

sub main'function_end_A
	{
	local($func)=@_;

	local($tmp)=<<"EOF";
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
EOF
	push(@out,$tmp);
	}

sub main'function_end_B
	{
	local($func)=@_;

	$func=$under.$func;

	push(@out,"${dot}L_${func}_end:\n");
	if ($main'cpp)
		{ push(@out,"SIZE($func,${dot}L_${func}_end-$func)\n"); }
        elsif ($main'coff or $main'aout)
                { }
	else	{ push(@out,".size\t$func,${dot}L_${func}_end-$func\n"); }
	push(@out,".ident	\"$func\"\n");
	$stack=0;
	%label=();
	}

sub main'wparam
	{
	local($num)=@_;

	return(&main'DWP($stack+$num*4,"esp","",0));
	}

sub main'stack_push
	{
	local($num)=@_;
	$stack+=$num*4;
	&main'sub("esp",$num*4);
	}

sub main'stack_pop
	{
	local($num)=@_;
	$stack-=$num*4;
	&main'add("esp",$num*4);
	}

sub main'swtmp
	{
	return(&main'DWP($_[0]*4,"esp","",0));
	}

# Should use swtmp, which is above esp.  Linix can trash the stack above esp
#sub main'wtmp
#	{
#	local($num)=@_;
#
#	return(&main'DWP(-($num+1)*4,"esp","",0));
#	}

sub main'comment
	{
	if (!defined($com_start) or $main'elf)
		{	# Regarding $main'elf above...
			# GNU and SVR4 as'es use different comment delimiters,
		push(@out,"\n");	# so we just skip ELF comments...
		return;
		}
	foreach (@_)
		{
		if (/^\s*$/)
			{ push(@out,"\n"); }
		else
			{ push(@out,"\t$com_start $_ $com_end\n"); }
		}
	}

sub main'public_label
	{
	$label{$_[0]}="${under}${_[0]}"	if (!defined($label{$_[0]}));
	push(@out,".globl\t$label{$_[0]}\n");
	}

sub main'label
	{
	if (!defined($label{$_[0]}))
		{
		$label{$_[0]}="${dot}${label}${_[0]}";
		$label++;
		}
	return($label{$_[0]});
	}

sub main'set_label
	{
	if (!defined($label{$_[0]}))
		{
		$label{$_[0]}="${dot}${label}${_[0]}";
		$label++;
		}
	if ($_[1]!=0)
		{
		if ($_[1]>1)	{ main'align($_[1]);		}
		else		{ push(@out,".align $align\n");	}
		}
	push(@out,"$label{$_[0]}:\n");
	}

sub main'file_end
	{
	# try to detect if SSE2 or MMX extensions were used on ELF platform...
	if ($main'elf && grep {/\b%[x]*mm[0-7]\b|OPENSSL_ia32cap_P\b/i} @out) {
		local($tmp);

		push (@out,"\n.section\t.bss\n");
		push (@out,".comm\t${under}OPENSSL_ia32cap_P,4,4\n");

		return;
	}

	if ($const ne "")
		{
		push(@out,".section .rodata\n");
		push(@out,$const);
		$const="";
		}
	}

sub main'data_byte
	{
	push(@out,"\t.byte\t".join(',',@_)."\n");
	}

sub main'data_word
	{
	push(@out,"\t.long\t".join(',',@_)."\n");
	}

sub main'align
	{
	my $val=$_[0],$p2,$i;
	if ($main'aout) {
		for ($p2=0;$val!=0;$val>>=1) { $p2++; }
		$val=$p2-1;
		$val.=",0x90";
	}
	push(@out,".align\t$val\n");
	}

# debug output functions: puts, putx, printf

sub main'puts
	{
	&pushvars();
	&main'push('$Lstring' . ++$constl);
	&main'call('puts');
	$stack-=4;
	&main'add("esp",4);
	&popvars();

	$const .= "Lstring$constl:\n\t.string \"@_[0]\"\n";
	}

sub main'putx
	{
	&pushvars();
	&main'push($_[0]);
	&main'push('$Lstring' . ++$constl);
	&main'call('printf');
	&main'add("esp",8);
	$stack-=8;
	&popvars();

	$const .= "Lstring$constl:\n\t.string \"\%X\"\n";
	}

sub main'printf
	{
	$ostack = $stack;
	&pushvars();
	for ($i = @_ - 1; $i >= 0; $i--)
		{
		if ($i == 0) # change this to support %s format strings
			{
			&main'push('$Lstring' . ++$constl);
			$const .= "Lstring$constl:\n\t.string \"@_[$i]\"\n";
			}
		else
			{
			if ($_[$i] =~ /([0-9]*)\(%esp\)/)
				{
				&main'push(($1 + $stack - $ostack) . '(%esp)');
				}
			else
				{
				&main'push($_[$i]);
				}
			}
		}
	&main'call('printf');
	$stack-=4*@_;
	&main'add("esp",4*@_);
	&popvars();
	}

sub pushvars
	{
	&main'pushf();
	&main'push("edx");
	&main'push("ecx");
	&main'push("eax");
	}

sub popvars
	{
	&main'pop("eax");
	&main'pop("ecx");
	&main'pop("edx");
	&main'popf();
	}

sub main'picmeup
	{
	local($dst,$sym)=@_;
	if ($main'cpp)
		{
		local($tmp)=<<___;
#if (defined(ELF) || defined(SOL)) && defined(PIC)
	call	1f
1:	popl	$regs{$dst}
	addl	\$_GLOBAL_OFFSET_TABLE_+[.-1b],$regs{$dst}
	movl	$sym\@GOT($regs{$dst}),$regs{$dst}
#else
	leal	$sym,$regs{$dst}
#endif
___
		push(@out,$tmp);
		}
	elsif ($main'pic && ($main'elf || $main'aout))
		{
		&main'call(&main'label("PIC_me_up"));
		&main'set_label("PIC_me_up");
		&main'blindpop($dst);
		&main'add($dst,"\$${under}_GLOBAL_OFFSET_TABLE_+[.-".
				&main'label("PIC_me_up") . "]");
		&main'mov($dst,&main'DWP($under.$sym."\@GOT",$dst));
		}
	else
		{
		&main'lea($dst,&main'DWP($sym));
		}
	}

sub main'blindpop { &out1("popl",@_); }

sub main'initseg
	{
	local($f)=@_;
	local($tmp);
	if ($main'elf)
		{
		$tmp=<<___;
.section	.init
	call	$under$f
	jmp	.Linitalign
.align	$align
.Linitalign:
___
		}
	elsif ($main'coff)
		{
		$tmp=<<___;	# applies to both Cygwin and Mingw
.section	.ctors
.long	$under$f
___
		}
	elsif ($main'aout)
		{
		local($ctor)="${under}_GLOBAL_\$I\$$f";
		$tmp=".text\n";
		$tmp.=".type	$ctor,\@function\n" if ($main'pic);
		$tmp.=<<___;	# OpenBSD way...
.globl	$ctor
.align	2
$ctor:
	jmp	$under$f
___
		}
	push(@out,$tmp) if ($tmp);
	}

1;
