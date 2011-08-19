#!/usr/bin/env perl

my $quiet = 1;

unpack("L",pack("N",1))!=1 || die "only little-endian hosts are supported";

# first argument can specify custom suffix...
$suffix=(@ARGV[0]=~/^\$/) ? shift(@ARGV) : "\$m";
#################################################################
# rename segments in COFF modules according to %map table below	#
%map=(	".text" => "fipstx$suffix",				#
	".text\$"=> "fipstx$suffix",				#
	".rdata"=> "fipsrd$suffix",				#
	".data" => "fipsda$suffix"	);			#
#################################################################

# collect file list
foreach (@ARGV) {
    if (/\*/)	{ push(@files,glob($_)); }
    else	{ push(@files,$_);       }
}

use Fcntl;
use Fcntl ":seek";

foreach (@files) {
    $file=$_;
    print "processing $file\n" unless $quiet;

    sysopen(FD,$file,O_RDWR|O_BINARY) || die "sysopen($file): $!";

    # read IMAGE_DOS_HEADER
    sysread(FD,$mz,64)==64 || die "$file is too short";
    @dos_header=unpack("a2C58I",$mz);
    if (@dos_header[0] eq "MZ") {
	$e_lfanew=pop(@dos_header);
	sysseek(FD,$e_lfanew,SEEK_SET)	|| die "$file is too short";
	sysread(FD,$Magic,4)==4		|| die "$file is too short";
	unpack("I",$Magic)==0x4550	|| die "$file is not COFF image";
    } elsif ($file =~ /\.obj$/i) {
	# .obj files have no IMAGE_DOS_HEADER
	sysseek(FD,0,SEEK_SET)		|| die "unable to rewind $file";
    } else { next; }

    # read IMAGE_FILE_HEADER
    sysread(FD,$coff,20)==20 || die "$file is too short";
    ($Machine,$NumberOfSections,$TimeDateStamp,
     $PointerToSymbolTable,$NumberOfSysmbols,
     $SizeOfOptionalHeader,$Characteristics)=unpack("SSIIISS",$coff);

    # skip over IMAGE_OPTIONAL_HEADER
    sysseek(FD,$SizeOfOptionalHeader,SEEK_CUR) || die "$file is too short";

    # traverse IMAGE_SECTION_HEADER table
    for($i=0;$i<$NumberOfSections;$i++) {
	sysread(FD,$SectionHeader,40)==40 || die "$file is too short";
	($Name,@opaque)=unpack("Z8C*",$SectionHeader);
	if ($map{$Name}) {
	    sysseek(FD,-40,SEEK_CUR) || die "unable to rewind $file";
	    syswrite(FD,pack("a8C*",$map{$Name},@opaque))==40 || die "syswrite failed: $!";
	    printf "    %-8s -> %.8s\n",$Name,$map{$Name} unless $quiet;
	}
    }
    close(FD);
}
