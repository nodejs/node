from ctypes import *
from ctypes.wintypes import DWORD, WIN32_FIND_DATAA, WIN32_FIND_DATAW, MAX_PATH
from comtypes import IUnknown, GUID, COMMETHOD, HRESULT, CoClass

# for GetPath
SLGP_SHORTPATH = 0x1
SLGP_UNCPRIORITY = 0x2
SLGP_RAWPATH = 0x4

# for SetShowCmd, GetShowCmd
##SW_SHOWNORMAL
##SW_SHOWMAXIMIZED
##SW_SHOWMINNOACTIVE


# for Resolve
##SLR_INVOKE_MSI
##SLR_NOLINKINFO
##SLR_NO_UI
##SLR_NOUPDATE
##SLR_NOSEARCH
##SLR_NOTRACK
##SLR_UPDATE

# fake these...
ITEMIDLIST = c_int
LPITEMIDLIST = LPCITEMIDLIST = POINTER(ITEMIDLIST)

class IShellLinkA(IUnknown):
    _iid_ = GUID('{000214EE-0000-0000-C000-000000000046}')
    _methods_ = [
        COMMETHOD([], HRESULT, 'GetPath',
                  ( ['in', 'out'], c_char_p, 'pszFile' ),
                  ( ['in'], c_int, 'cchMaxPath' ),
                  ( ['in', 'out'], POINTER(WIN32_FIND_DATAA), 'pfd' ),
                  ( ['in'], DWORD, 'fFlags' )),
        COMMETHOD([], HRESULT, 'GetIDList',
                  ( ['retval', 'out'], POINTER(LPITEMIDLIST), 'ppidl' )),
        COMMETHOD([], HRESULT, 'SetIDList',
                  ( ['in'], LPCITEMIDLIST, 'pidl' )),
        COMMETHOD([], HRESULT, 'GetDescription',
                  ( ['in', 'out'], c_char_p, 'pszName' ),
                  ( ['in'], c_int, 'cchMaxName' )),
        COMMETHOD([], HRESULT, 'SetDescription',
                  ( ['in'], c_char_p, 'pszName' )),
        COMMETHOD([], HRESULT, 'GetWorkingDirectory',
                  ( ['in', 'out'], c_char_p, 'pszDir' ),
                  ( ['in'], c_int, 'cchMaxPath' )),
        COMMETHOD([], HRESULT, 'SetWorkingDirectory',
                  ( ['in'], c_char_p, 'pszDir' )),
        COMMETHOD([], HRESULT, 'GetArguments',
                  ( ['in', 'out'], c_char_p, 'pszArgs' ),
                  ( ['in'], c_int, 'cchMaxPath' )),
        COMMETHOD([], HRESULT, 'SetArguments',
                  ( ['in'], c_char_p, 'pszArgs' )),
        COMMETHOD(['propget'], HRESULT, 'Hotkey',
                  ( ['retval', 'out'], POINTER(c_short), 'pwHotkey' )),
        COMMETHOD(['propput'], HRESULT, 'Hotkey',
                  ( ['in'], c_short, 'pwHotkey' )),
        COMMETHOD(['propget'], HRESULT, 'ShowCmd',
                  ( ['retval', 'out'], POINTER(c_int), 'piShowCmd' )),
        COMMETHOD(['propput'], HRESULT, 'ShowCmd',
                  ( ['in'], c_int, 'piShowCmd' )),
        COMMETHOD([], HRESULT, 'GetIconLocation',
                  ( ['in', 'out'], c_char_p, 'pszIconPath' ),
                  ( ['in'], c_int, 'cchIconPath' ),
                  ( ['in', 'out'], POINTER(c_int), 'piIcon' )),
        COMMETHOD([], HRESULT, 'SetIconLocation',
                  ( ['in'], c_char_p, 'pszIconPath' ),
                  ( ['in'], c_int, 'iIcon' )),
        COMMETHOD([], HRESULT, 'SetRelativePath',
                  ( ['in'], c_char_p, 'pszPathRel' ),
                  ( ['in'], DWORD, 'dwReserved' )),
        COMMETHOD([], HRESULT, 'Resolve',
                  ( ['in'], c_int, 'hwnd' ),
                  ( ['in'], DWORD, 'fFlags' )),
        COMMETHOD([], HRESULT, 'SetPath',
                  ( ['in'], c_char_p, 'pszFile' )),
        ]

    def GetPath(self, flags=SLGP_SHORTPATH):
        buf = create_string_buffer(MAX_PATH)
        # We're not interested in WIN32_FIND_DATA
        self.__com_GetPath(buf, MAX_PATH, None, flags)
        return buf.value

    def GetDescription(self):
        buf = create_string_buffer(1024)
        self.__com_GetDescription(buf, 1024)
        return buf.value

    def GetWorkingDirectory(self):
        buf = create_string_buffer(MAX_PATH)
        self.__com_GetWorkingDirectory(buf, MAX_PATH)
        return buf.value

    def GetArguments(self):
        buf = create_string_buffer(1024)
        self.__com_GetArguments(buf, 1024)
        return buf.value

    def GetIconLocation(self):
        iIcon = c_int()
        buf = create_string_buffer(MAX_PATH)
        self.__com_GetIconLocation(buf, MAX_PATH, byref(iIcon))
        return buf.value, iIcon.value

