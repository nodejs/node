#---------------------------------------------------------------------------
#
# xc-translit.m4
#
# Copyright (c) Daniel Stenberg <daniel@haxx.se>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#
# SPDX-License-Identifier: MIT
#---------------------------------------------------------------------------

# File version for 'aclocal' use. Keep it a single number.
# serial 2


dnl XC_SH_TR_SH (expression)
dnl -------------------------------------------------
dnl Shell execution time transliteration of 'expression'
dnl argument, where all non-alfanumeric characters are
dnl converted to the underscore '_' character.
dnl Normal shell expansion and substitution takes place
dnl for given 'expression' at shell execution time before
dnl transliteration is applied to it.

AC_DEFUN([XC_SH_TR_SH],
[`echo "$1" | sed 's/[[^a-zA-Z0-9_]]/_/g'`])


dnl XC_SH_TR_SH_EX (expression, [extra])
dnl -------------------------------------------------
dnl Like XC_SH_TR_SH but transliterating characters
dnl given in 'extra' argument to lowercase 'p'. For
dnl example [*+], [*], and [+] are valid 'extra' args.

AC_DEFUN([XC_SH_TR_SH_EX],
[ifelse([$2], [],
  [XC_SH_TR_SH([$1])],
  [`echo "$1" | sed 's/[[$2]]/p/g' | sed 's/[[^a-zA-Z0-9_]]/_/g'`])])


dnl XC_M4_TR_SH (expression)
dnl -------------------------------------------------
dnl m4 execution time transliteration of 'expression'
dnl argument, where all non-alfanumeric characters are
dnl converted to the underscore '_' character.

AC_DEFUN([XC_M4_TR_SH],
[patsubst(XC_QPATSUBST(XC_QUOTE($1),
                       [[^a-zA-Z0-9_]], [_]),
          [\(_\(.*\)_\)], [\2])])


dnl XC_M4_TR_SH_EX (expression, [extra])
dnl -------------------------------------------------
dnl Like XC_M4_TR_SH but transliterating characters
dnl given in 'extra' argument to lowercase 'p'. For
dnl example [*+], [*], and [+] are valid 'extra' args.

AC_DEFUN([XC_M4_TR_SH_EX],
[ifelse([$2], [],
  [XC_M4_TR_SH([$1])],
  [patsubst(XC_QPATSUBST(XC_QPATSUBST(XC_QUOTE($1),
                                      [[$2]],
                                      [p]),
                         [[^a-zA-Z0-9_]], [_]),
            [\(_\(.*\)_\)], [\2])])])


dnl XC_SH_TR_CPP (expression)
dnl -------------------------------------------------
dnl Shell execution time transliteration of 'expression'
dnl argument, where all non-alfanumeric characters are
dnl converted to the underscore '_' character and alnum
dnl characters are converted to uppercase.
dnl Normal shell expansion and substitution takes place
dnl for given 'expression' at shell execution time before
dnl transliteration is applied to it.

AC_DEFUN([XC_SH_TR_CPP],
[`echo "$1" | dnl
sed 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' | dnl
sed 's/[[^A-Z0-9_]]/_/g'`])


dnl XC_SH_TR_CPP_EX (expression, [extra])
dnl -------------------------------------------------
dnl Like XC_SH_TR_CPP but transliterating characters
dnl given in 'extra' argument to uppercase 'P'. For
dnl example [*+], [*], and [+] are valid 'extra' args.

AC_DEFUN([XC_SH_TR_CPP_EX],
[ifelse([$2], [],
  [XC_SH_TR_CPP([$1])],
  [`echo "$1" | dnl
sed 's/[[$2]]/P/g' | dnl
sed 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' | dnl
sed 's/[[^A-Z0-9_]]/_/g'`])])


dnl XC_M4_TR_CPP (expression)
dnl -------------------------------------------------
dnl m4 execution time transliteration of 'expression'
dnl argument, where all non-alfanumeric characters are
dnl converted to the underscore '_' character and alnum
dnl characters are converted to uppercase.

AC_DEFUN([XC_M4_TR_CPP],
[patsubst(XC_QPATSUBST(XC_QTRANSLIT(XC_QUOTE($1),
                                    [abcdefghijklmnopqrstuvwxyz],
                                    [ABCDEFGHIJKLMNOPQRSTUVWXYZ]),
                       [[^A-Z0-9_]], [_]),
          [\(_\(.*\)_\)], [\2])])


dnl XC_M4_TR_CPP_EX (expression, [extra])
dnl -------------------------------------------------
dnl Like XC_M4_TR_CPP but transliterating characters
dnl given in 'extra' argument to uppercase 'P'. For
dnl example [*+], [*], and [+] are valid 'extra' args.

AC_DEFUN([XC_M4_TR_CPP_EX],
[ifelse([$2], [],
  [XC_M4_TR_CPP([$1])],
  [patsubst(XC_QPATSUBST(XC_QTRANSLIT(XC_QPATSUBST(XC_QUOTE($1),
                                                   [[$2]],
                                                   [P]),
                                      [abcdefghijklmnopqrstuvwxyz],
                                      [ABCDEFGHIJKLMNOPQRSTUVWXYZ]),
                         [[^A-Z0-9_]], [_]),
            [\(_\(.*\)_\)], [\2])])])


dnl XC_QUOTE (expression)
dnl -------------------------------------------------
dnl Expands to quoted result of 'expression' expansion.

AC_DEFUN([XC_QUOTE],
[[$@]])


dnl XC_QPATSUBST (string, regexp[, repl])
dnl -------------------------------------------------
dnl Expands to quoted result of 'patsubst' expansion.

AC_DEFUN([XC_QPATSUBST],
[XC_QUOTE(patsubst([$1], [$2], [$3]))])


dnl XC_QTRANSLIT (string, chars, repl)
dnl -------------------------------------------------
dnl Expands to quoted result of 'translit' expansion.

AC_DEFUN([XC_QTRANSLIT],
[XC_QUOTE(translit([$1], [$2], [$3]))])

