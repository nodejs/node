//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#pragma hdrstop
#include "errstr.h"

void CopyException (EXCEPINFO *peiDest, const EXCEPINFO *peiSource)
{
    FreeExcepInfo(peiDest);
    *peiDest = *peiSource;
    if (peiSource->bstrSource) {
        peiDest->bstrSource =
            SysAllocStringLen(peiSource->bstrSource, SysStringLen(peiSource->bstrSource));
    }
    if (peiSource->bstrDescription) {
        peiDest->bstrDescription =
            SysAllocStringLen(peiSource->bstrDescription, SysStringLen(peiSource->bstrDescription));
    }
    if (peiSource->bstrHelpFile) {
        peiDest->bstrHelpFile =
            SysAllocStringLen(peiSource->bstrHelpFile, SysStringLen(peiSource->bstrHelpFile));
    }
}

/***
*BOOL FSupportsErrorInfo
*Purpose:
*  Answers if the given object supports the Rich Error mechanism
*  on the given interface.
*
*Entry:
*  punk = the object
*  riid = the IID of the interface on the object
*
*Exit:
*  return value = BOOL
*
***********************************************************************/
BOOL FSupportsErrorInfo(IUnknown *punk, REFIID riid)
{
    BOOL fSupports;
    ISupportErrorInfo *psupport;

    fSupports = FALSE;
    if(SUCCEEDED(punk->QueryInterface(__uuidof(ISupportErrorInfo), (void **)&psupport)))
    {
        if(NOERROR == psupport->InterfaceSupportsErrorInfo(riid))
            fSupports = TRUE;
        psupport->Release();
    }
    return fSupports;
}

/***
*PUBLIC HRESULT GetErrorInfo
*Purpose:
*  Filling the given EXCEPINFO structure from the contents of
*  the current OLE error object (if any).
*
*Entry:
*  pexcepinfo = pointer to caller allocated EXCEPINFO to fillin.
*
*Exit:
*  return value = HRESULT. S_OK if obtained info, else S_FALSE
*
*Note:
*  This routine assumes that the given EXCEPINFO does *not* contain
*  any strings that need to be freed before its contents are set.
*
***********************************************************************/
HRESULT GetErrorInfo(EXCEPINFO *pexcepinfo)
{
    HRESULT hr;

    memset(pexcepinfo, 0, sizeof(*pexcepinfo));
    IErrorInfo *perrinfo;

    // GetErrorInfo returns S_FALSE if there is no rich error info
    // and S_OK if there is.

    if(NOERROR == (hr = GetErrorInfo(0L, &perrinfo)))
    {
        perrinfo->GetSource(&pexcepinfo->bstrSource);
        perrinfo->GetDescription(&pexcepinfo->bstrDescription);
        perrinfo->GetHelpFile(&pexcepinfo->bstrHelpFile);
        perrinfo->GetHelpContext(&pexcepinfo->dwHelpContext);
        perrinfo->Release();
    }

    return hr;
}



/***************************************************************************
HRESULT mapping
***************************************************************************/
template <rtErrors errnum> class ErrorTypeMap;

#define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
    template <> class ErrorTypeMap<name>  \
    { \
    public: \
        static const ErrorTypeEnum Type = jst; \
    };
#define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
#include "rterrors.h"
#undef RT_PUBLICERROR_MSG
#undef RT_ERROR_MSG

struct MHR
{
    HRESULT hrIn;
    HRESULT hrOut;
    ErrorTypeEnum errorType;
};

// This table maps OLE errors to JScript errors. The comment on each line
// shows the numeric value. The table must be sorted so we can do a binary
// search on it.
#define MAPHR(in, out) { HR(in), out, ErrorTypeMap<out>::Type }