class IShellLinkW(IUnknown):
    _iid_ = GUID('{000214F9-0000-0000-C000-000000000046}')
    _methods_ = [
        COMMETHOD([], HRESULT, 'GetPath',
                  ( ['in', 'out'], c_wchar_p, 'pszFile' ),
                  ( ['in'], c_int, 'cchMaxPath' ),
                  ( ['in', 'out'], POINTER(WIN32_FIND_DATAW), 'pfd' ),
                  ( ['in'], DWORD, 'fFlags' )),
        COMMETHOD([], HRESULT, 'GetIDList',
                  ( ['retval', 'out'], POINTER(LPITEMIDLIST), 'ppidl' )),
        COMMETHOD([], HRESULT, 'SetIDList',
                  ( ['in'], LPCITEMIDLIST, 'pidl' )),
        COMMETHOD([], HRESULT, 'GetDescription',
                  ( ['in', 'out'], c_wchar_p, 'pszName' ),
                  ( ['in'], c_int, 'cchMaxName' )),
        COMMETHOD([], HRESULT, 'SetDescription',
                  ( ['in'], c_wchar_p, 'pszName' )),
        COMMETHOD([], HRESULT, 'GetWorkingDirectory',
                  ( ['in', 'out'], c_wchar_p, 'pszDir' ),
                  ( ['in'], c_int, 'cchMaxPath' )),
        COMMETHOD([], HRESULT, 'SetWorkingDirectory',
                  ( ['in'], c_wchar_p, 'pszDir' )),
        COMMETHOD([], HRESULT, 'GetArguments',
                  ( ['in', 'out'], c_wchar_p, 'pszArgs' ),
                  ( ['in'], c_int, 'cchMaxPath' )),
        COMMETHOD([], HRESULT, 'SetArguments',
                  ( ['in'], c_wchar_p, 'pszArgs' )),
        COMMETHOD(['propget'], HRESULT, 'Hotkey',
                  ( ['retval', 'out'], POINTER(c_short), 'pwHotkey' )),
        COMMETHOD(['propput'], HRESULT, 'Hotkey',
                  ( ['in'], c_short, 'pwHotkey' )),
        COMMETHOD(['propget'], HRESULT, 'ShowCmd',
                  ( ['retval', 'out'], POINTER(c_int), 'piShowCmd' )),
        COMMETHOD(['propput'], HRESULT, 'ShowCmd',
                  ( ['in'], c_int, 'piShowCmd' )),
        COMMETHOD([], HRESULT, 'GetIconLocation',
                  ( ['in', 'out'], c_wchar_p, 'pszIconPath' ),
                  ( ['in'], c_int, 'cchIconPath' ),
                  ( ['in', 'out'], POINTER(c_int), 'piIcon' )),
        COMMETHOD([], HRESULT, 'SetIconLocation',
                  ( ['in'], c_wchar_p, 'pszIconPath' ),
                  ( ['in'], c_int, 'iIcon' )),
        COMMETHOD([], HRESULT, 'SetRelativePath',
                  ( ['in'], c_wchar_p, 'pszPathRel' ),
                  ( ['in'], DWORD, 'dwReserved' )),
        COMMETHOD([], HRESULT, 'Resolve',
                  ( ['in'], c_int, 'hwnd' ),
                  ( ['in'], DWORD, 'fFlags' )),
        COMMETHOD([], HRESULT, 'SetPath',
                  ( ['in'], c_wchar_p, 'pszFile' )),
        ]

    def GetPath(self, flags=SLGP_SHORTPATH):
        buf = create_unicode_buffer(MAX_PATH)
        # We're not interested in WIN32_FIND_DATA
        self.__com_GetPath(buf, MAX_PATH, None, flags)
        return buf.value

    def GetDescription(self):
        buf = create_unicode_buffer(1024)
        self.__com_GetDescription(buf, 1024)
        return buf.value

    def GetWorkingDirectory(self):
        buf = create_unicode_buffer(MAX_PATH)
        self.__com_GetWorkingDirectory(buf, MAX_PATH)
        return buf.value

    def GetArguments(self):
        buf = create_unicode_buffer(1024)
        self.__com_GetArguments(buf, 1024)
        return buf.value

    def GetIconLocation(self):
        iIcon = c_int()
        buf = create_unicode_buffer(MAX_PATH)
        self.__com_GetIconLocation(buf, MAX_PATH, byref(iIcon))
        return buf.value, iIcon.value

class ShellLink(CoClass):
    u'ShellLink class'
    _reg_clsid_ = GUID('{00021401-0000-0000-C000-000000000046}')
    _idlflags_ = []
    _com_interfaces_ = [IShellLinkW, IShellLinkA]


if __name__ == "__main__":

    import sys
    import comtypes
    from comtypes.client import CreateObject
    from comtypes.persist import IPersistFile



    shortcut = CreateObject(ShellLink)
    print shortcut
    ##help(shortcut)

    shortcut.SetPath(sys.executable)

    shortcut.SetDescription("Python %s" % sys.version)
    shortcut.SetIconLocation(sys.executable, 1)

    print shortcut.GetPath(2)
    print shortcut.GetIconLocation()

    pf = shortcut.QueryInterface(IPersistFile)
    pf.Save("foo.lnk", True)
    print pf.GetCurFile()
