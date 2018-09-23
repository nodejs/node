#!/bin/env perl
#
open FD,"crypto/opensslv.h";
while(<FD>) {
    if (/OPENSSL_VERSION_NUMBER\s+(0x[0-9a-f]+)/i) {
	$ver = hex($1);
	$v1 = ($ver>>28);
	$v2 = ($ver>>20)&0xff;
	$v3 = ($ver>>12)&0xff;
	$v4 = ($ver>> 4)&0xff;
	$beta = $ver&0xf;
	$version = "$v1.$v2.$v3";
	if ($beta==0xf)	{ $version .= chr(ord('a')+$v4-1) if ($v4);	}
	elsif ($beta==0){ $version .= "-dev";				}
	else		{ $version .= "-beta$beta";			}
	last;
    }
}
close(FD);

$filename = $ARGV[0]; $filename =~ /(.*)\.([^.]+)$/;
$basename = $1;
$extname  = $2;

if ($extname =~ /dll/i)	{ $description = "OpenSSL shared library"; }
else			{ $description = "OpenSSL application";    }

print <<___;
#include <winver.h>

LANGUAGE 0x09,0x01

1 VERSIONINFO
  FILEVERSION $v1,$v2,$v3,$v4
  PRODUCTVERSION $v1,$v2,$v3,$v4
  FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
  FILEFLAGS 0x01L
#else
  FILEFLAGS 0x00L
#endif
  FILEOS VOS__WINDOWS32
  FILETYPE VFT_DLL
  FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            // Required:
            VALUE "CompanyName", "The OpenSSL Project, http://www.openssl.org/\\0"
            VALUE "FileDescription", "$description\\0"
            VALUE "FileVersion", "$version\\0"
            VALUE "InternalName", "$basename\\0"
            VALUE "OriginalFilename", "$filename\\0"
            VALUE "ProductName", "The OpenSSL Toolkit\\0"
            VALUE "ProductVersion", "$version\\0"
            // Optional:
            //VALUE "Comments", "\\0"
            VALUE "LegalCopyright", "Copyright © 1998-2006 The OpenSSL Project. Copyright © 1995-1998 Eric A. Young, Tim J. Hudson. All rights reserved.\\0"
            //VALUE "LegalTrademarks", "\\0"
            //VALUE "PrivateBuild", "\\0"
            //VALUE "SpecialBuild", "\\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 0x4b0
    END
END
___
