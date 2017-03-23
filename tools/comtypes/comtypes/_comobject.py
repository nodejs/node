from ctypes import (
    FormatError, POINTER, Structure, WINFUNCTYPE, byref, c_long, c_void_p,
    oledll, pointer, windll
)
from _ctypes import CopyComPointer
import logging
import os

from comtypes import COMError, ReturnHRESULT, instancemethod, _encode_idl
from comtypes.errorinfo import ISupportErrorInfo, ReportException, ReportError
from comtypes import IPersist
from comtypes.hresult import (
    DISP_E_BADINDEX, DISP_E_MEMBERNOTFOUND, E_FAIL, E_NOINTERFACE,
    E_INVALIDARG, E_NOTIMPL, RPC_E_CHANGED_MODE, S_FALSE, S_OK
)
from comtypes.typeinfo import IProvideClassInfo, IProvideClassInfo2


logger = logging.getLogger(__name__)
_debug = logger.debug
_warning = logger.warning
_error = logger.error

################################################################
# COM object implementation

# so we don't have to import comtypes.automation
DISPATCH_METHOD = 1
DISPATCH_PROPERTYGET = 2
DISPATCH_PROPERTYPUT = 4
DISPATCH_PROPERTYPUTREF = 8


class E_NotImplemented(Exception):
    """COM method is not implemented"""


def HRESULT_FROM_WIN32(errcode):
    "Convert a Windows error code into a HRESULT value."
    if errcode is None:
        return 0x80000000
    if errcode & 0x80000000:
        return errcode
    return (errcode & 0xFFFF) | 0x80070000


def winerror(exc):
    """Return the windows error code from a WindowsError or COMError
    instance."""
    try:
        code = exc[0]
        if isinstance(code, (int, long)):
            return code
    except IndexError:
        pass
    # Sometimes, a WindowsError instance has no error code.  An access
    # violation raised by ctypes has only text, for example.  In this
    # cases we return a generic error code.
    return E_FAIL


def _do_implement(interface_name, method_name):
    def _not_implemented(*args):
        """Return E_NOTIMPL because the method is not implemented."""
        _debug("unimplemented method %s_%s called", interface_name,
               method_name)
        return E_NOTIMPL
    return _not_implemented


def catch_errors(obj, mth, paramflags, interface, mthname):
    clsid = getattr(obj, "_reg_clsid_", None)

    def call_with_this(*args, **kw):
        try:
            result = mth(*args, **kw)
        except ReturnHRESULT, err:
            (hresult, text) = err.args
            return ReportError(text, iid=interface._iid_, clsid=clsid,
                               hresult=hresult)
        except (COMError, WindowsError), details:
            _error("Exception in %s.%s implementation:", interface.__name__,
                   mthname, exc_info=True)
            return HRESULT_FROM_WIN32(winerror(details))
        except E_NotImplemented:
            _warning("Unimplemented method %s.%s called", interface.__name__,
                     mthname)
            return E_NOTIMPL
        except:
            _error("Exception in %s.%s implementation:", interface.__name__,
                   mthname, exc_info=True)
            return ReportException(E_FAIL, interface._iid_, clsid=clsid)
        if result is None:
            return S_OK
        return result
    if paramflags is None:
        has_outargs = False
    else:
        has_outargs = bool([x[0] for x in paramflags
                            if x[0] & 2])
    call_with_this.has_outargs = has_outargs
    return call_with_this


################################################################

def hack(inst, mth, paramflags, interface, mthname):
    if paramflags is None:
        return catch_errors(inst, mth, paramflags, interface, mthname)
    code = mth.func_code
    if code.co_varnames[1:2] == ("this",):
        return catch_errors(inst, mth, paramflags, interface, mthname)
    dirflags = [f[0] for f in paramflags]
    # An argument is an input arg either if flags are NOT set in the
    # idl file, or if the flags contain 'in'. In other words, the
    # direction flag is either exactly '0' or has the '1' bit set:
    # Output arguments have flag '2'

    args_out_idx = []
    args_in_idx = []
    for i, a in enumerate(dirflags):
        if a&2:
            args_out_idx.append(i)
        if a&1 or a==0:
            args_in_idx.append(i)
    args_out = len(args_out_idx)

    ## XXX Remove this:
