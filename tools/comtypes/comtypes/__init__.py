import types
import sys
import os

# comtypes version numbers follow semver (http://semver.org/) and PEP 440
__version__ = "1.1.3"

import logging
class NullHandler(logging.Handler):
    """A Handler that does nothing."""
    def emit(self, record):
        pass

logger = logging.getLogger(__name__)

# Add a NULL handler to the comtypes logger.  This prevents getting a
# message like this:
#    No handlers could be found for logger "comtypes"
# when logging is not configured and logger.error() is called.
logger.addHandler(NullHandler())

from ctypes import *
from _ctypes import COMError
from comtypes import patcher

def _check_version(actual):
    from comtypes.tools.codegenerator import version as required
    if actual != required:
        raise ImportError("Wrong version")
    if not hasattr(sys, "frozen"):
        g = sys._getframe(1).f_globals
        mod_path = g.get("__file__")
        tlb_path = g.get("typelib_path")
        try:
            mod_mtime = os.stat(mod_path).st_mtime
            tlib_mtime = os.stat(tlb_path).st_mtime
        except (OSError, TypeError):
            return
        if mod_mtime < tlib_mtime:
            raise ImportError("Typelib newer than module")

try:
    COMError()
except TypeError:
    pass
else:
    # Python 2.5 and 2.5.1 have a bug in the COMError implementation:
    # The type has no __init__ method, and no hresult, text, and
    # details instance vars.  Work around this bug by monkeypatching
    # COMError.
    def monkeypatch_COMError():
        def __init__(self, hresult, text, details):
            self.hresult = hresult
            self.text = text
            self.details = details
            super(COMError, self).__init__(hresult, text, details)
        COMError.__init__ = __init__
    monkeypatch_COMError()
    del monkeypatch_COMError

if sys.version_info >= (3, 0):
    pythonapi.PyInstanceMethod_New.argtypes = [py_object]
    pythonapi.PyInstanceMethod_New.restype = py_object
    PyInstanceMethod_Type = type(pythonapi.PyInstanceMethod_New(id))

    def instancemethod(func, inst, cls):
        mth = PyInstanceMethod_Type(func)
        if inst is None:
            return mth
        return mth.__get__(inst)
else:
    def instancemethod(func, inst, cls):
        return types.MethodType(func, inst, cls)

class ReturnHRESULT(Exception):
    """ReturnHRESULT(hresult, text)

    Return a hresult code from a COM method implementation
    without logging an error.
    """

##class IDLWarning(UserWarning):
##    "Warn about questionable type information"

from comtypes.GUID import GUID
_GUID = GUID
IID = GUID
DWORD = c_ulong

wireHWND = c_ulong

################################################################
# About COM apartments:
# http://blogs.msdn.com/larryosterman/archive/2004/04/28/122240.aspx
################################################################

################################################################
# constants for object creation
CLSCTX_INPROC_SERVER = 1
CLSCTX_INPROC_HANDLER = 2
CLSCTX_LOCAL_SERVER = 4

CLSCTX_INPROC = 3
CLSCTX_SERVER = 5
CLSCTX_ALL = 7

CLSCTX_INPROC_SERVER16 = 8
CLSCTX_REMOTE_SERVER = 16
CLSCTX_INPROC_HANDLER16 = 32
CLSCTX_RESERVED1 = 64
CLSCTX_RESERVED2 = 128
CLSCTX_RESERVED3 = 256
CLSCTX_RESERVED4 = 512
CLSCTX_NO_CODE_DOWNLOAD = 1024
CLSCTX_RESERVED5 = 2048
CLSCTX_NO_CUSTOM_MARSHAL = 4096
CLSCTX_ENABLE_CODE_DOWNLOAD = 8192
CLSCTX_NO_FAILURE_LOG = 16384
CLSCTX_DISABLE_AAA = 32768
CLSCTX_ENABLE_AAA = 65536
CLSCTX_FROM_DEFAULT_CONTEXT = 131072

tagCLSCTX = c_int # enum
CLSCTX = tagCLSCTX

# Constants for security setups
SEC_WINNT_AUTH_IDENTITY_UNICODE = 0x2
RPC_C_AUTHN_WINNT = 10
RPC_C_AUTHZ_NONE = 0
RPC_C_AUTHN_LEVEL_CONNECT = 2
RPC_C_IMP_LEVEL_IMPERSONATE = 3
EOAC_NONE = 0



################################################################
# Initialization and shutdown
_ole32 = oledll.ole32
_ole32_nohresult = windll.ole32 # use this for functions that don't return a HRESULT

COINIT_MULTITHREADED     = 0x0
COINIT_APARTMENTTHREADED = 0x2
COINIT_DISABLE_OLE1DDE   = 0x4
COINIT_SPEED_OVER_MEMORY = 0x8

def CoInitialize():
    return CoInitializeEx(COINIT_APARTMENTTHREADED)

def CoInitializeEx(flags=None):
    if flags is None:
        if os.name == "ce":
            flags = getattr(sys, "coinit_flags", COINIT_MULTITHREADED)
        else:
            flags = getattr(sys, "coinit_flags", COINIT_APARTMENTTHREADED)
    logger.debug("CoInitializeEx(None, %s)", flags)
    _ole32.CoInitializeEx(None, flags)

# COM is initialized automatically for the thread that imports this
# module for the first time.  sys.coinit_flags is passed as parameter
# to CoInitializeEx, if defined, otherwise COINIT_APARTMENTTHREADED
# (COINIT_MULTITHREADED on Windows CE) is used.
#
# A shutdown function is registered with atexit, so that
# CoUninitialize is called when Python is shut down.
CoInitializeEx()

# We need to have CoUninitialize for multithreaded model where we have
# to initialize and uninitialize COM for every new thread (except main)
# in which we are using COM
def CoUninitialize():
    logger.debug("CoUninitialize()")
    _ole32_nohresult.CoUninitialize()


