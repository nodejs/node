// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//{{NO_DEPENDENCIES}}
// Copyright (c) 2003-2010 International Business Machines
// Corporation and others. All Rights Reserved.
//
// Used by common.rc and other .rc files.
//Do not edit with Microsoft Developer Studio because it will modify this
//header the wrong way. This is here to prevent Visual Studio .NET from
//unnessarily building the resource files when it's not needed.
//

/*
These are defined before unicode/uversion.h in order to prevent
STLPort's broken stddef.h from being used when rc.exe parses this file.
*/
#define _STLP_OUTERMOST_HEADER_ID 0
#define _STLP_WINCE 1

#include "unicode/uversion.h"

#define ICU_WEBSITE "http://icu-project.org"
#define ICU_COMPANY "The ICU Project"
#define ICU_PRODUCT_PREFIX "ICU"
#define ICU_PRODUCT "International Components for Unicode"