const MHR g_rgmhr[] =
{
    // FACILITY_NULL errors
#if _WIN32 || _WIN64
    /*0x80004001*/ MAPHR(E_NOTIMPL, VBSERR_ActionNotSupported),
    /*0x80004002*/ MAPHR(E_NOINTERFACE, VBSERR_OLENotSupported),
#else
#error Neither __WIN32, nor _WIN64 is defined
#endif

    // FACILITY_DISPATCH - IDispatch errors.
    /*0x80020001*/ MAPHR(DISP_E_UNKNOWNINTERFACE, VBSERR_OLENoPropOrMethod),
    /*0x80020003*/ MAPHR(DISP_E_MEMBERNOTFOUND, VBSERR_OLENoPropOrMethod),
    /*0x80020004*/ MAPHR(DISP_E_PARAMNOTFOUND, VBSERR_NamedParamNotFound),
    /*0x80020005*/ MAPHR(DISP_E_TYPEMISMATCH, VBSERR_TypeMismatch),
    /*0x80020006*/ MAPHR(DISP_E_UNKNOWNNAME, VBSERR_OLENoPropOrMethod),
    /*0x80020007*/ MAPHR(DISP_E_NONAMEDARGS, VBSERR_NamedArgsNotSupported),
    /*0x80020008*/ MAPHR(DISP_E_BADVARTYPE, VBSERR_InvalidTypeLibVariable),
    /*0x8002000A*/ MAPHR(DISP_E_OVERFLOW, VBSERR_Overflow),
    /*0x8002000B*/ MAPHR(DISP_E_BADINDEX, VBSERR_OutOfBounds),
    /*0x8002000C*/ MAPHR(DISP_E_UNKNOWNLCID, VBSERR_LocaleSettingNotSupported),
    /*0x8002000D*/ MAPHR(DISP_E_ARRAYISLOCKED, VBSERR_ArrayLocked),
    /*0x8002000E*/ MAPHR(DISP_E_BADPARAMCOUNT, VBSERR_FuncArityMismatch),
    /*0x8002000F*/ MAPHR(DISP_E_PARAMNOTOPTIONAL, VBSERR_ParameterNotOptional),
    /*0x80020011*/ MAPHR(DISP_E_NOTACOLLECTION, VBSERR_NotEnum),

    // FACILITY_DISPATCH - Typelib errors.
    /*0x8002802F*/ MAPHR(TYPE_E_DLLFUNCTIONNOTFOUND, VBSERR_InvalidDllFunctionName),
    /*0x80028CA0*/ MAPHR(TYPE_E_TYPEMISMATCH, VBSERR_TypeMismatch),
    /*0x80028CA1*/ MAPHR(TYPE_E_OUTOFBOUNDS, VBSERR_OutOfBounds),
    /*0x80028CA2*/ MAPHR(TYPE_E_IOERROR, VBSERR_IOError),
    /*0x80028CA3*/ MAPHR(TYPE_E_CANTCREATETMPFILE, VBSERR_CantCreateTmpFile),
    /*0x80029C4A*/ MAPHR(TYPE_E_CANTLOADLIBRARY, VBSERR_DLLLoadErr),

    // FACILITY_STORAGE errors
    /*0x80030002*/ MAPHR(STG_E_FILENOTFOUND, VBSERR_OLEFileNotFound),
    /*0x80030003*/ MAPHR(STG_E_PATHNOTFOUND, VBSERR_PathNotFound),
    /*0x80030004*/ MAPHR(STG_E_TOOMANYOPENFILES, VBSERR_TooManyFiles),
    /*0x80030005*/ MAPHR(STG_E_ACCESSDENIED, VBSERR_PermissionDenied),
    /*0x80030008*/ MAPHR(STG_E_INSUFFICIENTMEMORY, VBSERR_OutOfMemory),
    /*0x80030012*/ MAPHR(STG_E_NOMOREFILES, VBSERR_TooManyFiles),
    /*0x80030013*/ MAPHR(STG_E_DISKISWRITEPROTECTED, VBSERR_PermissionDenied),
    /*0x8003001D*/ MAPHR(STG_E_WRITEFAULT, VBSERR_IOError),
    /*0x8003001E*/ MAPHR(STG_E_READFAULT, VBSERR_IOError),
    /*0x80030020*/ MAPHR(STG_E_SHAREVIOLATION, VBSERR_PathFileAccess),
    /*0x80030021*/ MAPHR(STG_E_LOCKVIOLATION, VBSERR_PermissionDenied),
    /*0x80030050*/ MAPHR(STG_E_FILEALREADYEXISTS, VBSERR_FileAlreadyExists),
    /*0x80030070*/ MAPHR(STG_E_MEDIUMFULL, VBSERR_DiskFull),
    /*0x800300FC*/ MAPHR(STG_E_INVALIDNAME, VBSERR_FileNotFound),
    /*0x80030100*/ MAPHR(STG_E_INUSE, VBSERR_PermissionDenied),
    /*0x80030101*/ MAPHR(STG_E_NOTCURRENT, VBSERR_PermissionDenied),
    /*0x80030103*/ MAPHR(STG_E_CANTSAVE, VBSERR_IOError),


    // FACILITY_ITF errors.
    /*0x80040154*/ MAPHR(REGDB_E_CLASSNOTREG, VBSERR_CantCreateObject),
    /*0x800401E3*/ MAPHR(MK_E_UNAVAILABLE, VBSERR_CantCreateObject),
    /*0x800401E6*/ MAPHR(MK_E_INVALIDEXTENSION, VBSERR_OLEFileNotFound),
    /*0x800401EA*/ MAPHR(MK_E_CANTOPENFILE, VBSERR_OLEFileNotFound),
    /*0x800401F3*/ MAPHR(CO_E_CLASSSTRING, VBSERR_CantCreateObject),
    /*0x800401F5*/ MAPHR(CO_E_APPNOTFOUND, VBSERR_CantCreateObject),
    /*0x800401FE*/ MAPHR(CO_E_APPDIDNTREG, VBSERR_CantCreateObject),

#if _WIN32 || _WIN64
    // FACILITY_WIN32 errors
    /*0x80070005*/ MAPHR(E_ACCESSDENIED, VBSERR_PermissionDenied),
    /*0x8007000E*/ MAPHR(E_OUTOFMEMORY, VBSERR_OutOfMemory),
    /*0x80070057*/ MAPHR(E_INVALIDARG, VBSERR_IllegalFuncCall),
    /*0x800706BA*/ MAPHR(_HRESULT_TYPEDEF_(0x800706BA), VBSERR_ServerNotFound),

    // FACILITY_WINDOWS
    /*0x80080005*/ MAPHR(CO_E_SERVER_EXEC_FAILURE, VBSERR_CantCreateObject),
#endif // _WIN32 || _WIN64
};
const long kcmhr = sizeof(g_rgmhr) / sizeof(g_rgmhr[0]);