def shutdown(func=_ole32_nohresult.CoUninitialize,
             _debug=logger.debug,
             _exc_clear=getattr(sys, "exc_clear", lambda: None)):
    # Make sure no COM pointers stay in exception frames.
    _exc_clear()
    # Sometimes, CoUnititialize, running at Python shutdown,
    # raises an exception.  We suppress this when __debug__ is
    # False.
    _debug("Calling CoUnititialize()")
    if __debug__:
        func()
    else:
        try: func()
        except WindowsError: pass
    # Set the flag which means that calling obj.Release() is no longer
    # needed.
    if _cominterface_meta is not None:
        _cominterface_meta._com_shutting_down = True
    _debug("CoUnititialize() done.")

import atexit
atexit.register(shutdown)
del shutdown

################################################################
# global registries.

# allows to find interface classes by guid strings (iid)
com_interface_registry = {}

# allows to find coclasses by guid strings (clsid)
com_coclass_registry = {}

def _is_object(obj):
    """This function determines if the argument is a COM object.  It
    is used in several places to determine whether propputref or
    propput setters have to be used."""
    from comtypes.automation import VARIANT
    # A COM pointer is an 'Object'
    if isinstance(obj, POINTER(IUnknown)):
        return True
    # A COM pointer in a VARIANT is an 'Object', too
    elif isinstance(obj, VARIANT) and isinstance(obj.value, POINTER(IUnknown)):
        return True
    # It may be a dynamic dispatch object.
    return hasattr(obj, "_comobj")

################################################################
# The metaclasses...

class _cominterface_meta(type):
    """Metaclass for COM interfaces.  Automatically creates high level
    methods from COMMETHOD lists.
    """

    # This flag is set to True by the atexit handler which calls
    # CoUnititialize.
    _com_shutting_down = False

    # Creates also a POINTER type for the newly created class.
    def __new__(self, name, bases, namespace):
        methods = namespace.pop("_methods_", None)
        dispmethods = namespace.pop("_disp_methods_", None)
        cls = type.__new__(self, name, bases, namespace)

        if methods is not None:
            cls._methods_ = methods
        if dispmethods is not None:
            cls._disp_methods_ = dispmethods

        # If we sublass a COM interface, for example:
        #
        # class IDispatch(IUnknown):
        #     ....
        #
        # then we need to make sure that POINTER(IDispatch) is a
        # subclass of POINTER(IUnknown) because of the way ctypes
        # typechecks work.
        if bases == (object,):
            _ptr_bases = (cls, _compointer_base)
        else:
            _ptr_bases = (cls, POINTER(bases[0]))

        # The interface 'cls' is used as a mixin.
        p = type(_compointer_base)("POINTER(%s)" % cls.__name__,
                                   _ptr_bases,
                                   {"__com_interface__": cls,
                                    "_needs_com_addref_": None})

        from ctypes import _pointer_type_cache
        _pointer_type_cache[cls] = p

        if cls._case_insensitive_:

            @patcher.Patch(p)
            class CaseInsensitive(object):
                # case insensitive attributes for COM methods and properties
                def __getattr__(self, name):
                    """Implement case insensitive access to methods and properties"""
                    try:
                        fixed_name = self.__map_case__[name.lower()]
                    except KeyError:
                        raise AttributeError(name)
                    if fixed_name != name: # prevent unbounded recursion
                        return getattr(self, fixed_name)
                    raise AttributeError(name)

                # __setattr__ is pretty heavy-weight, because it is called for
                # EVERY attribute assignment.  Settings a non-com attribute
                # through this function takes 8.6 usec, while without this
                # function it takes 0.7 sec - 12 times slower.
                #
                # How much faster would this be if implemented in C?
                def __setattr__(self, name, value):
                    """Implement case insensitive access to methods and properties"""
                    object.__setattr__(self,
                                       self.__map_case__.get(name.lower(), name),
                                       value)

        @patcher.Patch(POINTER(p))
        class ReferenceFix(object):
            def __setitem__(self, index, value):
                # We override the __setitem__ method of the
                # POINTER(POINTER(interface)) type, so that the COM
                # reference count is managed correctly.
                #
                # This is so that we can implement COM methods that have to
                # return COM pointers more easily and consistent.  Instead of
                # using CopyComPointer in the method implementation, we can
                # simply do:
                #
                # def GetTypeInfo(self, this, ..., pptinfo):
                #     if not pptinfo: return E_POINTER
                #     pptinfo[0] = a_com_interface_pointer
                #     return S_OK
                if index != 0:
                    # CopyComPointer, which is in _ctypes, does only
                    # handle an index of 0.  This code does what
                    # CopyComPointer should do if index != 0.
                    if bool(value):
                        value.AddRef()
                    super(POINTER(p), self).__setitem__(index, value)
                    return
                from _ctypes import CopyComPointer
                CopyComPointer(value, self)

        return cls

    def __setattr__(self, name, value):
        if name == "_methods_":
            # XXX I'm no longer sure why the code generator generates
            # "_methods_ = []" in the interface definition, and later
            # overrides this by "Interface._methods_ = [...]
