#!/bin/sh

#  make_zip_errrors.sh: create zip_errors.mdoc from zip.h
#  Copyright (C) 1999-2013 Dieter Baron and Thomas Klausner
#
#  This file is part of libzip, a library to manipulate ZIP archives.
#  The authors can be contacted at <info@libzip.org>
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#  3. The names of the authors may not be used to endorse or promote
#     products derived from this software without specific prior
#     written permission.
# 
#  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
#  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
#  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
#  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
#  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
#  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


if [ "$#" -ne 2 ]
then
    echo "Usage: $0 in_file out_file" >&2
    echo "       e.g. $0 zip.h zip_errors.mdoc" >&2
    exit 1
fi

if [ "$1" = "$2" ]
then
    echo "$0: error: output file = input file" >&2
    exit 1
fi

date=`LC_TIME=en_US date '+%B %e, %Y' | sed 's/  / /'`

cat <<EOF >> "$2.$$" || exit 1
.\" zip_errors.mdoc -- list of all libzip error codes
.\" Copyright (C) 1999-2013 Dieter Baron and Thomas Klausner
.\"
.\" This file is part of libzip, a library to manipulate ZIP archives.
.\" The authors can be contacted at <info@libzip.org>
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in
.\"    the documentation and/or other materials provided with the
.\"    distribution.
.\" 3. The names of the authors may not be used to endorse or promote
.\"    products derived from this software without specific prior
.\"    written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE AUTHORS \`\`AS IS'' AND ANY EXPRESS
.\" OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
.\" WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
.\" DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
.\" GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
.\" INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
.\" IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
.\" OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
.\" IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
.\"
.\"   This file was generated automatically by $0
.\"   from $1; make changes there.
.\"
.Dd $date
.Dt ZIP_ERRORS 3
.Os
.Sh NAME
.Nm zip_errors
.Nd list of all libzip error codes
.Sh LIBRARY
libzip (-lzip)
.Sh SYNOPSIS
.In zip.h
.Sh DESCRIPTION
The following error codes are used by libzip:
.Bl -tag -width XZIPXERXCOMPNOTSUPPXX
EOF

sed -n  's/^#define \(ZIP_ER_[A-Z_0-9]*\).*\/\* \(.\) \([^*]*\) \*\//.It Bq Er \1@\3./p' "$1" \
    | sort\
    | tr @ '\012' \
    >> "$2.$$" || exit 1

cat <<EOF >> "$2.$$" || exit 1
.El
.Sh AUTHORS
.An -nosplit
.An Dieter Baron Aq Mt dillo@nih.at
and
.An Thomas Klausner Aq Mt tk@giga.or.at
EOF

mv "$2.$$" "$2" || exit 1
