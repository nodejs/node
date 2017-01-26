import ctypes
from ctypes import WinDLL, byref, WinError
from ctypes.wintypes import MSG
_user32 = WinDLL("user32")

GetMessage = _user32.GetMessageA
GetMessage.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_uint,
    ctypes.c_uint,
]
TranslateMessage = _user32.TranslateMessage
DispatchMessage = _user32.DispatchMessageA


class _MessageLoop(object):

    def __init__(self):
        self._filters = []

    def insert_filter(self, obj, index=-1):
        self._filters.insert(index, obj)

    def remove_filter(self, obj):
        self._filters.remove(obj)

    def run(self):
        msg = MSG()
        lpmsg = byref(msg)
        while 1:
            ret = GetMessage(lpmsg, 0, 0, 0)
            if ret == -1:
                raise WinError()
            elif ret == 0:
                return # got WM_QUIT
            if not self.filter_message(lpmsg):
                TranslateMessage(lpmsg)
                DispatchMessage(lpmsg)

    def filter_message(self, lpmsg):
        return any(filter(lpmsg) for filter in self._filters)

_messageloop = _MessageLoop()

run = _messageloop.run
insert_filter = _messageloop.insert_filter
remove_filter = _messageloop.remove_filter

__all__ = ["run", "insert_filter", "remove_filter"]