##            assert self.__dict__.get("_methods_", None) is None
            self._make_methods(value)
            self._make_specials()
        elif name == "_disp_methods_":
            assert self.__dict__.get("_disp_methods_", None) is None
            self._make_dispmethods(value)
            self._make_specials()
        type.__setattr__(self, name, value)

    def _make_specials(self):
        # This call installs methods that forward the Python protocols
        # to COM protocols.

        def has_name(name):
            # Determine whether a property or method named 'name'
            # exists
            if self._case_insensitive_:
                return name.lower() in self.__map_case__
            return hasattr(self, name)

        # XXX These special methods should be generated by the code generator.
        if has_name("Count"):
            @patcher.Patch(self)
            class _(object):
                def __len__(self):
                    "Return the the 'self.Count' property."
                    return self.Count

        if has_name("Item"):
            @patcher.Patch(self)
            class _(object):
                # 'Item' is the 'default' value.  Make it available by
                # calling the instance (Not sure this makes sense, but
                # win32com does this also).
                def __call__(self, *args, **kw):
                    "Return 'self.Item(*args, **kw)'"
                    return self.Item(*args, **kw)

                # does this make sense? It seems that all standard typelibs I've
                # seen so far that support .Item also support ._NewEnum
                @patcher.no_replace
                def __getitem__(self, index):
                    "Return 'self.Item(index)'"
                    # Handle tuples and all-slice
                    if isinstance(index, tuple):
                        args = index
                    elif index == _all_slice:
                        args = ()
                    else:
                        args = (index,)

                    try:
                        result = self.Item(*args)
                    except COMError, err:
                        (hresult, text, details) = err.args
                        if hresult == -2147352565:  # DISP_E_BADINDEX
                            raise IndexError("invalid index")
                        else:
                            raise

                    # Note that result may be NULL COM pointer. There is no way
                    # to interpret this properly, so it is returned as-is.

                    # Hm, should we call __ctypes_from_outparam__ on the
                    # result?
                    return result

                @patcher.no_replace
                def __setitem__(self, index, value):
                    "Attempt 'self.Item[index] = value'"
                    try:
                        self.Item[index] = value
                    except COMError, err:
                        (hresult, text, details) = err.args
                        if hresult == -2147352565:  # DISP_E_BADINDEX
                            raise IndexError("invalid index")
                        else:
                            raise
                    except TypeError:
                        msg = "%r object does not support item assignment"
                        raise TypeError(msg % type(self))

        if has_name("_NewEnum"):
            @patcher.Patch(self)
            class _(object):
                def __iter__(self):
                    "Return an iterator over the _NewEnum collection."
                    # This method returns a pointer to _some_ _NewEnum interface.
                    # It relies on the fact that the code generator creates next()
                    # methods for them automatically.
                    #
                    # Better would maybe to return an object that
                    # implements the Python iterator protocol, and
                    # forwards the calls to the COM interface.
                    enum = self._NewEnum
                    if isinstance(enum, types.MethodType):
                        # _NewEnum should be a propget property, with dispid -4.
                        #
                        # Sometimes, however, it is a method.
                        enum = enum()
                    if hasattr(enum, "Next"):
                        return enum
                    # _NewEnum returns an IUnknown pointer, QueryInterface() it to
                    # IEnumVARIANT
                    from comtypes.automation import IEnumVARIANT
                    return enum.QueryInterface(IEnumVARIANT)

    def _make_case_insensitive(self):
        # The __map_case__ dictionary maps lower case names to the
        # names in the original spelling to enable case insensitive
        # method and attribute access.
        try:
            self.__dict__["__map_case__"]
        except KeyError:
            d = {}
            d.update(getattr(self, "__map_case__", {}))
            self.__map_case__ = d

    def _make_dispmethods(self, methods):
        if self._case_insensitive_:
            self._make_case_insensitive()

        # create dispinterface methods and properties on the interface 'self'
        properties = {}
        for m in methods:
            what, name, idlflags, restype, argspec = m

            # is it a property set or property get?
            is_prop = False

            # argspec is a sequence of tuples, each tuple is:
            # ([paramflags], type, name)
            try:
                memid = [x for x in idlflags if isinstance(x, int)][0]
            except IndexError:
                raise TypeError("no dispid found in idlflags")
            if what == "DISPPROPERTY": # DISPPROPERTY
                assert not argspec # XXX does not yet work for properties with parameters
                accessor = self._disp_property(memid, idlflags)
                is_prop = True
                setattr(self, name, accessor)
            elif what == "DISPMETHOD": # DISPMETHOD
                # argspec is a tuple of (idlflags, type, name[,
                # defval]) items.
                method = self._disp_method(memid, name, idlflags, restype, argspec)