HRESULT MapHr(HRESULT hr, ErrorTypeEnum * errorTypeOut)
{
    int imhrMin, imhrLim, imhr;

#if DEBUG
    // In debug, check that all the entries in the error map table are
    // sorted based on the HRESULT in ascending order. We will then binary
    // search the sorted array. We need do this only once per invocation.
    static BOOL fCheckSort = TRUE;

    if (fCheckSort)
    {
        fCheckSort = FALSE;
        for (imhr = 1; imhr < kcmhr; imhr++)
            Assert((ulong)g_rgmhr[imhr - 1].hrIn < (ulong)g_rgmhr[imhr].hrIn);
    }
#endif // DEBUG

    if (errorTypeOut != nullptr)
    {
        *errorTypeOut = kjstError;
    }

    if (SUCCEEDED(hr))
        return NOERROR;

    if (FACILITY_CONTROL == HRESULT_FACILITY(hr))
        return hr;

    for (imhrMin = 0, imhrLim = kcmhr; imhrMin < imhrLim; )
    {
        imhr = (imhrMin + imhrLim) / 2;
        if ((ulong)g_rgmhr[imhr].hrIn < (ulong)hr)
            imhrMin = imhr + 1;
        else
            imhrLim = imhr;
    }
    if (imhrMin < kcmhr && hr == g_rgmhr[imhrMin].hrIn)
    {
        if (errorTypeOut != nullptr)
        {
            *errorTypeOut = g_rgmhr[imhrMin].errorType;
        }

        return g_rgmhr[imhrMin].hrOut;
    }

    return hr;
}


