# XXX need to find out what the share from comtypes.dataobject.
from ctypes import *
from ctypes.wintypes import _RECTL, SIZEL, HDC, tagRECT, tagPOINT

from comtypes import COMMETHOD
from comtypes import GUID
from comtypes import IUnknown

class tagPALETTEENTRY(Structure):
    _fields_ = [
        ('peRed', c_ubyte),
        ('peGreen', c_ubyte),
        ('peBlue', c_ubyte),
        ('peFlags', c_ubyte),
        ]
assert sizeof(tagPALETTEENTRY) == 4, sizeof(tagPALETTEENTRY)
assert alignment(tagPALETTEENTRY) == 1, alignment(tagPALETTEENTRY)

class tagLOGPALETTE(Structure):
    _pack_ = 2
    _fields_ = [
        ('palVersion', c_ushort),
        ('palNumEntries', c_ushort),
        ('palPalEntry', POINTER(tagPALETTEENTRY)),
        ]
assert sizeof(tagLOGPALETTE) == 8, sizeof(tagLOGPALETTE)
assert alignment(tagLOGPALETTE) == 2, alignment(tagLOGPALETTE)

class tagDVTARGETDEVICE(Structure):
    _fields_ = [
        ('tdSize', c_ulong),
        ('tdDriverNameOffset', c_ushort),
        ('tdDeviceNameOffset', c_ushort),
        ('tdPortNameOffset', c_ushort),
        ('tdExtDevmodeOffset', c_ushort),
        ('tdData', POINTER(c_ubyte)),
        ]
assert sizeof(tagDVTARGETDEVICE) == 16, sizeof(tagDVTARGETDEVICE)
assert alignment(tagDVTARGETDEVICE) == 4, alignment(tagDVTARGETDEVICE)

class tagExtentInfo(Structure):
    _fields_ = [
        ('cb', c_ulong),
        ('dwExtentMode', c_ulong),
        ('sizelProposed', SIZEL),
        ]
    def __init__(self, *args, **kw):
        self.cb = sizeof(self)
        super(tagExtentInfo, self).__init__(*args, **kw)
    def __repr__(self):
        size = (self.sizelProposed.cx, self.sizelProposed.cy)
        return "<ExtentInfo(mode=%s, size=%s) at %x>" % (self.dwExtentMode,
                                                         size,
                                                         id(self))
assert sizeof(tagExtentInfo) == 16, sizeof(tagExtentInfo)
assert alignment(tagExtentInfo) == 4, alignment(tagExtentInfo)
DVEXTENTINFO = tagExtentInfo

IAdviseSink = IUnknown # fake the interface

class IViewObject(IUnknown):
    _case_insensitive_ = False
    _iid_ = GUID('{0000010D-0000-0000-C000-000000000046}')
    _idlflags_ = []

    _methods_ = [
        COMMETHOD([], HRESULT, 'Draw',
                  ( ['in'], c_ulong, 'dwDrawAspect' ),
                  ( ['in'], c_int, 'lindex' ),
                  ( ['in'], c_void_p, 'pvAspect' ),
                  ( ['in'], POINTER(tagDVTARGETDEVICE), 'ptd' ),
                  ( ['in'], HDC, 'hdcTargetDev' ),
                  ( ['in'], HDC, 'hdcDraw' ),
                  ( ['in'], POINTER(_RECTL), 'lprcBounds' ),
                  ( ['in'], POINTER(_RECTL), 'lprcWBounds' ),
                  ( ['in'], c_void_p, 'pfnContinue' ), # a pointer to a callback function
                  ( ['in'], c_ulong, 'dwContinue')),
        COMMETHOD([], HRESULT, 'GetColorSet',
                  ( ['in'], c_ulong, 'dwDrawAspect' ),
                  ( ['in'], c_int, 'lindex' ),
                  ( ['in'], c_void_p, 'pvAspect' ),
                  ( ['in'], POINTER(tagDVTARGETDEVICE), 'ptd' ),
                  ( ['in'], HDC, 'hicTargetDev' ),
                  ( ['out'], POINTER(POINTER(tagLOGPALETTE)), 'ppColorSet' )),
        COMMETHOD([], HRESULT, 'Freeze',
                  ( ['in'], c_ulong, 'dwDrawAspect' ),
                  ( ['in'], c_int, 'lindex' ),
                  ( ['in'], c_void_p, 'pvAspect' ),
                  ( ['out'], POINTER(c_ulong), 'pdwFreeze' )),
        COMMETHOD([], HRESULT, 'Unfreeze',
                  ( ['in'], c_ulong, 'dwFreeze' )),
        COMMETHOD([], HRESULT, 'SetAdvise',
                  ( ['in'], c_ulong, 'dwAspect' ),
                  ( ['in'], c_ulong, 'advf' ),
                  ( ['in'], POINTER(IAdviseSink), 'pAdvSink' )),
        COMMETHOD([], HRESULT, 'GetAdvise',
                  ( ['out'], POINTER(c_ulong), 'pdwAspect' ),
                  ( ['out'], POINTER(c_ulong), 'pAdvf' ),
                  ( ['out'], POINTER(POINTER(IAdviseSink)), 'ppAdvSink' )),
        ]