## not in 2.3                method.__name__ = name
                if 'propget' in idlflags:
                    nargs = len(argspec)
                    properties.setdefault((name, nargs), [None, None, None])[0] = method
                    is_prop = True
                elif 'propput' in idlflags:
                    nargs = len(argspec)-1
                    properties.setdefault((name, nargs), [None, None, None])[1] = method
                    is_prop = True
                elif 'propputref' in idlflags:
                    nargs = len(argspec)-1
                    properties.setdefault((name, nargs), [None, None, None])[2] = method
                    is_prop = True
                else:
                    setattr(self, name, method)
            # COM is case insensitive.
            #
            # For a method, this is the real name.  For a property,
            # this is the name WITHOUT the _set_ or _get_ prefix.
            if self._case_insensitive_:
                self.__map_case__[name.lower()] = name
                if is_prop:
                    self.__map_case__[name[5:].lower()] = name[5:]

        for (name, nargs), methods in properties.items():
            # methods contains [propget or None, propput or None, propputref or None]
            if methods[1] is not None and methods[2] is not None:
                # both propput and propputref.
                #
                # Create a setter method that examines the argument type
                # and calls 'propputref' if it is an Object (in the VB
                # sense), or call 'propput' otherwise.
                propput = methods[1]
                propputref = methods[2]
                def put_or_putref(self, *args):
                    if _is_object(args[-1]):
                        return propputref(self, *args)
                    else:
                        return propput(self, *args)
                methods[1] = put_or_putref
                del methods[2]
            elif methods[2] is not None:
                # use propputref
                del methods[1]
            else:
                # use propput (if any)
                del methods[2]
            if nargs:
                setattr(self, name, named_property("%s.%s" % (self.__name__, name), *methods))
            else:
                assert len(methods) <= 2
                setattr(self, name, property(*methods))

            # COM is case insensitive
            if self._case_insensitive_:
                self.__map_case__[name.lower()] = name

    # Some ideas, (not only) related to disp_methods:
    #
    # Should the functions/methods we create have restype and/or
    # argtypes attributes?

    def _disp_method(self, memid, name, idlflags, restype, argspec):
        if 'propget' in idlflags:
            def getfunc(obj, *args, **kw):
                return self.Invoke(obj, memid, _invkind=2, *args, **kw) # DISPATCH_PROPERTYGET
            return getfunc
        elif 'propput' in idlflags:
            def putfunc(obj, *args, **kw):
                return self.Invoke(obj, memid, _invkind=4, *args, **kw) # DISPATCH_PROPERTYPUT
            return putfunc
        elif 'propputref' in idlflags:
            def putfunc(obj, *args, **kw):
                return self.Invoke(obj, memid, _invkind=8, *args, **kw) # DISPATCH_PROPERTYPUTREF
            return putfunc
        # a first attempt to make use of the restype.  Still, support
        # for named arguments and default argument values should be
        # added.
        if hasattr(restype, "__com_interface__"):
            interface = restype.__com_interface__
            def func(s, *args, **kw):
                result = self.Invoke(s, memid, _invkind=1, *args, **kw)
                if result is None:
                    return
                return result.QueryInterface(interface)
        else:
            def func(obj, *args, **kw):
                return self.Invoke(obj, memid, _invkind=1, *args, **kw) # DISPATCH_METHOD
        return func

    def _disp_property(self, memid, idlflags):
        # XXX doc string missing in property
        def _get(obj):
            return obj.Invoke(memid, _invkind=2) # DISPATCH_PROPERTYGET
        if "readonly" in idlflags:
            return property(_get)
        def _set(obj, value):
            # Detect whether to use DISPATCH_PROPERTYPUT or
            # DISPATCH_PROPERTYPUTREF
            invkind = 8 if _is_object(value) else 4
            return obj.Invoke(memid, value, _invkind=invkind)
        return property(_get, _set)

    def __get_baseinterface_methodcount(self):
        "Return the number of com methods in the base interfaces"
        try:
            result = 0
            for itf in self.mro()[1:-1]:
                result += len(itf.__dict__["_methods_"])
            return result
        except KeyError, err:
            (name,) = err.args
            if name == "_methods_":
                raise TypeError("baseinterface '%s' has no _methods_" % itf.__name__)
            raise

    def _fix_inout_args(self, func, argtypes, paramflags):
        # This function provides a workaround for a bug in ctypes.
        # [in, out] parameters must be converted with the argtype's
        # .from_param() method BEFORE they are passed to the _ctypes
        # build_callargs() function in Modules/_ctypes/_ctypes.c.
        #
        # For details see below.
        #
        # TODO: The workaround should be disabled when a ctypes
        # version is used where the bug is fixed.
        SIMPLETYPE = type(c_int)
        BYREFTYPE = type(byref(c_int()))
        def call_with_inout(self_, *args, **kw):
            args = list(args)
            # Indexed by order in the output
            outargs = {}
            outnum = 0
            for i, info in enumerate(paramflags):
                direction = info[0]
                if direction & 3 == 3:
                    # This is an [in, out] parameter.
                    #
                    # Determine name and required type of the parameter.
                    name = info[1]
                    # [in, out] parameters are passed as pointers,
                    # this is the pointed-to type:
                    atyp = argtypes[i]._type_

                    # Get the actual parameter, either as positional or
                    # keyword arg.
                    try:
                        try:
                            v = args[i]
                        except IndexError:
                            v = kw[name]
                    except KeyError:
                        # no parameter was passed, make an empty one
                        # of the required type
                        v = atyp()
                    else:
                        # parameter was passed, call .from_param() to
                        # convert it to a ctypes type.
                        if getattr(v, "_type_", None) is atyp:
                            # Array of or pointer to type 'atyp' was
                            # passed, pointer to 'atyp' expected.
                            pass
                        elif type(atyp) is SIMPLETYPE:
                            # The from_param method of simple types
                            # (c_int, c_double, ...) returns a byref()
                            # object which we cannot use since later
                            # it will be wrapped in a pointer.  Simply
                            # call the constructor with the argument
                            # in that case.
                            v = atyp(v)
                        else:
                            v = atyp.from_param(v)
                            assert not isinstance(v, BYREFTYPE)
                    outargs[outnum] = v
                    outnum += 1
                    if len(args) > i:
                        args[i] = v
                    else:
                        kw[name] = v
                elif direction & 2 == 2:
                    outnum += 1

            rescode = func(self_, *args, **kw)
            # If there is only a single output value, then do not expect it to
            # be iterable.
            if len(outargs) == 1:  # rescode is not iterable
                return rescode.__ctypes_from_outparam__()

            rescode = list(rescode)
            for outnum, o in outargs.items():
                rescode[outnum] = o.__ctypes_from_outparam__()
            return rescode
        return call_with_inout

    def _make_methods(self, methods):
        if self._case_insensitive_:
            self._make_case_insensitive()

        # we insist on an _iid_ in THIS class!
        try:
            iid = self.__dict__["_iid_"]
        except KeyError:
            raise AttributeError("this class must define an _iid_")
        else:
            iid = str(iid)
##            if iid in com_interface_registry:
##                # Warn when multiple interfaces are defined with identical iids.
##                # This would also trigger if we reload() a module that contains
##                # interface types, so suppress the warning in this case.
##                other = com_interface_registry[iid]
##                if self.__name__ != other.__name__ or self.__module__ != other.__module__:
##                    text = "Multiple interface defn: %s, %s" % (self, other)
##                    warnings.warn(text, UserWarning)
            com_interface_registry[iid] = self
            del iid
        vtbl_offset = self.__get_baseinterface_methodcount()

        properties = {}

        # create private low level, and public high level methods
        for i, item in enumerate(methods):
            restype, name, argtypes, paramflags, idlflags, doc = item
            # the function prototype
            prototype = WINFUNCTYPE(restype, *argtypes)

            # a low level unbound method calling the com method.
            # attach it with a private name (__com_AddRef, for example),
            # so that custom method implementations can call it.

            # If the method returns a HRESULT, we pass the interface iid,
            # so that we can request error info for the interface.
            if restype == HRESULT:
##                print "%s.%s" % (self.__name__, name)
                raw_func = prototype(i + vtbl_offset, name, None, self._iid_)
                func = prototype(i + vtbl_offset, name, paramflags, self._iid_)
            else:
                raw_func = prototype(i + vtbl_offset, name, None, None)
                func = prototype(i + vtbl_offset, name, paramflags, None)
            setattr(self,
                    "_%s__com_%s" % (self.__name__, name),
                    instancemethod(raw_func, None, self))

            if paramflags:
                # see comment in the _fix_inout_args method
                dirflags = [(p[0]&3) for p in paramflags]
                if 3 in dirflags:
##                    fullname = "%s::%s" % (self.__name__, name)
##                    print "FIX %s" % fullname
                    func = self._fix_inout_args(func, argtypes, paramflags)

            # 'func' is a high level function calling the COM method
            func.__doc__ = doc
            try:
                func.__name__ = name # for pyhelp
            except TypeError:
                # In Python 2.3, __name__ is a readonly attribute
                pass
            # make it an unbound method.  Remember, 'self' is a type here.
            mth = instancemethod(func, None, self)

            # is it a property set or property get?
            is_prop = False

            # XXX Hm.  What, when paramflags is None?
            # Or does have '0' values?
            # Seems we loose then, at least for properties...

            # The following code assumes that the docstrings for
            # propget and propput are identical.
            if "propget" in idlflags:
                assert name.startswith("_get_")
                nargs = len([flags for flags in paramflags
                             if flags[0] & 7 in (0, 1)])
                # XXX or should we do this?
                # nargs = len([flags for flags in paramflags
                #             if (flags[0] & 1) or (flags[0] == 0)])
                propname = name[len("_get_"):]
                properties.setdefault((propname, doc, nargs), [None, None, None])[0] = func
                is_prop = True
            elif "propput" in idlflags:
                assert name.startswith("_set_")
                nargs = len([flags for flags in paramflags
                              if flags[0] & 7 in (0, 1)]) - 1
                propname = name[len("_set_"):]
                properties.setdefault((propname, doc, nargs), [None, None, None])[1] = func
                is_prop = True
            elif "propputref" in idlflags:
                assert name.startswith("_setref_")
                nargs = len([flags for flags in paramflags
                              if flags[0] & 7 in (0, 1)]) - 1
                propname = name[len("_setref_"):]
                properties.setdefault((propname, doc, nargs), [None, None, None])[2] = func
                is_prop = True

            # We install the method in the class, except when it's a
            # property accessor.  And we make sure we don't overwrite
            # a property that's already present in the class.
            if not is_prop:
                if hasattr(self, name):
                    setattr(self, "_" + name, mth)
                else:
                    setattr(self, name, mth)

            # COM is case insensitive.
            #
            # For a method, this is the real name.  For a property,
            # this is the name WITHOUT the _set_ or _get_ prefix.
            if self._case_insensitive_:
                self.__map_case__[name.lower()] = name
                if is_prop:
                    self.__map_case__[name[5:].lower()] = name[5:]

        # create public properties / attribute accessors
        for (name, doc, nargs), methods in properties.items():
            # methods contains [propget or None, propput or None, propputref or None]
            if methods[1] is not None and methods[2] is not None:
                # both propput and propputref.
                #
                # Create a setter method that examines the argument type
                # and calls 'propputref' if it is an Object (in the VB
                # sense), or call 'propput' otherwise.
                propput = methods[1]
                propputref = methods[2]
                def put_or_putref(self, *args):
                    if _is_object(args[-1]):
                        return propputref(self, *args)
                    else:
                        return propput(self, *args)
                methods[1] = put_or_putref
                del methods[2]
            elif methods[2] is not None:
                # use propputref
                del methods[1]
            else:
                # use propput (if any)
                del methods[2]
            if nargs == 0:
                prop = property(*methods + [None, doc])
            else:
                # Hm, must be a descriptor where the __get__ method
                # returns a bound object having __getitem__ and
                # __setitem__ methods.
                prop = named_property("%s.%s" % (self.__name__, name), *methods + [doc])
            # Again, we should not overwrite class attributes that are
            # already present.
            if hasattr(self, name):
                setattr(self, "_" + name, prop)
            else:
                setattr(self, name, prop)

            # COM is case insensitive
            if self._case_insensitive_:
                self.__map_case__[name.lower()] = name


################################################################
# helper classes for COM propget / propput
# Should they be implemented in C for speed?

_all_slice = slice(None, None, None)


class bound_named_property(object):
    def __init__(self, name, getter, setter, im_inst):
        self.name = name
        self.im_inst = im_inst
        self.getter = getter
        self.setter = setter

    def __getitem__(self, index):
        if self.getter is None:
            raise TypeError("unsubscriptable object")
        if isinstance(index, tuple):
            return self.getter(self.im_inst, *index)
        elif index == _all_slice:
            return self.getter(self.im_inst)
        else:
            return self.getter(self.im_inst, index)

    def __call__(self, *args):
        if self.getter is None:
            raise TypeError("object is not callable")
        return self.getter(self.im_inst, *args)

    def __setitem__(self, index, value):
        if self.setter is None:
            raise TypeError("object does not support item assignment")
        if isinstance(index, tuple):
            self.setter(self.im_inst, *(index + (value,)))
        elif index == _all_slice:
            self.setter(self.im_inst, value)
        else:
            self.setter(self.im_inst, index, value)

    def __repr__(self):
        return "<bound_named_property %r at %x>" % (self.name, id(self))

    def __iter__(self):
        """ Explicitly disallow iteration. """
        msg = "%r is not iterable" % self.name
        raise TypeError(msg)