##    if args_in != code.co_argcount - 1:
##        return catch_errors(inst, mth, interface, mthname)

    clsid = getattr(inst, "_reg_clsid_", None)

    def call_without_this(this, *args):
        # Method implementations could check for and return E_POINTER
        # themselves.  Or an error will be raised when
        # 'outargs[i][0] = value' is executed.
##        for a in outargs:
##            if not a:
##                return E_POINTER

        #make argument list for handler by index array built above
        inargs = []
        for a in args_in_idx:
            inargs.append(args[a])
        try:
            result = mth(*inargs)
            if args_out == 1:
                args[args_out_idx[0]][0] = result
            elif args_out != 0:
                if len(result) != args_out:
                    msg = "Method should have returned a %s-tuple" % args_out
                    raise ValueError(msg)
                for i, value in enumerate(result):
                    args[args_out_idx[i]][0] = value
        except ReturnHRESULT, err:
            (hresult, text) = err.args
            return ReportError(text, iid=interface._iid_, clsid=clsid,
                               hresult=hresult)
        except COMError, err:
            (hr, text, details) = err.args
            _error("Exception in %s.%s implementation:", interface.__name__,
                   mthname, exc_info=True)
            try:
                descr, source, helpfile, helpcontext, progid = details
            except (ValueError, TypeError):
                msg = str(details)
            else:
                msg = "%s: %s" % (source, descr)
            hr = HRESULT_FROM_WIN32(hr)
            return ReportError(msg, iid=interface._iid_, clsid=clsid,
                               hresult=hr)
        except WindowsError, details:
            _error("Exception in %s.%s implementation:", interface.__name__,
                   mthname, exc_info=True)
            hr = HRESULT_FROM_WIN32(winerror(details))
            return ReportException(hr, interface._iid_, clsid=clsid)
        except E_NotImplemented:
            _warning("Unimplemented method %s.%s called", interface.__name__,
                     mthname)
            return E_NOTIMPL
        except:
            _error("Exception in %s.%s implementation:", interface.__name__,
                   mthname, exc_info=True)
            return ReportException(E_FAIL, interface._iid_, clsid=clsid)
        return S_OK
    if args_out:
        call_without_this.has_outargs = True
    return call_without_this


class _MethodFinder(object):
    def __init__(self, inst):
        self.inst = inst
        # map lower case names to names with correct spelling.
        self.names = dict([(n.lower(), n) for n in dir(inst)])

    def get_impl(self, interface, mthname, paramflags, idlflags):
        mth = self.find_impl(interface, mthname, paramflags, idlflags)
        if mth is None:
            return _do_implement(interface.__name__, mthname)
        return hack(self.inst, mth, paramflags, interface, mthname)

    def find_method(self, fq_name, mthname):
        # Try to find a method, first with the fully qualified name
        # ('IUnknown_QueryInterface'), if that fails try the simple
        # name ('QueryInterface')
        try:
            return getattr(self.inst, fq_name)
        except AttributeError:
            pass
        return getattr(self.inst, mthname)

    def find_impl(self, interface, mthname, paramflags, idlflags):
        fq_name = "%s_%s" % (interface.__name__, mthname)
        if interface._case_insensitive_:
            # simple name, like 'QueryInterface'
            mthname = self.names.get(mthname.lower(), mthname)
            # qualified name, like 'IUnknown_QueryInterface'
            fq_name = self.names.get(fq_name.lower(), fq_name)

        try:
            return self.find_method(fq_name, mthname)
        except AttributeError:
            pass
        propname = mthname[5:]  # strip the '_get_' or '_set' prefix
        if interface._case_insensitive_:
            propname = self.names.get(propname.lower(), propname)
        # propput and propget is done with 'normal' attribute access,
        # but only for COM properties that do not take additional
        # arguments:

        if "propget" in idlflags and len(paramflags) == 1:
            return self.getter(propname)
        if "propput" in idlflags and len(paramflags) == 1:
            return self.setter(propname)
        _debug("%r: %s.%s not implemented", self.inst, interface.__name__,
               mthname)
        return None

    def setter(self, propname):
        #
        def set(self, value):
            try:
                # XXX this may not be correct is the object implements
                # _get_PropName but not _set_PropName
                setattr(self, propname, value)
            except AttributeError:
                raise E_NotImplemented()
        return instancemethod(set, self.inst, type(self.inst))

    def getter(self, propname):
        def get(self):
            try:
                return getattr(self, propname)
            except AttributeError:
                raise E_NotImplemented()
        return instancemethod(get, self.inst, type(self.inst))


