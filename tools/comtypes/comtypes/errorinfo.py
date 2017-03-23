import sys
from ctypes import *
from comtypes import IUnknown, HRESULT, COMMETHOD, GUID, BSTR
from comtypes.hresult import *

LPCOLESTR = c_wchar_p
DWORD = c_ulong

class ICreateErrorInfo(IUnknown):
    _iid_ = GUID("{22F03340-547D-101B-8E65-08002B2BD119}")
    _methods_ = [
        COMMETHOD([], HRESULT, 'SetGUID',
                  (['in'], POINTER(GUID), "rguid")),
        COMMETHOD([], HRESULT, 'SetSource',
                  (['in'], LPCOLESTR, "szSource")),
        COMMETHOD([], HRESULT, 'SetDescription',
                  (['in'], LPCOLESTR, "szDescription")),
        COMMETHOD([], HRESULT, 'SetHelpFile',
                  (['in'], LPCOLESTR, "szHelpFile")),
        COMMETHOD([], HRESULT, 'SetHelpContext',
                  (['in'], DWORD, "dwHelpContext"))
        ]

class IErrorInfo(IUnknown):
    _iid_ = GUID("{1CF2B120-547D-101B-8E65-08002B2BD119}")
    _methods_ = [
        COMMETHOD([], HRESULT, 'GetGUID',
                  (['out'], POINTER(GUID), "pGUID")),
        COMMETHOD([], HRESULT, 'GetSource',
                  (['out'], POINTER(BSTR), "pBstrSource")),
        COMMETHOD([], HRESULT, 'GetDescription',
                  (['out'], POINTER(BSTR), "pBstrDescription")),
        COMMETHOD([], HRESULT, 'GetHelpFile',
                  (['out'], POINTER(BSTR), "pBstrHelpFile")),
        COMMETHOD([], HRESULT, 'GetHelpContext',
                  (['out'], POINTER(DWORD), "pdwHelpContext")),
        ]

class ISupportErrorInfo(IUnknown):
    _iid_ = GUID("{DF0B3D60-548F-101B-8E65-08002B2BD119}")
    _methods_ = [
        COMMETHOD([], HRESULT, 'InterfaceSupportsErrorInfo',
                  (['in'], POINTER(GUID), 'riid'))
        ]

################################################################
_oleaut32 = oledll.oleaut32

def CreateErrorInfo():
    cei = POINTER(ICreateErrorInfo)()
    _oleaut32.CreateErrorInfo(byref(cei))
    return cei

def GetErrorInfo():
    """Get the error information for the current thread."""
    errinfo = POINTER(IErrorInfo)()
    if S_OK == _oleaut32.GetErrorInfo(0, byref(errinfo)):
        return errinfo
    return None

def SetErrorInfo(errinfo):
    """Set error information for the current thread."""
    return _oleaut32.SetErrorInfo(0, errinfo)

def ReportError(text, iid,
                clsid=None, helpfile=None, helpcontext=0, hresult=DISP_E_EXCEPTION):
    """Report a COM error.  Returns the passed in hresult value."""
    ei = CreateErrorInfo()
    ei.SetDescription(text)
    ei.SetGUID(iid)
    if helpfile is not None:
        ei.SetHelpFile(helpfile)
    if helpcontext is not None:
        ei.SetHelpContext(helpcontext)
    if clsid is not None:
        if isinstance(clsid, basestring):
            clsid = GUID(clsid)
        try:
            progid = clsid.as_progid()
        except WindowsError:
            pass
        else:
            ei.SetSource(progid) # progid for the class or application that created the error
    _oleaut32.SetErrorInfo(0, ei)
    return hresult

def ReportException(hresult, iid, clsid=None, helpfile=None, helpcontext=None,
                    stacklevel=None):
    """Report a COM exception.  Returns the passed in hresult value."""
    typ, value, tb = sys.exc_info()
    if stacklevel is not None:
        for _ in range(stacklevel):
            tb = tb.tb_next
        line = tb.tb_frame.f_lineno
        name = tb.tb_frame.f_globals["__name__"]
        text = "%s: %s (%s, line %d)" % (typ, value, name, line)
    else:
        text = "%s: %s" % (typ, value)
    return ReportError(text, iid,
                       clsid=clsid, helpfile=helpfile, helpcontext=helpcontext,
                       hresult=hresult)

__all__ = ["ICreateErrorInfo", "IErrorInfo", "ISupportErrorInfo",
           "ReportError", "ReportException",
           "SetErrorInfo", "GetErrorInfo", "CreateErrorInfo"]
