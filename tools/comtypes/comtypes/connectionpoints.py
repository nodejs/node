from ctypes import *
from comtypes import IUnknown, COMMETHOD, GUID, HRESULT, dispid
_GUID = GUID

class tagCONNECTDATA(Structure):
    _fields_ = [
        ('pUnk', POINTER(IUnknown)),
        ('dwCookie', c_ulong),
    ]
CONNECTDATA = tagCONNECTDATA

################################################################

class IConnectionPointContainer(IUnknown):
    _iid_ = GUID('{B196B284-BAB4-101A-B69C-00AA00341D07}')
    _idlflags_ = []

class IConnectionPoint(IUnknown):
    _iid_ = GUID('{B196B286-BAB4-101A-B69C-00AA00341D07}')
    _idlflags_ = []

class IEnumConnections(IUnknown):
    _iid_ = GUID('{B196B287-BAB4-101A-B69C-00AA00341D07}')
    _idlflags_ = []

    def __iter__(self):
        return self

    def next(self):
        cp, fetched = self.Next(1)
        if fetched == 0:
            raise StopIteration
        return cp

class IEnumConnectionPoints(IUnknown):
    _iid_ = GUID('{B196B285-BAB4-101A-B69C-00AA00341D07}')
    _idlflags_ = []

    def __iter__(self):
        return self

    def next(self):
        cp, fetched = self.Next(1)
        if fetched == 0:
            raise StopIteration
        return cp

################################################################

IConnectionPointContainer._methods_ = [
    COMMETHOD([], HRESULT, 'EnumConnectionPoints',
              ( ['out'], POINTER(POINTER(IEnumConnectionPoints)), 'ppEnum' )),
    COMMETHOD([], HRESULT, 'FindConnectionPoint',
              ( ['in'], POINTER(_GUID), 'riid' ),
              ( ['out'], POINTER(POINTER(IConnectionPoint)), 'ppCP' )),
]

IConnectionPoint._methods_ = [
    COMMETHOD([], HRESULT, 'GetConnectionInterface',
              ( ['out'], POINTER(_GUID), 'pIID' )),
    COMMETHOD([], HRESULT, 'GetConnectionPointContainer',
              ( ['out'], POINTER(POINTER(IConnectionPointContainer)), 'ppCPC' )),
    COMMETHOD([], HRESULT, 'Advise',
              ( ['in'], POINTER(IUnknown), 'pUnkSink' ),
              ( ['out'], POINTER(c_ulong), 'pdwCookie' )),
    COMMETHOD([], HRESULT, 'Unadvise',
              ( ['in'], c_ulong, 'dwCookie' )),
    COMMETHOD([], HRESULT, 'EnumConnections',
              ( ['out'], POINTER(POINTER(IEnumConnections)), 'ppEnum' )),
]

IEnumConnections._methods_ = [
    COMMETHOD([], HRESULT, 'Next',
              ( ['in'], c_ulong, 'cConnections' ),
              ( ['out'], POINTER(tagCONNECTDATA), 'rgcd' ),
              ( ['out'], POINTER(c_ulong), 'pcFetched' )),
    COMMETHOD([], HRESULT, 'Skip',
              ( ['in'], c_ulong, 'cConnections' )),
    COMMETHOD([], HRESULT, 'Reset'),
    COMMETHOD([], HRESULT, 'Clone',
              ( ['out'], POINTER(POINTER(IEnumConnections)), 'ppEnum' )),
]

IEnumConnectionPoints._methods_ = [
    COMMETHOD([], HRESULT, 'Next',
              ( ['in'], c_ulong, 'cConnections' ),
              ( ['out'], POINTER(POINTER(IConnectionPoint)), 'ppCP' ),
              ( ['out'], POINTER(c_ulong), 'pcFetched' )),
    COMMETHOD([], HRESULT, 'Skip',
              ( ['in'], c_ulong, 'cConnections' )),
    COMMETHOD([], HRESULT, 'Reset'),
    COMMETHOD([], HRESULT, 'Clone',
              ( ['out'], POINTER(POINTER(IEnumConnectionPoints)), 'ppEnum' )),
]
