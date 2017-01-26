"""SAFEARRAY api functions, data types, and constants."""

from ctypes import *
from ctypes.wintypes import *
from comtypes import HRESULT, GUID

################################################################
##if __debug__:
##    from ctypeslib.dynamic_module import include
##    include("""\
##    #define UNICODE
##    #define NO_STRICT
##    #include <windows.h>
##    """,
##            persist=True)

################################################################

VARTYPE = c_ushort
PVOID = c_void_p
USHORT = c_ushort

_oleaut32 = WinDLL("oleaut32")

class tagSAFEARRAYBOUND(Structure):
    _fields_ = [
        ('cElements', DWORD),
        ('lLbound', LONG),
]
SAFEARRAYBOUND = tagSAFEARRAYBOUND

class tagSAFEARRAY(Structure):
    _fields_ = [
        ('cDims', USHORT),
        ('fFeatures', USHORT),
        ('cbElements', DWORD),
        ('cLocks', DWORD),
        ('pvData', PVOID),
        ('rgsabound', SAFEARRAYBOUND * 1),
        ]
SAFEARRAY = tagSAFEARRAY

SafeArrayAccessData = _oleaut32.SafeArrayAccessData
SafeArrayAccessData.restype = HRESULT
# Last parameter manually changed from POINTER(c_void_p) to c_void_p:
SafeArrayAccessData.argtypes = [POINTER(SAFEARRAY), c_void_p]

SafeArrayCreateVectorEx = _oleaut32.SafeArrayCreateVectorEx
SafeArrayCreateVectorEx.restype = POINTER(SAFEARRAY)
SafeArrayCreateVectorEx.argtypes = [VARTYPE, LONG, DWORD, PVOID]

SafeArrayCreateEx = _oleaut32.SafeArrayCreateEx
SafeArrayCreateEx.restype = POINTER(SAFEARRAY)
SafeArrayCreateEx.argtypes = [VARTYPE, c_uint, POINTER(SAFEARRAYBOUND), PVOID]

SafeArrayCreate = _oleaut32.SafeArrayCreate
SafeArrayCreate.restype = POINTER(SAFEARRAY)
SafeArrayCreate.argtypes = [VARTYPE, c_uint, POINTER(SAFEARRAYBOUND)]

SafeArrayUnaccessData = _oleaut32.SafeArrayUnaccessData
SafeArrayUnaccessData.restype = HRESULT
SafeArrayUnaccessData.argtypes = [POINTER(SAFEARRAY)]

_SafeArrayGetVartype = _oleaut32.SafeArrayGetVartype
_SafeArrayGetVartype.restype = HRESULT
_SafeArrayGetVartype.argtypes = [POINTER(SAFEARRAY), POINTER(VARTYPE)]
def SafeArrayGetVartype(pa):
    result = VARTYPE()
    _SafeArrayGetVartype(pa, result)
    return result.value

SafeArrayGetElement = _oleaut32.SafeArrayGetElement
SafeArrayGetElement.restype = HRESULT
SafeArrayGetElement.argtypes = [POINTER(SAFEARRAY), POINTER(LONG), c_void_p]

SafeArrayDestroy = _oleaut32.SafeArrayDestroy
SafeArrayDestroy.restype = HRESULT
SafeArrayDestroy.argtypes = [POINTER(SAFEARRAY)]

SafeArrayCreateVector = _oleaut32.SafeArrayCreateVector
SafeArrayCreateVector.restype = POINTER(SAFEARRAY)
SafeArrayCreateVector.argtypes = [VARTYPE, LONG, DWORD]

SafeArrayDestroyData = _oleaut32.SafeArrayDestroyData
SafeArrayDestroyData.restype = HRESULT
SafeArrayDestroyData.argtypes = [POINTER(SAFEARRAY)]

SafeArrayGetDim = _oleaut32.SafeArrayGetDim
SafeArrayGetDim.restype = UINT
SafeArrayGetDim.argtypes = [POINTER(SAFEARRAY)]

_SafeArrayGetLBound = _oleaut32.SafeArrayGetLBound
_SafeArrayGetLBound.restype = HRESULT
_SafeArrayGetLBound.argtypes = [POINTER(SAFEARRAY), UINT, POINTER(LONG)]
def SafeArrayGetLBound(pa, dim):
    result = LONG()
    _SafeArrayGetLBound(pa, dim, result)
    return result.value

_SafeArrayGetUBound = _oleaut32.SafeArrayGetUBound
_SafeArrayGetUBound.restype = HRESULT
_SafeArrayGetUBound.argtypes = [POINTER(SAFEARRAY), UINT, POINTER(LONG)]
def SafeArrayGetUBound(pa, dim):
    result = LONG()
    _SafeArrayGetUBound(pa, dim, result)
    return result.value


SafeArrayLock = _oleaut32.SafeArrayLock
SafeArrayLock.restype = HRESULT
SafeArrayLock.argtypes = [POINTER(SAFEARRAY)]
SafeArrayPtrOfIndex = _oleaut32.SafeArrayPtrOfIndex
SafeArrayPtrOfIndex.restype = HRESULT
# Last parameter manually changed from POINTER(c_void_p) to c_void_p:
SafeArrayPtrOfIndex.argtypes = [POINTER(SAFEARRAY), POINTER(LONG), c_void_p]
SafeArrayUnlock = _oleaut32.SafeArrayUnlock
SafeArrayUnlock.restype = HRESULT
SafeArrayUnlock.argtypes = [POINTER(SAFEARRAY)]
_SafeArrayGetIID = _oleaut32.SafeArrayGetIID
_SafeArrayGetIID.restype = HRESULT
_SafeArrayGetIID.argtypes = [POINTER(SAFEARRAY), POINTER(GUID)]
def SafeArrayGetIID(pa):
    result = GUID()
    _SafeArrayGetIID(pa, result)
    return result
SafeArrayDestroyDescriptor = _oleaut32.SafeArrayDestroyDescriptor
SafeArrayDestroyDescriptor.restype = HRESULT
SafeArrayDestroyDescriptor.argtypes = [POINTER(SAFEARRAY)]