def _create_vtbl_type(fields, itf):
    try:
        return _vtbl_types[fields]
    except KeyError:
        class Vtbl(Structure):
            _fields_ = fields
        Vtbl.__name__ = "Vtbl_%s" % itf.__name__
        _vtbl_types[fields] = Vtbl
        return Vtbl

# Ugh. Another type cache to avoid leaking types.
_vtbl_types = {}

################################################################

try:
    if os.name == "ce":
        _InterlockedIncrement = windll.coredll.InterlockedIncrement
        _InterlockedDecrement = windll.coredll.InterlockedDecrement
    else:
        _InterlockedIncrement = windll.kernel32.InterlockedIncrement
        _InterlockedDecrement = windll.kernel32.InterlockedDecrement
except AttributeError:
    import threading
    _lock = threading.Lock()
    _acquire = _lock.acquire
    _release = _lock.release
    # win 64 doesn't have these functions

    def _InterlockedIncrement(ob):
        _acquire()
        refcnt = ob.value + 1
        ob.value = refcnt
        _release()
        return refcnt

    def _InterlockedDecrement(ob):
        _acquire()
        refcnt = ob.value - 1
        ob.value = refcnt
        _release()
        return refcnt
else:
    _InterlockedIncrement.argtypes = [POINTER(c_long)]
    _InterlockedDecrement.argtypes = [POINTER(c_long)]
    _InterlockedIncrement.restype = c_long
    _InterlockedDecrement.restype = c_long


class LocalServer(object):

    _queue = None

    def run(self, classobjects):
        # Use windll instead of oledll so that we don't get an
        # exception on a FAILED hresult:
        result = windll.ole32.CoInitialize(None)
        if RPC_E_CHANGED_MODE == result:
            # we're running in MTA: no message pump needed
            _debug("Server running in MTA")
            self.run_mta()
        else:
            # we're running in STA: need a message pump
            _debug("Server running in STA")
            if result >= 0:
                # we need a matching CoUninitialize() call for a successful
                # CoInitialize().
                windll.ole32.CoUninitialize()
            self.run_sta()

        for obj in classobjects:
            obj._revoke_class()

    def run_sta(self):
        from comtypes import messageloop
        messageloop.run()

    def run_mta(self):
        import Queue
        self._queue = Queue.Queue()
        self._queue.get()

    def Lock(self):
        oledll.ole32.CoAddRefServerProcess()

    def Unlock(self):
        rc = oledll.ole32.CoReleaseServerProcess()
        if rc == 0:
            if self._queue:
                self._queue.put(42)
            else:
                windll.user32.PostQuitMessage(0)


class InprocServer(object):

    def __init__(self):
        self.locks = c_long(0)

    def Lock(self):
        _InterlockedIncrement(self.locks)

    def Unlock(self):
        _InterlockedDecrement(self.locks)

    def DllCanUnloadNow(self):
        if self.locks.value:
            return S_FALSE
        if COMObject._instances_:
            return S_FALSE
        return S_OK