class IViewObject2(IViewObject):
    _case_insensitive_ = False
    _iid_ = GUID('{00000127-0000-0000-C000-000000000046}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'GetExtent',
                  ( ['in'], c_ulong, 'dwDrawAspect' ),
                  ( ['in'], c_int, 'lindex' ),
                  ( ['in'], POINTER(tagDVTARGETDEVICE), 'ptd' ),
                  ( ['out'], POINTER(SIZEL), 'lpsizel' )),
        ]

class IViewObjectEx(IViewObject2):
    _case_insensitive_ = False
    _iid_ = GUID('{3AF24292-0C96-11CE-A0CF-00AA00600AB8}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'GetRect',
                  ( ['in'], c_ulong, 'dwAspect' ),
                  ( ['out'], POINTER(_RECTL), 'pRect' )),
        COMMETHOD([], HRESULT, 'GetViewStatus',
                  ( ['out'], POINTER(c_ulong), 'pdwStatus' )),
        COMMETHOD([], HRESULT, 'QueryHitPoint',
                  ( ['in'], c_ulong, 'dwAspect' ),
                  ( ['in'], POINTER(tagRECT), 'pRectBounds' ),
                  ( ['in'], tagPOINT, 'ptlLoc' ),
                  ( ['in'], c_int, 'lCloseHint' ),
                  ( ['out'], POINTER(c_ulong), 'pHitResult' )),
        COMMETHOD([], HRESULT, 'QueryHitRect',
                  ( ['in'], c_ulong, 'dwAspect' ),
                  ( ['in'], POINTER(tagRECT), 'pRectBounds' ),
                  ( ['in'], POINTER(tagRECT), 'pRectLoc' ),
                  ( ['in'], c_int, 'lCloseHint' ),
                  ( ['out'], POINTER(c_ulong), 'pHitResult' )),
        COMMETHOD([], HRESULT, 'GetNaturalExtent',
                  ( ['in'], c_ulong, 'dwAspect' ),
                  ( ['in'], c_int, 'lindex' ),
                  ( ['in'], POINTER(tagDVTARGETDEVICE), 'ptd' ),
                  ( ['in'], HDC, 'hicTargetDev' ),
                  ( ['in'], POINTER(tagExtentInfo), 'pExtentInfo' ),
                  ( ['out'], POINTER(SIZEL), 'pSizel' )),
        ]


DVASPECT = c_int # enum
DVASPECT_CONTENT    = 1
DVASPECT_THUMBNAIL  = 2
DVASPECT_ICON       = 4
DVASPECT_DOCPRINT   = 8 

DVASPECT2 = c_int # enum
DVASPECT_OPAQUE = 16
DVASPECT_TRANSPARENT = 32

DVEXTENTMODE = c_int # enum
# Container asks the object how big it wants to be to exactly fit its content:
DVEXTENT_CONTENT = 0
# The container proposes a size to the object for its use in resizing:
DVEXTENT_INTEGRAL = 1
