//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include <ParserPch.h>
#include "rtError.h"

// PUBLIC ERROR codes

// verify HR is as-expected for the Legacy (private) error JSERR_CantExecute
C_ASSERT(JSCRIPT_E_CANTEXECUTE != JSERR_CantExecute);
// verify the HR value (as MAKE_HRESULT(SEVERITY_ERROR, FACILITY_CONTROL, 0x1393))
C_ASSERT(JSERR_CantExecute == 0x800A1393);

// verify HR matches between public SDK and private (.h) files
C_ASSERT(JSCRIPT_E_CANTEXECUTE == JSPUBLICERR_CantExecute);
// verify the HR value (as MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSCRIPT, 0x0001))
C_ASSERT(JSPUBLICERR_CantExecute == 0x89020001L);

// /PUBLIC ERROR codes

// boundary check - all errNum should be capped to 10,000 (RTERROR_STRINGFORMAT_OFFSET) - except for VBSERR_CantDisplayDate==32812
#define VERIFY_BOUNDARY_ERRNUM(name,errnum) C_ASSERT(name == VBSERR_CantDisplayDate || errnum < RTERROR_STRINGFORMAT_OFFSET);

#define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) VERIFY_BOUNDARY_ERRNUM(name, errnum)
#define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) VERIFY_BOUNDARY_ERRNUM(name, errnum)
#include "rterrors.h"
#undef  RT_PUBLICERROR_MSG
#undef  RT_ERROR_MSG