class named_property(object):
    def __init__(self, name, fget=None, fset=None, doc=None):
        self.name = name
        self.getter = fget
        self.setter = fset
        self.__doc__ = doc

    def __get__(self, im_inst, im_class=None):
        if im_inst is None:
            return self
        return bound_named_property(self.name, self.getter, self.setter, im_inst)

    # Make this a data descriptor
    def __set__(self, obj):
        raise AttributeError("Unsettable attribute")

    def __repr__(self):
        return "<named_property %r at %x>" % (self.name, id(self))

################################################################

class _compointer_meta(type(c_void_p), _cominterface_meta):
    "metaclass for COM interface pointer classes"
    # no functionality, but needed to avoid a metaclass conflict

class _compointer_base(c_void_p):
    "base class for COM interface pointer classes"
    __metaclass__ = _compointer_meta
    def __del__(self, _debug=logger.debug):
        "Release the COM refcount we own."
        if self:
            # comtypes calls CoUnititialize() when the atexit handlers
            # runs.  CoUninitialize() cleans up the COM objects that
            # are still alive. Python COM pointers may still be
            # present but we can no longer call Release() on them -
            # this may give a protection fault.  So we need the
            # _com_shutting_down flag.
            #
            if not type(self)._com_shutting_down:
                _debug("Release %s", self)
                self.Release()

    def __cmp__(self, other):
        """Compare pointers to COM interfaces."""
        # COM identity rule
        #
        # XXX To compare COM interface pointers, should we
        # automatically QueryInterface for IUnknown on both items, and
        # compare the pointer values?
        if not isinstance(other, _compointer_base):
            return 1

        # get the value property of the c_void_p baseclass, this is the pointer value
        return cmp(super(_compointer_base, self).value, super(_compointer_base, other).value)

    def __eq__(self, other):
        if not isinstance(other, _compointer_base):
            return False
        # get the value property of the c_void_p baseclass, this is the pointer value
        return super(_compointer_base, self).value == super(_compointer_base, other).value

    def __hash__(self):
        """Return the hash value of the pointer."""
        # hash the pointer values
        return hash(super(_compointer_base, self).value)

    # redefine the .value property; return the object itself.
    def __get_value(self):
        return self
    value = property(__get_value, doc="""Return self.""")

    def __repr__(self):
        ptr = super(_compointer_base, self).value
        return "<%s ptr=0x%x at %x>" % (self.__class__.__name__, ptr or 0, id(self))

    # This fixes the problem when there are multiple python interface types
    # wrapping the same COM interface.  This could happen because some interfaces
    # are contained in multiple typelibs.
    #
    # It also allows to pass a CoClass instance to an api
    # expecting a COM interface.
    def from_param(klass, value):
        """Convert 'value' into a COM pointer to the interface.

        This method accepts a COM pointer, or a CoClass instance
        which is QueryInterface()d."""
        if value is None:
            return None
        # CLF: 2013-01-18
        # A default value of 0, meaning null, can pass through to here.
        if value == 0:
            return None
        if isinstance(value, klass):
            return value
        # multiple python interface types for the same COM interface.
        # Do we need more checks here?
        if klass._iid_ == getattr(value, "_iid_", None):
            return value
        # Accept an CoClass instance which exposes the interface required.
        try:
            table = value._com_pointers_
        except AttributeError:
            pass
        else:
            try:
                # a kind of QueryInterface
                return table[klass._iid_]
            except KeyError:
                raise TypeError("Interface %s not supported" % klass._iid_)
        return value.QueryInterface(klass.__com_interface__)
    from_param = classmethod(from_param)

################################################################

from ctypes import _SimpleCData

class BSTR(_SimpleCData):
    "The windows BSTR data type"
    _type_ = "X"
    _needsfree = False
    def __repr__(self):
        return "%s(%r)" % (self.__class__.__name__, self.value)

    def __ctypes_from_outparam__(self):
        self._needsfree = True
        return self.value

    def __del__(self, _free=windll.oleaut32.SysFreeString):
        # Free the string if self owns the memory
        # or if instructed by __ctypes_from_outparam__.
        if self._b_base_ is None \
               or self._needsfree:
            _free(self)

    def from_param(cls, value):
        """Convert into a foreign function call parameter."""
        if isinstance(value, cls):
            return value
        # Although the builtin SimpleCData.from_param call does the
        # right thing, it doesn't ensure that SysFreeString is called
        # on destruction.
        return cls(value)
    from_param = classmethod(from_param)

################################################################
# IDL stuff

class helpstring(unicode):
    "Specifies the helpstring for a COM method or property."

class defaultvalue(object):
    "Specifies the default value for parameters marked optional."
    def __init__(self, value):
        self.value = value

class dispid(int):
    "Specifies the DISPID of a method or property."

# XXX STDMETHOD, COMMETHOD, DISPMETHOD, and DISPPROPERTY should return
# instances with methods, or at least accessors instead of tuple.

def STDMETHOD(restype, name, argtypes=()):
    "Specifies a COM method slot without idlflags"
    # restype, name, argtypes, paramflags, idlflags, docstring
    return restype, name, argtypes, None, (), None

def DISPMETHOD(idlflags, restype, name, *argspec):
    "Specifies a method of a dispinterface"
    return "DISPMETHOD", name, idlflags, restype, argspec

def DISPPROPERTY(idlflags, proptype, name):
    "Specifies a property of a dispinterface"
    return "DISPPROPERTY", name, idlflags, proptype, ()#, argspec

# COMMETHOD returns:
# restype, methodname, tuple(argtypes), tuple(paramflags), tuple(idlflags), helptext
#
# paramflags is a sequence of (flags (integer), paramname (string)
# tuple(idlflags) is for the method itself: (dispid, 'readonly')
#
# Example: (HRESULT, 'Width', (c_long,), (2, 'rhs'), (4, 'readonly'), None)

