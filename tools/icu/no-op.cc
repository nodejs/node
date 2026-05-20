/*
**********************************************************************
*   Copyright (C) 2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
*/

//
// ICU needs the C++, not the C linker to be used, even if the main function
// is in C.
//
// This is a dummy function just to get gyp to compile some internal
// tools as C++.
//
// It should not appear in production node binaries.

extern void icu_dummy_cxx() {}
