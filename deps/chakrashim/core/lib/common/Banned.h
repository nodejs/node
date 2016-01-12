//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
/***
* banned.h - list of Microsoft Security Development Lifecycle banned APIs
*
* Purpose:
*       This include file contains a list of banned API which should not be used in new code and
*       removed from legacy code over time
* History
* 01-Jan-2006 - mikehow - Initial Version
* 22-Apr-2008 - mikehow - Updated to SDL 4.1, commented out recommendations and added memcpy
* 26-Jan-2009 - mikehow - Updated to SDL 5.0, made the list sane, added compliance levels
* 10-Feb-2009 - mikehow - Updated based on feedback from MS Office
* 12-May-2009 - jpardue - Updated based on feedback from mikehow (added wmemcpy)
*
***/

// IMPORTANT:
// Some of these functions are Windows specific, so you may want to add *nix specific banned function calls

#ifndef _INC_BANNED
#   define _INC_BANNED
#endif

#ifdef _MSC_VER
#   pragma once
#   pragma deprecated (strcpy, strcpyA, strcpyW, wcscpy, _tcscpy, _mbscpy, StrCpy, StrCpyA, StrCpyW, lstrcpy, lstrcpyA, lstrcpyW, _tccpy, _mbccpy, _ftcscpy)
#   pragma deprecated (strcat, strcatA, strcatW, wcscat, _tcscat, _mbscat, StrCat, StrCatA, StrCatW, lstrcat, lstrcatA, lstrcatW, StrCatBuff, StrCatBuffA, StrCatBuffW, StrCatChainW, _tccat, _mbccat, _ftcscat)
#   pragma deprecated (wvsprintf, wvsprintfA, wvsprintfW, vsprintf, _vstprintf, vswprintf)
#   pragma deprecated (strncpy, wcsncpy, _tcsncpy, _mbsncpy, _mbsnbcpy, StrCpyN, StrCpyNA, StrCpyNW, StrNCpy, strcpynA, StrNCpyA, StrNCpyW, lstrcpyn, lstrcpynA, lstrcpynW)
#   pragma deprecated (strncat, wcsncat, _tcsncat, _mbsncat, _mbsnbcat, StrCatN, StrCatNA, StrCatNW, StrNCat, StrNCatA, StrNCatW, lstrncat, lstrcatnA, lstrcatnW, lstrcatn)
#   pragma deprecated (IsBadWritePtr, IsBadHugeWritePtr, IsBadReadPtr, IsBadHugeReadPtr, IsBadCodePtr, IsBadStringPtr)
#   pragma deprecated (gets, _getts, _gettws)
#   pragma deprecated (RtlCopyMemory, CopyMemory)

#   pragma deprecated (wnsprintf, wnsprintfA, wnsprintfW, sprintfW, sprintfA, wsprintf, wsprintfW, wsprintfA, sprintf, swprintf, _stprintf, _snwprintf, _snprintf, _sntprintf)
#   pragma deprecated (_vsnprintf, vsnprintf, _vsnwprintf, _vsntprintf, wvnsprintf, wvnsprintfA, wvnsprintfW)
#   pragma deprecated (strtok, _tcstok, wcstok, _mbstok)
#   pragma deprecated (makepath, _tmakepath, _makepath, _wmakepath)
#   pragma deprecated (_splitpath, _tsplitpath, _wsplitpath)
#   pragma deprecated (scanf, wscanf, _tscanf, sscanf, swscanf, _stscanf, snscanf, snwscanf, _sntscanf)
#   pragma deprecated (_itoa, _itow, _i64toa, _i64tow, _ui64toa, _ui64tot, _ui64tow, _ultoa, _ultot, _ultow)


#if (_SDL_BANNED_LEVEL3)
#   pragma deprecated (CharToOem, CharToOemA, CharToOemW, OemToChar, OemToCharA, OemToCharW, CharToOemBuffA, CharToOemBuffW)
#   pragma deprecated (alloca, _alloca)
#   pragma deprecated (strlen, wcslen, _mbslen, _mbstrlen, StrLen, lstrlen)
#   pragma deprecated (ChangeWindowMessageFilter)
#endif

#ifndef PATHCCH_NO_DEPRECATE
// Path APIs which assume MAX_PATH instead of requiring the caller to specify
// the buffer size have been deprecated. Include <PathCch.h> and use the PathCch
// equivalents instead.
#   pragma deprecated (PathAddBackslash, PathAddBackslashA, PathAddBackslashW)
#   pragma deprecated (PathAddExtension, PathAddExtensionA, PathAddExtensionW)
#   pragma deprecated (PathAppend, PathAppendA, PathAppendW)
#   pragma deprecated (PathCanonicalize, PathCanonicalizeA, PathCanonicalizeW)
#   pragma deprecated (PathCombine, PathCombineA, PathCombineW)
#   pragma deprecated (PathRenameExtension, PathRenameExtensionA, PathRenameExtensionW)
#endif // PATHCCH_NO_DEPRECATE

#else // _MSC_VER

#endif  /* _INC_BANNED */