## sample generated code:
##    DISPPROPERTY([5, 'readonly'], OLE_YSIZE_HIMETRIC, 'Height'),
##    DISPMETHOD([6], None, 'Render',
##               ( [], c_int, 'hdc' ),
##               ( [], c_int, 'x' ),
##               ( [], c_int, 'y' ))

################################################################

_PARAMFLAGS = {
    "in": 1,
    "out": 2,
    "lcid": 4,
    "retval": 8,
    "optional": 16,
    }

def _encode_idl(names):
    # sum up all values found in _PARAMFLAGS, ignoring all others.
    return sum([_PARAMFLAGS.get(n, 0) for n in names])

_NOTHING = object()
def _unpack_argspec(idl, typ, name=None, defval=_NOTHING):
    return idl, typ, name, defval

def COMMETHOD(idlflags, restype, methodname, *argspec):
    """Specifies a COM method slot with idlflags.

    XXX should explain the sematics of the arguments.
    """
    paramflags = []
    argtypes = []

    # collect all helpstring instances
    # We should suppress docstrings when Python is started with -OO
    helptext = [t for t in idlflags if isinstance(t, helpstring)]
    # join them together(does this make sense?) and replace by None if empty.
    helptext = "".join(helptext) or None

    from comtypes.automation import VARIANT

    for item in argspec:
        idl, typ, argname, defval = _unpack_argspec(*item)
        pflags = _encode_idl(idl)
        if "optional" in idl:
            if defval is _NOTHING:
                if typ is VARIANT:
                    defval = VARIANT.missing
                elif typ is POINTER(VARIANT):
                    defval = pointer(VARIANT.missing)
                else:
##                    msg = ("'optional' only allowed for VARIANT and VARIANT*, not for %s"
##                           % typ.__name__)
##                    warnings.warn(msg, IDLWarning, stacklevel=2)
                    defval = typ()
        if defval is _NOTHING:
            paramflags.append((pflags, argname))
        else:
            paramflags.append((pflags, argname, defval))
        argtypes.append(typ)
    if "propget" in idlflags:
        methodname = "_get_%s" % methodname
    elif "propput" in idlflags:
        methodname = "_set_%s" % methodname
    elif "propputref" in idlflags:
        methodname = "_setref_%s" % methodname
    return restype, methodname, tuple(argtypes), tuple(paramflags), tuple(idlflags), helptext

################################################################
# IUnknown, the root of all evil...

class IUnknown(object):
    """The most basic COM interface.

    Each subclasses of IUnknown must define these class attributes:

    _iid_ - a GUID instance defining the identifier of this interface

    _methods_ - a list of methods for this interface.

    The _methods_ list must in VTable order.  Methods are specified
    with STDMETHOD or COMMETHOD calls.
    """
    _case_insensitive_ = False
    __metaclass__ = _cominterface_meta
    _iid_ = GUID("{00000000-0000-0000-C000-000000000046}")

    _methods_ = [
        STDMETHOD(HRESULT, "QueryInterface",
                  [POINTER(GUID), POINTER(c_void_p)]),
        STDMETHOD(c_ulong, "AddRef"),
        STDMETHOD(c_ulong, "Release")
    ]

    def QueryInterface(self, interface, iid=None):
        "QueryInterface(interface) -> instance"
        p = POINTER(interface)()
        if iid is None:
            iid = interface._iid_
        self.__com_QueryInterface(byref(iid), byref(p))
        clsid = self.__dict__.get('__clsid')
        if clsid is not None:
            p.__dict__['__clsid'] = clsid
        return p

    # these are only so that they get a docstring.
    # XXX There should be other ways to install a docstring.
    def AddRef(self):
        "Increase the internal refcount by one and return it."
        return self.__com_AddRef()

    def Release(self):
        "Decrease the internal refcount by one and return it."
        return self.__com_Release()

# IPersist is a trivial interface, which allows to ask an object about
# its clsid.
class IPersist(IUnknown):
    _iid_ = GUID('{0000010C-0000-0000-C000-000000000046}')
    _idlflags_ = []
    _methods_ = [
        COMMETHOD([], HRESULT, 'GetClassID',
                  ( ['out'], POINTER(GUID), 'pClassID' )),
        ]

class IServiceProvider(IUnknown):
    _iid_ = GUID('{6D5140C1-7436-11CE-8034-00AA006009FA}')

    # Overridden QueryService to make it nicer to use (passing it an
    # interface and it returns a pointer to that interface)
    def QueryService(self, serviceIID, interface):
        p = POINTER(interface)()
        self._QueryService(byref(serviceIID), byref(interface._iid_), byref(p))
        return p

    _methods_ = [
        COMMETHOD([], HRESULT, 'QueryService',
                  ( ['in'], POINTER(GUID), 'guidService' ),
                  ( ['in'], POINTER(GUID), 'riid' ),
                  ( ['in'], POINTER(c_void_p), 'ppvObject' ))
        ]

################################################################
def CoGetObject(displayname, interface):
    """Convert a displayname to a moniker, then bind and return the object
    identified by the moniker."""
    if interface is None:
        interface = IUnknown
    punk = POINTER(interface)()
    # Do we need a way to specify the BIND_OPTS parameter?
    _ole32.CoGetObject(unicode(displayname),
                       None,
                       byref(interface._iid_),
                       byref(punk))
    return punk

def CoCreateInstance(clsid, interface=None, clsctx=None, punkouter=None):
    """The basic windows api to create a COM class object and return a
    pointer to an interface.
    """
    if clsctx is None:
        clsctx = CLSCTX_SERVER
    if interface is None:
        interface = IUnknown
    p = POINTER(interface)()
    iid = interface._iid_
    _ole32.CoCreateInstance(byref(clsid), punkouter, clsctx, byref(iid), byref(p))
    return p