class COMObject(object):
    _instances_ = {}

    def __new__(cls, *args, **kw):
        self = super(COMObject, cls).__new__(cls)
        if isinstance(self, c_void_p):
            # We build the VTables only for direct instances of
            # CoClass, not for POINTERs to CoClass.
            return self
        if hasattr(self, "_com_interfaces_"):
            self.__prepare_comobject()
        return self

    def __prepare_comobject(self):
        # When a CoClass instance is created, COM pointers to all
        # interfaces are created.  Also, the CoClass must be kept alive as
        # until the COM reference count drops to zero, even if no Python
        # code keeps a reference to the object.
        #
        # The _com_pointers_ instance variable maps string interface iids
        # to C compatible COM pointers.
        self._com_pointers_ = {}
        # COM refcount starts at zero.
        self._refcnt = c_long(0)

        # Some interfaces have a default implementation in COMObject:
        # - ISupportErrorInfo
        # - IPersist (if the subclass has a _reg_clsid_ attribute)
        # - IProvideClassInfo (if the subclass has a _reg_clsid_ attribute)
        # - IProvideClassInfo2 (if the subclass has a _outgoing_interfaces_
        #   attribute)
        #
        # Add these if they are not listed in _com_interfaces_.
        interfaces = tuple(self._com_interfaces_)
        if ISupportErrorInfo not in interfaces:
            interfaces += (ISupportErrorInfo,)
        if hasattr(self, "_reg_typelib_"):
            from comtypes.typeinfo import LoadRegTypeLib
            self._COMObject__typelib = LoadRegTypeLib(*self._reg_typelib_)
            if hasattr(self, "_reg_clsid_"):
                if IProvideClassInfo not in interfaces:
                    interfaces += (IProvideClassInfo,)
                if hasattr(self, "_outgoing_interfaces_") and \
                   IProvideClassInfo2 not in interfaces:
                    interfaces += (IProvideClassInfo2,)
        if hasattr(self, "_reg_clsid_"):
            if IPersist not in interfaces:
                interfaces += (IPersist,)
        for itf in interfaces[::-1]:
            self.__make_interface_pointer(itf)

    def __make_interface_pointer(self, itf):
        methods = []  # method implementations
        fields = []  # (name, prototype) for virtual function table
        iids = []  # interface identifiers.
        # iterate over interface inheritance in reverse order to build the
        # virtual function table, and leave out the 'object' base class.
        finder = self._get_method_finder_(itf)
        for interface in itf.__mro__[-2::-1]:
            iids.append(interface._iid_)
            for m in interface._methods_:
                restype, mthname, argtypes, paramflags, idlflags, helptext = m
                proto = WINFUNCTYPE(restype, c_void_p, *argtypes)
                fields.append((mthname, proto))
                mth = finder.get_impl(interface, mthname, paramflags, idlflags)
                methods.append(proto(mth))
        Vtbl = _create_vtbl_type(tuple(fields), itf)
        vtbl = Vtbl(*methods)
        for iid in iids:
            self._com_pointers_[iid] = pointer(pointer(vtbl))
        if hasattr(itf, "_disp_methods_"):
            self._dispimpl_ = {}
            for m in itf._disp_methods_:
                what, mthname, idlflags, restype, argspec = m
                #################
                # What we have:
                #
                # restypes is a ctypes type or None
                # argspec is seq. of (['in'], paramtype, paramname) tuples (or
                # lists?)
                #################
                # What we need:
                #
                # idlflags must contain 'propget', 'propset' and so on:
                # Must be constructed by converting disptype
                #
                # paramflags must be a sequence
                # of (F_IN|F_OUT|F_RETVAL, paramname[, default-value]) tuples
                #
                # comtypes has this function which helps:
                #    def _encode_idl(names):
                #        # convert to F_xxx and sum up "in", "out",
                #        # "retval" values found in _PARAMFLAGS, ignoring
                #        # other stuff.
                #        return sum([_PARAMFLAGS.get(n, 0) for n in names])
                #################

                if what == "DISPMETHOD":
                    if 'propget' in idlflags:
                        invkind = 2  # DISPATCH_PROPERTYGET
                        mthname = "_get_" + mthname
                    elif 'propput' in idlflags:
                        invkind = 4  # DISPATCH_PROPERTYPUT
                        mthname = "_set_" + mthname
                    elif 'propputref' in idlflags:
                        invkind = 8  # DISPATCH_PROPERTYPUTREF
                        mthname = "_setref_" + mthname
                    else:
                        invkind = 1  # DISPATCH_METHOD
                        if restype:
                            argspec = argspec + ((['out'], restype, ""),)
                    self.__make_dispentry(finder, interface, mthname,
                                          idlflags, argspec, invkind)
                elif what == "DISPPROPERTY":
                    # DISPPROPERTY have implicit "out"
                    if restype:
                        argspec += ((['out'], restype, ""),)
                    self.__make_dispentry(finder, interface,
                                          "_get_" + mthname,
                                          idlflags, argspec,
                                          2  # DISPATCH_PROPERTYGET
                                          )
                    if not 'readonly' in idlflags:
                        self.__make_dispentry(finder, interface,
                                              "_set_" + mthname,
                                              idlflags, argspec,
                                              4)  # DISPATCH_PROPERTYPUT
                        # Add DISPATCH_PROPERTYPUTREF also?

    def __make_dispentry(self,
                         finder, interface, mthname,
                         idlflags, argspec, invkind):
        # We build a _dispmap_ entry now that maps invkind and
        # dispid to implementations that the finder finds;
        # IDispatch_Invoke will later call it.
        paramflags = [((_encode_idl(x[0]), x[1]) + tuple(x[3:]))
                      for x in argspec]
        # XXX can the dispid be at a different index?  Check codegenerator.
        dispid = idlflags[0]
        impl = finder.get_impl(interface, mthname, paramflags, idlflags)
        self._dispimpl_[(dispid, invkind)] = impl
        # invkind is really a set of flags; we allow both
        # DISPATCH_METHOD and DISPATCH_PROPERTYGET (win32com uses
        # this, maybe other languages too?)
        if invkind in (1, 2):
            self._dispimpl_[(dispid, 3)] = impl

    def _get_method_finder_(self, itf):
        # This method can be overridden to customize how methods are
        # found.
        return _MethodFinder(self)

    ################################################################
    # LocalServer / InprocServer stuff
    __server__ = None

    @staticmethod
    def __run_inprocserver__():
        if COMObject.__server__ is None:
            COMObject.__server__ = InprocServer()
        elif isinstance(COMObject.__server__, InprocServer):
            pass
        else:
            raise RuntimeError("Wrong server type")

    @staticmethod
    def __run_localserver__(classobjects):
        assert COMObject.__server__ is None
        # XXX Decide whether we are in STA or MTA
        server = COMObject.__server__ = LocalServer()
        server.run(classobjects)
        COMObject.__server__ = None

    @staticmethod
    def __keep__(obj):
        COMObject._instances_[obj] = None
        _debug("%d active COM objects: Added   %r", len(COMObject._instances_),
               obj)
        if COMObject.__server__:
            COMObject.__server__.Lock()

    @staticmethod
    def __unkeep__(obj):
        try:
            del COMObject._instances_[obj]
        except AttributeError:
            _debug("? active COM objects: Removed %r", obj)
        else:
            _debug("%d active COM objects: Removed %r",
                   len(COMObject._instances_), obj)
        _debug("Remaining: %s", COMObject._instances_.keys())
        if COMObject.__server__:
            COMObject.__server__.Unlock()
    #
    ################################################################

    #########################################################
    # IUnknown methods implementations
    def IUnknown_AddRef(self, this,
                        __InterlockedIncrement=_InterlockedIncrement,
                        _debug=_debug):
        result = __InterlockedIncrement(self._refcnt)
        if result == 1:
            self.__keep__(self)
        _debug("%r.AddRef() -> %s", self, result)
        return result

    def _final_release_(self):
        """This method may be overridden in subclasses
        to free allocated resources or so."""
        pass

    def IUnknown_Release(self, this,
                         __InterlockedDecrement=_InterlockedDecrement,
                         _debug=_debug):
        # If this is called at COM shutdown, _InterlockedDecrement()
        # must still be available, although module level variables may
        # have been deleted already - so we supply it as default
        # argument.
        result = __InterlockedDecrement(self._refcnt)
        _debug("%r.Release() -> %s", self, result)
        if result == 0:
            self._final_release_()
            self.__unkeep__(self)
            # Hm, why isn't this cleaned up by the cycle gc?
            self._com_pointers_ = {}
        return result

    def IUnknown_QueryInterface(self, this, riid, ppvObj, _debug=_debug):
        # XXX This is probably too slow.
        # riid[0].hashcode() alone takes 33 us!
        iid = riid[0]
        ptr = self._com_pointers_.get(iid, None)
        if ptr is not None:
            # CopyComPointer(src, dst) calls AddRef!
            _debug("%r.QueryInterface(%s) -> S_OK", self, iid)
            return CopyComPointer(ptr, ppvObj)
        _debug("%r.QueryInterface(%s) -> E_NOINTERFACE", self, iid)
        return E_NOINTERFACE

    def QueryInterface(self, interface):
        "Query the object for an interface pointer"
        # This method is NOT the implementation of
        # IUnknown::QueryInterface, instead it is supposed to be
        # called on an COMObject by user code.  It allows to get COM
        # interface pointers from COMObject instances.
        ptr = self._com_pointers_.get(interface._iid_, None)
        if ptr is None:
            raise COMError(E_NOINTERFACE, FormatError(E_NOINTERFACE),
                           (None, None, 0, None, None))
        # CopyComPointer(src, dst) calls AddRef!
        result = POINTER(interface)()
        CopyComPointer(ptr, byref(result))
        return result

    ################################################################
    # ISupportErrorInfo::InterfaceSupportsErrorInfo implementation
    def ISupportErrorInfo_InterfaceSupportsErrorInfo(self, this, riid):
        if riid[0] in self._com_pointers_:
            return S_OK
        return S_FALSE

    ################################################################
    # IProvideClassInfo::GetClassInfo implementation
    def IProvideClassInfo_GetClassInfo(self):
        try:
            self.__typelib
        except AttributeError:
            raise WindowsError(E_NOTIMPL)
        return self.__typelib.GetTypeInfoOfGuid(self._reg_clsid_)

    ################################################################
    # IProvideClassInfo2::GetGUID implementation

    def IProvideClassInfo2_GetGUID(self, dwGuidKind):
        # GUIDKIND_DEFAULT_SOURCE_DISP_IID = 1
        if dwGuidKind != 1:
            raise WindowsError(E_INVALIDARG)
        return self._outgoing_interfaces_[0]._iid_

    ################################################################
    # IDispatch methods
    @property
    def __typeinfo(self):
        # XXX Looks like this better be a static property, set by the
        # code that sets __typelib also...
        iid = self._com_interfaces_[0]._iid_
        return self.__typelib.GetTypeInfoOfGuid(iid)

    def IDispatch_GetTypeInfoCount(self):
        try:
            self.__typelib
        except AttributeError:
            return 0
        else:
            return 1

    def IDispatch_GetTypeInfo(self, this, itinfo, lcid, ptinfo):
        if itinfo != 0:
            return DISP_E_BADINDEX
        try:
            ptinfo[0] = self.__typeinfo
            return S_OK
        except AttributeError:
            return E_NOTIMPL

    def IDispatch_GetIDsOfNames(self, this, riid, rgszNames, cNames, lcid,
                                rgDispId):
        # This call uses windll instead of oledll so that a failed
        # call to DispGetIDsOfNames will return a HRESULT instead of
        # raising an error.
        try:
            tinfo = self.__typeinfo
        except AttributeError:
            return E_NOTIMPL
        return windll.oleaut32.DispGetIDsOfNames(tinfo,
                                                 rgszNames, cNames, rgDispId)

    def IDispatch_Invoke(self, this, dispIdMember, riid, lcid, wFlags,
                         pDispParams, pVarResult, pExcepInfo, puArgErr):
        try:
            self._dispimpl_
        except AttributeError:
            try:
                tinfo = self.__typeinfo
            except AttributeError:
                # Hm, we pretend to implement IDispatch, but have no
                # typeinfo, and so cannot fulfill the contract.  Should we
                # better return E_NOTIMPL or DISP_E_MEMBERNOTFOUND?  Some
                # clients call IDispatch_Invoke with 'known' DISPID_...'
                # values, without going through GetIDsOfNames first.
                return DISP_E_MEMBERNOTFOUND
            # This call uses windll instead of oledll so that a failed
            # call to DispInvoke will return a HRESULT instead of raising
            # an error.
            interface = self._com_interfaces_[0]
            ptr = self._com_pointers_[interface._iid_]
            return windll.oleaut32.DispInvoke(
                ptr, tinfo, dispIdMember, wFlags, pDispParams, pVarResult,
                pExcepInfo, puArgErr
            )

        try:
            # XXX Hm, wFlags should be considered a SET of flags...
            mth = self._dispimpl_[(dispIdMember, wFlags)]
        except KeyError:
            return DISP_E_MEMBERNOTFOUND

        # Unpack the parameters: It would be great if we could use the
        # DispGetParam function - but we cannot since it requires that
        # we pass a VARTYPE for each argument and we do not know that.
        #
        # Seems that n arguments have dispids (0, 1, ..., n-1).
        # Unnamed arguments are packed into the DISPPARAMS array in
        # reverse order (starting with the highest dispid), named
        # arguments are packed in the order specified by the
        # rgdispidNamedArgs array.
        #
        params = pDispParams[0]

        if wFlags & (4 | 8):
            # DISPATCH_PROPERTYPUT
            # DISPATCH_PROPERTYPUTREF
            #
            # How are the parameters unpacked for propertyput
            # operations with additional parameters?  Can propput
            # have additional args?
            args = [params.rgvarg[i].value
                    for i in reversed(range(params.cNamedArgs))]
            # MSDN: pVarResult is ignored if DISPATCH_PROPERTYPUT or
            # DISPATCH_PROPERTYPUTREF is specified.
            return mth(this, *args)

        else:
            # DISPATCH_METHOD
            # DISPATCH_PROPERTYGET
            # the positions of named arguments
            #
            # 2to3 has problems to translate 'range(...)[::-1]'
            # correctly, so use 'list(range)[::-1]' instead (will be
            # fixed in Python 3.1, probably):
            named_indexes = [params.rgdispidNamedArgs[i]
                             for i in range(params.cNamedArgs)]
            # the positions of unnamed arguments
            num_unnamed = params.cArgs - params.cNamedArgs
            unnamed_indexes = list(reversed(range(num_unnamed)))
            # It seems that this code calculates the indexes of the
            # parameters in the params.rgvarg array correctly.
            indexes = named_indexes + unnamed_indexes
            args = [params.rgvarg[i].value for i in indexes]

            if pVarResult and getattr(mth, "has_outargs", False):
                args.append(pVarResult)
            return mth(this, *args)

    ################################################################
    # IPersist interface
    def IPersist_GetClassID(self):
        return self._reg_clsid_

__all__ = ["COMObject"]
