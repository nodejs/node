#!/usr/local/bin/perl

package x86nasm;

$label="L000";
$under=($main'netware)?'':'_';

%lb=(	'eax',	'al',
	'ebx',	'bl',
	'ecx',	'cl',
	'edx',	'dl',
	'ax',	'al',
	'bx',	'bl',
	'cx',	'cl',
	'dx',	'dl',
	);

%hb=(	'eax',	'ah',
	'ebx',	'bh',
	'ecx',	'ch',
	'edx',	'dh',
	'ax',	'ah',
	'bx',	'bh',
	'cx',	'ch',
	'dx',	'dh',
	);

sub main'asm_init_output { @out=(); }
sub main'asm_get_output { return(@out); }
sub main'get_labels { return(@labels); }

sub main'external_label
{
	push(@labels,@_);
	foreach (@_) {
		push(@out,".") if ($main'mwerks);
		push(@out, "extern\t${under}$_\n");
	}
}

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

sub main'BP
	{
	&get_mem("BYTE",@_);
	}

sub main'DWP
	{
	&get_mem("DWORD",@_);
	}

sub main'QWP
	{
	&get_mem("",@_);
	}

sub main'BC
	{
	return (($main'mwerks)?"":"BYTE ")."@_";
	}

sub main'DWC
	{
	return (($main'mwerks)?"":"DWORD ")."@_";
	}

sub main'stack_push
	{
	my($num)=@_;
	$stack+=$num*4;
	&main'sub("esp",$num*4);
	}

sub main'stack_pop
	{
	my($num)=@_;
	$stack-=$num*4;
	&main'add("esp",$num*4);
	}

sub get_mem
	{
	my($size,$addr,$reg1,$reg2,$idx)=@_;
	my($t,$post);
	my($ret)=$size;
	if ($ret ne "")
		{
		$ret .= " PTR" if ($main'mwerks);
		$ret .= " ";
		}
	$ret .= "[";
	$addr =~ s/^\s+//;
	if ($addr =~ /^(.+)\+(.+)$/)
		{
		$reg2=&conv($1);
		$addr="$under$2";
		}
	elsif ($addr =~ /^[_a-z][_a-z0-9]*$/i)
		{
		$addr="$under$addr";
		}

	if ($addr =~ /^.+\-.+$/) { $addr="($addr)"; }

	$reg1="$regs{$reg1}" if defined($regs{$reg1});
	$reg2="$regs{$reg2}" if defined($regs{$reg2});
	if (($addr ne "") && ($addr ne 0))
		{
		if ($addr !~ /^-/)
			{ $ret.="${addr}+"; }
		else	{ $post=$addr; }
		}
	if ($reg2 ne "")
		{
		$t="";
		$t="*$idx" if ($idx != 0);
		$reg1="+".$reg1 if ("$reg1$post" ne "");
		$ret.="$reg2$t$reg1$post]";
		}
	else
		{
		$ret.="$reg1$post]"
		}
	$ret =~ s/\+\]/]/; # in case $addr was the only argument
	return($ret);
	}

sub main'mov	{ &out2("mov",@_); }
sub main'movb	{ &out2("mov",@_); }
sub main'and	{ &out2("and",@_); }
sub main'or	{ &out2("or",@_); }
sub main'shl	{ &out2("shl",@_); }
sub main'shr	{ &out2("shr",@_); }
sub main'xor	{ &out2("xor",@_); }
sub main'xorb	{ &out2("xor",@_); }
sub main'add	{ &out2("add",@_); }
sub main'adc	{ &out2("adc",@_); }
sub main'sub	{ &out2("sub",@_); }
sub main'sbb	{ &out2("sbb",@_); }
sub main'rotl	{ &out2("rol",@_); }
sub main'rotr	{ &out2("ror",@_); }
sub main'exch	{ &out2("xchg",@_); }
sub main'cmp	{ &out2("cmp",@_); }
sub main'lea	{ &out2("lea",@_); }
sub main'mul	{ &out1("mul",@_); }
sub main'imul	{ &out2("imul",@_); }
sub main'div	{ &out1("div",@_); }
sub main'dec	{ &out1("dec",@_); }
sub main'inc	{ &out1("inc",@_); }
sub main'jmp	{ &out1("jmp",@_); }
sub main'jmp_ptr { &out1p("jmp",@_); }

# This is a bit of a kludge: declare all branches as NEAR.
$near=($main'mwerks)?'':'NEAR';
sub main'je	{ &out1("je $near",@_); }
sub main'jle	{ &out1("jle $near",@_); }
sub main'jz	{ &out1("jz $near",@_); }
sub main'jge	{ &out1("jge $near",@_); }
sub main'jl	{ &out1("jl $near",@_); }
sub main'ja	{ &out1("ja $near",@_); }
sub main'jae	{ &out1("jae $near",@_); }
sub main'jb	{ &out1("jb $near",@_); }
sub main'jbe	{ &out1("jbe $near",@_); }
sub main'jc	{ &out1("jc $near",@_); }
sub main'jnc	{ &out1("jnc $near",@_); }
sub main'jnz	{ &out1("jnz $near",@_); }
sub main'jne	{ &out1("jne $near",@_); }
sub main'jno	{ &out1("jno $near",@_); }

sub main'push	{ &out1("push",@_); $stack+=4; }
sub main'pop	{ &out1("pop",@_); $stack-=4; }
sub main'pushf	{ &out0("pushfd"); $stack+=4; }
sub main'popf	{ &out0("popfd"); $stack-=4; }
sub main'bswap	{ &out1("bswap",@_); &using486(); }
sub main'not	{ &out1("not",@_); }
sub main'call	{ &out1("call",($_[0]=~/^\@L/?'':$under).$_[0]); }
sub main'call_ptr { &out1p("call",@_); }
sub main'ret	{ &out0("ret"); }
sub main'nop	{ &out0("nop"); }
sub main'test	{ &out2("test",@_); }
sub main'bt	{ &out2("bt",@_); }
sub main'leave	{ &out0("leave"); }
sub main'cpuid	{ &out0("cpuid"); }
sub main'rdtsc	{ &out0("rdtsc"); }
sub main'halt	{ &out0("hlt"); }
sub main'movz	{ &out2("movzx",@_); }
sub main'neg	{ &out1("neg",@_); }
sub main'cld	{ &out0("cld"); }

# SSE2
sub main'emms	{ &out0("emms"); }
sub main'movd	{ &out2("movd",@_); }
sub main'movq	{ &out2("movq",@_); }
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

sub out2
	{
	my($name,$p1,$p2)=@_;
	my($l,$t);

	push(@out,"\t$name\t");
	if (!$main'mwerks and $name eq "lea")
		{
		$p1 =~ s/^[^\[]*\[/\[/;
		$p2 =~ s/^[^\[]*\[/\[/;
		}
	$t=&conv($p1).",";
	$l=length($t);
	push(@out,$t);
	$l=4-($l+9)/8;
	push(@out,"\t" x $l);
	push(@out,&conv($p2));
	push(@out,"\n");
	}

sub out0
	{
	my($name)=@_;

	push(@out,"\t$name\n");
	}

sub out1
	{
	my($name,$p1)=@_;
	my($l,$t);
	push(@out,"\t$name\t".&conv($p1)."\n");
	}

sub conv
	{
	my($p)=@_;
	$p =~ s/0x([0-9A-Fa-f]+)/0$1h/;
	return $p;
	}

sub using486
	{
	return if $using486;
	$using486++;
	grep(s/\.386/\.486/,@out);
	}

sub main'file
	{
	if ($main'mwerks)	{ push(@out,".section\t.text\n"); }
	else	{
		local $tmp=<<___;
%ifdef __omf__
section	code	use32 class=code
%else
section	.text
%endif
___
		push(@out,$tmp);
		}
	}

sub main'function_begin
	{
	my($func,$extra)=@_;

	push(@labels,$func);
	push(@out,".") if ($main'mwerks);
	my($tmp)=<<"EOF";
global	$under$func
$under$func:
	push	ebp
	push	ebx
	push	esi
	push	edi
EOF
	push(@out,$tmp);
	$stack=20;
	}

sub main'function_begin_B
	{
	my($func,$extra)=@_;
	push(@out,".") if ($main'mwerks);
	my($tmp)=<<"EOF";
global	$under$func
$under$func:
EOF
	push(@out,$tmp);
	$stack=4;
	}

sub main'function_end
	{
	my($func)=@_;

	my($tmp)=<<"EOF";
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
EOF
	push(@out,$tmp);
	$stack=0;
	%label=();
	}

sub main'function_end_B
	{
	$stack=0;
	%label=();
	}

sub main'function_end_A
	{
	my($func)=@_;

	my($tmp)=<<"EOF";
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
EOF
	push(@out,$tmp);
	}

sub main'file_end
	{
	}

sub main'wparam
	{
	my($num)=@_;

	return(&main'DWP($stack+$num*4,"esp","",0));
	}

sub main'swtmp
	{
	return(&main'DWP($_[0]*4,"esp","",0));
	}

# Should use swtmp, which is above esp.  Linix can trash the stack above esp
#sub main'wtmp
#	{
#	my($num)=@_;
#
#	return(&main'DWP(-(($num+1)*4),"esp","",0));
#	}

sub main'comment
	{
	foreach (@_)
		{
		push(@out,"\t; $_\n");
		}
	}

sub main'public_label
	{
	$label{$_[0]}="${under}${_[0]}"	if (!defined($label{$_[0]}));
	push(@out,".") if ($main'mwerks);
	push(@out,"global\t$label{$_[0]}\n");
	}

sub main'label
	{
	if (!defined($label{$_[0]}))
		{
		$label{$_[0]}="\@${label}${_[0]}";
		$label++;
		}
	return($label{$_[0]});
	}

sub main'set_label
	{
	if (!defined($label{$_[0]}))
		{
		$label{$_[0]}="\@${label}${_[0]}";
		$label++;
		}
	if ($_[1]!=0 && $_[1]>1)
		{
		main'align($_[1]);
		}
	push(@out,"$label{$_[0]}:\n");
	}

sub main'data_byte
	{
	push(@out,(($main'mwerks)?".byte\t":"DB\t").join(',',@_)."\n");
	}

sub main'data_word
	{
	push(@out,(($main'mwerks)?".long\t":"DD\t").join(',',@_)."\n");
	}

sub main'align
	{
	push(@out,".") if ($main'mwerks);
	push(@out,"align\t$_[0]\n");
	}

sub out1p
	{
	my($name,$p1)=@_;
	my($l,$t);

	push(@out,"\t$name\t".&conv($p1)."\n");
	}

sub main'picmeup
	{
	local($dst,$sym)=@_;
	&main'lea($dst,&main'DWP($sym));
	}

sub main'blindpop { &out1("pop",@_); }

sub main'initseg
	{
	local($f)=@_;
	if ($main'win32)
		{
		local($tmp)=<<___;
segment	.CRT\$XCU data
extern	$under$f
DD	$under$f
___
		push(@out,$tmp);
		}
	}

1;