def CoGetClassObject(clsid, clsctx=None, pServerInfo=None, interface=None):
    if clsctx is None:
        clsctx = CLSCTX_SERVER
    if interface is None:
        import comtypes.server
        interface = comtypes.server.IClassFactory
    p = POINTER(interface)()
    _CoGetClassObject(clsid,
                      clsctx,
                      pServerInfo,
                      interface._iid_,
                      byref(p))
    return p

def GetActiveObject(clsid, interface=None):
    """Retrieves a pointer to a running object"""
    p = POINTER(IUnknown)()
    oledll.oleaut32.GetActiveObject(byref(clsid), None, byref(p))
    if interface is not None:
        p = p.QueryInterface(interface)
    return p

class MULTI_QI(Structure):
    _fields_ = [("pIID", POINTER(GUID)),
                ("pItf", POINTER(c_void_p)),
                ("hr", HRESULT)]

class _COAUTHIDENTITY(Structure):
    _fields_ = [
        ('User', POINTER(c_ushort)),
        ('UserLength', c_ulong),
        ('Domain', POINTER(c_ushort)),
        ('DomainLength', c_ulong),
        ('Password', POINTER(c_ushort)),
        ('PasswordLength', c_ulong),
        ('Flags', c_ulong),
    ]
COAUTHIDENTITY = _COAUTHIDENTITY

class _COAUTHINFO(Structure):
    _fields_ = [
        ('dwAuthnSvc', c_ulong),
        ('dwAuthzSvc', c_ulong),
        ('pwszServerPrincName', c_wchar_p),
        ('dwAuthnLevel', c_ulong),
        ('dwImpersonationLevel', c_ulong),
        ('pAuthIdentityData', POINTER(_COAUTHIDENTITY)),
        ('dwCapabilities', c_ulong),
    ]
COAUTHINFO = _COAUTHINFO

class _COSERVERINFO(Structure):
    _fields_ = [
        ('dwReserved1', c_ulong),
        ('pwszName', c_wchar_p),
        ('pAuthInfo', POINTER(_COAUTHINFO)),
        ('dwReserved2', c_ulong),
    ]
COSERVERINFO = _COSERVERINFO
_CoGetClassObject = _ole32.CoGetClassObject
_CoGetClassObject.argtypes = [POINTER(GUID), DWORD, POINTER(COSERVERINFO),
                              POINTER(GUID), POINTER(c_void_p)]

class tagBIND_OPTS(Structure):
    _fields_ = [
        ('cbStruct', c_ulong),
        ('grfFlags', c_ulong),
        ('grfMode', c_ulong),
        ('dwTickCountDeadline', c_ulong)
    ]
# XXX Add __init__ which sets cbStruct?
BIND_OPTS = tagBIND_OPTS

class tagBIND_OPTS2(Structure):
    _fields_ = [
        ('cbStruct', c_ulong),
        ('grfFlags', c_ulong),
        ('grfMode', c_ulong),
        ('dwTickCountDeadline', c_ulong),
        ('dwTrackFlags', c_ulong),
        ('dwClassContext', c_ulong),
        ('locale', c_ulong),
        ('pServerInfo', POINTER(_COSERVERINFO)),
    ]
# XXX Add __init__ which sets cbStruct?
BINDOPTS2 = tagBIND_OPTS2

#Structures for security setups
#########################################
class _SEC_WINNT_AUTH_IDENTITY(Structure):
    _fields_ = [
        ('User', POINTER(c_ushort)),
        ('UserLength', c_ulong),
        ('Domain', POINTER(c_ushort)),
        ('DomainLength', c_ulong),
        ('Password', POINTER(c_ushort)),
        ('PasswordLength', c_ulong),
        ('Flags', c_ulong),
    ]
SEC_WINNT_AUTH_IDENTITY = _SEC_WINNT_AUTH_IDENTITY

class _SOLE_AUTHENTICATION_INFO(Structure):
    _fields_ = [
        ('dwAuthnSvc', c_ulong),
        ('dwAuthzSvc', c_ulong),
        ('pAuthInfo', POINTER(_SEC_WINNT_AUTH_IDENTITY)),
    ]
SOLE_AUTHENTICATION_INFO = _SOLE_AUTHENTICATION_INFO

class _SOLE_AUTHENTICATION_LIST(Structure):
    _fields_ = [
        ('cAuthInfo', c_ulong),
        ('pAuthInfo', POINTER(_SOLE_AUTHENTICATION_INFO)),
    ]
SOLE_AUTHENTICATION_LIST = _SOLE_AUTHENTICATION_LIST

def CoCreateInstanceEx(clsid, interface=None,
                       clsctx=None,
                       machine=None,
                       pServerInfo=None):
    """The basic windows api to create a COM class object and return a
    pointer to an interface, possibly on another machine.

    Passing both "machine" and "pServerInfo" results in a ValueError.

    """
    if clsctx is None:
        clsctx=CLSCTX_LOCAL_SERVER|CLSCTX_REMOTE_SERVER

    if pServerInfo is not None:
        if machine is not None:
            msg = "Can not specify both machine name and server info"
            raise ValueError(msg)
    elif machine is not None:
        serverinfo = COSERVERINFO()
        serverinfo.pwszName = machine
        pServerInfo = byref(serverinfo)

    if interface is None:
        interface = IUnknown
    multiqi = MULTI_QI()
    multiqi.pIID = pointer(interface._iid_)
    _ole32.CoCreateInstanceEx(byref(clsid),
                             None,
                             clsctx,
                             pServerInfo,
                             1,
                             byref(multiqi))
    return cast(multiqi.pItf, POINTER(interface))


################################################################
from comtypes._comobject import COMObject

# What's a coclass?
# a POINTER to a coclass is allowed as parameter in a function declaration:
# http://msdn.microsoft.com/library/en-us/midl/midl/oleautomation.asp

from comtypes._meta import _coclass_meta

class CoClass(COMObject):
    __metaclass__ = _coclass_meta
################################################################
