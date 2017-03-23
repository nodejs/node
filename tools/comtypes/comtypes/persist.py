"""This module defines the following interfaces:

  IErrorLog
  IPropertyBag
  IPersistPropertyBag
  IPropertyBag2
  IPersistPropertyBag2

The 'DictPropertyBag' class is a class implementing the IPropertyBag
interface, useful in client code.
"""
from ctypes import *
from ctypes.wintypes import WORD, DWORD, BOOL
from comtypes import GUID, IUnknown, COMMETHOD, HRESULT, dispid
from comtypes import IPersist
from comtypes.automation import VARIANT, tagEXCEPINFO

# XXX Replace by canonical solution!!!
WSTRING = c_wchar_p

class IErrorLog(IUnknown):
    _iid_ = GUID('{3127CA40-446E-11CE-8135-00AA004BB851}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'AddError',
                  ( ['in'], WSTRING, 'pszPropName' ),
                  ( ['in'], POINTER(tagEXCEPINFO), 'pExcepInfo' )),
        ]

class IPropertyBag(IUnknown):
    _iid_ = GUID('{55272A00-42CB-11CE-8135-00AA004BB851}')
    _idlflags_ = []
    _methods_ = [
        # XXX Note: According to MSDN, pVar and pErrorLog are ['in', 'out'] parameters.
        #
        # XXX ctypes does NOT yet accept POINTER(IErrorLog) as 'out' parameter:
        # TypeError: 'out' parameter 3 must be a pointer type, not POINTER(IErrorLog)
        COMMETHOD([], HRESULT, 'Read',
                  ( ['in'], WSTRING, 'pszPropName' ),
                  ( ['in', 'out'], POINTER(VARIANT), 'pVar' ),
                  ( ['in'], POINTER(IErrorLog), 'pErrorLog' )),
##                  ( ['in', 'out'], POINTER(IErrorLog), 'pErrorLog' )),
        COMMETHOD([], HRESULT, 'Write',
                  ( ['in'], WSTRING, 'pszPropName' ),
                  ( ['in'], POINTER(VARIANT), 'pVar' )),
        ]

class IPersistPropertyBag(IPersist):
    _iid_ = GUID('{37D84F60-42CB-11CE-8135-00AA004BB851}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'InitNew'),
        COMMETHOD([], HRESULT, 'Load',
                  ( ['in'], POINTER(IPropertyBag), 'pPropBag' ),
                  ( ['in'], POINTER(IErrorLog), 'pErrorLog' )),
        COMMETHOD([], HRESULT, 'Save',
                  ( ['in'], POINTER(IPropertyBag), 'pPropBag' ),
                  ( ['in'], c_int, 'fClearDirty' ),
                  ( ['in'], c_int, 'fSaveAllProperties' )),
        ]


CLIPFORMAT = WORD

PROPBAG2_TYPE_UNDEFINED = 0
PROPBAG2_TYPE_DATA = 1
PROPBAG2_TYPE_URL = 2
PROPBAG2_TYPE_OBJECT = 3
PROPBAG2_TYPE_STREAM = 4
PROPBAG2_TYPE_STORAGE = 5
PROPBAG2_TYPE_MONIKER = 6

class tagPROPBAG2(Structure):
    _fields_ = [
        ('dwType', c_ulong),
        ('vt', c_ushort),
        ('cfType', CLIPFORMAT),
        ('dwHint', c_ulong),
        ('pstrName', WSTRING),
        ('clsid', GUID),
        ]

class IPropertyBag2(IUnknown):
    _iid_ = GUID('{22F55882-280B-11D0-A8A9-00A0C90C2004}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'Read',
                  ( ['in'], c_ulong, 'cProperties' ),
                  ( ['in'], POINTER(tagPROPBAG2), 'pPropBag' ),
                  ( ['in'], POINTER(IErrorLog), 'pErrLog' ),
                  ( ['out'], POINTER(VARIANT), 'pvarValue' ),
                  ( ['out'], POINTER(HRESULT), 'phrError' )),
        COMMETHOD([], HRESULT, 'Write',
                  ( ['in'], c_ulong, 'cProperties' ),
                  ( ['in'], POINTER(tagPROPBAG2), 'pPropBag' ),
                  ( ['in'], POINTER(VARIANT), 'pvarValue' )),
        COMMETHOD([], HRESULT, 'CountProperties',
                  ( ['out'], POINTER(c_ulong), 'pcProperties' )),
        COMMETHOD([], HRESULT, 'GetPropertyInfo',
                  ( ['in'], c_ulong, 'iProperty' ),
                  ( ['in'], c_ulong, 'cProperties' ),
                  ( ['out'], POINTER(tagPROPBAG2), 'pPropBag' ),
                  ( ['out'], POINTER(c_ulong), 'pcProperties' )),
        COMMETHOD([], HRESULT, 'LoadObject',
                  ( ['in'], WSTRING, 'pstrName' ),
                  ( ['in'], c_ulong, 'dwHint' ),
                  ( ['in'], POINTER(IUnknown), 'punkObject' ),
                  ( ['in'], POINTER(IErrorLog), 'pErrLog' )),
        ]