// === ScriptException ===
ScriptException::~ScriptException(void)
{
    FreeExcepInfo(&ei);
}


void ScriptException::CopyInto(ScriptException *pse)
{
    pse->ichMin = ichMin;
    pse->ichLim = ichLim;
    CopyException(&(pse->ei), &ei);
}

void ScriptException::Free(void)
{
    ichMin = ichLim = 0;
    FreeExcepInfo(&ei);
}

void ScriptException::GetError(HRESULT *phr, EXCEPINFO *pei)
{
    AssertMem(phr);
    AssertMemN(pei);

    if (HR(SCRIPT_E_RECORDED) == *phr)
    {
        Assert(FAILED(HR(ei.scode)));
        if (nullptr == pei)
            *phr = HR(ei.scode);
        else
        {
            *phr = HR(DISP_E_EXCEPTION);
            js_memcpy_s(pei, sizeof(*pei), &ei, sizeof(*pei));
            memset(&ei, 0, sizeof(ei));
            if (nullptr != pei->pfnDeferredFillIn)
            {
                pei->pfnDeferredFillIn(pei);
                pei->pfnDeferredFillIn = nullptr;
            }
        }
    }
}


// === CompileScriptException ===
CompileScriptException::~CompileScriptException()
{
    SysFreeString(bstrLine);
}

void CompileScriptException::Clear()
{
    memset(this, 0, sizeof(*this));
}

void CompileScriptException::Free()
{
    ScriptException::Free();
    line = ichMinLine = 0;
    if (nullptr != bstrLine)
    {
        SysFreeString(bstrLine);
        bstrLine = nullptr;
    }
}

HRESULT  CompileScriptException::ProcessError(IScanner * pScan, HRESULT hr, ParseNode * pnodeBase)
{
    if (nullptr == this)
        return hr;

    // fill in the ScriptException structure
    Clear();
    ei.scode = GetScode(MapHr(hr));

    // get the error string
    if (FACILITY_CONTROL != HRESULT_FACILITY(ei.scode) ||
        nullptr == (ei.bstrDescription =
        BstrGetResourceString(HRESULT_CODE(ei.scode))))
    {
        OLECHAR szT[50];
        _snwprintf_s(szT, ARRAYSIZE(szT), ARRAYSIZE(szT)-1, OLESTR("error %d"), ei.scode);
        if (nullptr == (ei.bstrDescription = SysAllocString(szT)))
            ei.scode = E_OUTOFMEMORY;
    }

    ei.bstrSource = BstrGetResourceString(IDS_COMPILATION_ERROR_SOURCE);
    if (nullptr == pnodeBase && nullptr != pScan)
    {
        // parsing phase - get the line number from the scanner
        AssertMem(pScan);
        this->hasLineNumberInfo = true;
        pScan->GetErrorLineInfo(this->ichMin, this->ichLim, this->line, this->ichMinLine);

        HRESULT hrSysAlloc = pScan->SysAllocErrorLine(this->ichMinLine, &this->bstrLine);
        if( FAILED(hrSysAlloc) )
        {
            return hrSysAlloc;
        }

        if (ichMin < ichMinLine)
            ichMin = ichMinLine;
    }
    else
    {
        // TODO: Variable length registers.
        // Remove E_FAIL once we have this feature.
        // error during code gen - no line number info available
        // E_ABORT may result if compilation does stack probe while thread is in disabled state.
        Assert(hr == JSERR_AsmJsCompileError || hr == ERRnoMemory || hr == VBSERR_OutOfStack || hr == E_OUTOFMEMORY || hr == E_FAIL || hr == E_ABORT);
    }
    return SCRIPT_E_RECORDED;
}
