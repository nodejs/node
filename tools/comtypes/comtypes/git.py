"""comtypes.git - access the process wide global interface table

The global interface table provides a way to marshal interface pointers
between different threading appartments.
"""
from ctypes import *
from comtypes import IUnknown, STDMETHOD, COMMETHOD, \
     GUID, HRESULT, CoCreateInstance, CLSCTX_INPROC_SERVER

DWORD = c_ulong

class IGlobalInterfaceTable(IUnknown):
    _iid_ = GUID("{00000146-0000-0000-C000-000000000046}")
    _methods_ = [
        STDMETHOD(HRESULT, "RegisterInterfaceInGlobal",
                  [POINTER(IUnknown), POINTER(GUID), POINTER(DWORD)]),
        STDMETHOD(HRESULT, "RevokeInterfaceFromGlobal", [DWORD]),
        STDMETHOD(HRESULT, "GetInterfaceFromGlobal",
                  [DWORD, POINTER(GUID), POINTER(POINTER(IUnknown))]),
        ]

    def RegisterInterfaceInGlobal(self, obj, interface=IUnknown):
        cookie = DWORD()
        self.__com_RegisterInterfaceInGlobal(obj, interface._iid_, cookie)
        return cookie.value

    def GetInterfaceFromGlobal(self, cookie, interface=IUnknown):
        ptr = POINTER(interface)()
        self.__com_GetInterfaceFromGlobal(cookie, interface._iid_, ptr)
        return ptr

    def RevokeInterfaceFromGlobal(self, cookie):
        self.__com_RevokeInterfaceFromGlobal(cookie)


# It was a pain to get this CLSID: it's neither in the registry, nor
# in any header files. I had to compile a C program, and find it out
# with the debugger.  Apparently it is in uuid.lib.
CLSID_StdGlobalInterfaceTable = GUID("{00000323-0000-0000-C000-000000000046}")

git = CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                       interface=IGlobalInterfaceTable,
                       clsctx=CLSCTX_INPROC_SERVER)

RevokeInterfaceFromGlobal = git.RevokeInterfaceFromGlobal
RegisterInterfaceInGlobal = git.RegisterInterfaceInGlobal
GetInterfaceFromGlobal = git.GetInterfaceFromGlobal

__all__ = ["RegisterInterfaceInGlobal", "RevokeInterfaceFromGlobal", "GetInterfaceFromGlobal"]

if __name__ == "__main__":
    from comtypes.typeinfo import CreateTypeLib, ICreateTypeLib

    tlib = CreateTypeLib("foo.bar") # we don not save it later
    assert (tlib.AddRef(), tlib.Release()) == (2, 1)

    cookie = RegisterInterfaceInGlobal(tlib)
    assert (tlib.AddRef(), tlib.Release()) == (3, 2)

    GetInterfaceFromGlobal(cookie, ICreateTypeLib)
    GetInterfaceFromGlobal(cookie, ICreateTypeLib)
    GetInterfaceFromGlobal(cookie)
    assert (tlib.AddRef(), tlib.Release()) == (3, 2)
    RevokeInterfaceFromGlobal(cookie)
    assert (tlib.AddRef(), tlib.Release()) == (2, 1)