class IPersistPropertyBag2(IPersist):
    _iid_ = GUID('{22F55881-280B-11D0-A8A9-00A0C90C2004}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'InitNew'),
        COMMETHOD([], HRESULT, 'Load',
                  ( ['in'], POINTER(IPropertyBag2), 'pPropBag' ),
                  ( ['in'], POINTER(IErrorLog), 'pErrLog' )),
        COMMETHOD([], HRESULT, 'Save',
                  ( ['in'], POINTER(IPropertyBag2), 'pPropBag' ),
                  ( ['in'], c_int, 'fClearDirty' ),
                  ( ['in'], c_int, 'fSaveAllProperties' )),
        COMMETHOD([], HRESULT, 'IsDirty'),
        ]


# STGM constants
# Access
STGM_READ = 0x00000000
STGM_WRITE = 0x00000001
STGM_READWRITE = 0x00000002

# Sharing
STGM_SHARE_EXCLUSIVE = 0x00000010
STGM_SHARE_DENY_WRITE = 0x00000020
STGM_SHARE_DENY_READ = 0x00000030
STGM_SHARE_DENY_NONE = 0x00000040
STGM_PRIORITY = 0x00040000

# Creation
STGM_FAILIFTHERE = 0x00000000
STGM_CREATE = 0x00001000
STGM_CONVERT = 0x00020000

# Transactioning
STGM_DIRECT = 0x00000000
STGM_TRANSACTED = 0x00010000

# Transactioning Performance
STGM_NOSCRATCH = 0x00100000
STGM_NOSNAPSHOT = 0x00200000

# Direct SWMR and Simple
STGM_SIMPLE = 0x08000000
STGM_DIRECT_SWMR = 0x00400000

# Delete on release
STGM_DELETEONRELEASE = 0x04000000

LPOLESTR = LPCOLESTR = c_wchar_p

class IPersistFile(IPersist):
    _iid_ = GUID('{0000010B-0000-0000-C000-000000000046}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'IsDirty'),
        COMMETHOD([], HRESULT, 'Load',
                  ( ['in'], LPCOLESTR, 'pszFileName' ),
                  ( ['in'], DWORD, 'dwMode' )),
        COMMETHOD([], HRESULT, 'Save',
                  ( ['in'], LPCOLESTR, 'pszFileName' ),
                  ( ['in'], BOOL, 'fRemember' )),
        COMMETHOD([], HRESULT, 'SaveCompleted',
                  ( ['in'], LPCOLESTR, 'pszFileName' )),
        COMMETHOD([], HRESULT, 'GetCurFile',
                  ( ['out'], POINTER(LPOLESTR), 'ppszFileName' ))
        ]


from comtypes import COMObject
from comtypes.hresult import *
class DictPropertyBag(COMObject):
    """An object implementing the IProperty interface on a dictionary.

    Pass named values in the constructor for the client to Read(), or
    retrieve from the .values instance variable after the client has
    called Load().
    """
    _com_interfaces_ = [IPropertyBag]

    def __init__(self, **kw):
        super(DictPropertyBag, self).__init__()
        self.values = kw

    def Read(self, this, name, pVar, errorlog):
        try:
            val = self.values[name]
        except KeyError:
            return E_INVALIDARG
        # The caller did provide info about the type that is expected
        # with the pVar[0].vt typecode, except when this is VT_EMPTY.
        var = pVar[0]
        typecode = var.vt
        var.value = val
        if typecode:
            var.ChangeType(typecode)
        return S_OK

    def Write(self, this, name, var):
        val = var[0].value
        self.values[name] = val
        return S_OK
