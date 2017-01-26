import ctypes
import traceback
import comtypes
import comtypes.hresult
import comtypes.automation
import comtypes.typeinfo
import comtypes.connectionpoints
from comtypes.client._generate import GetModule
import logging
logger = logging.getLogger(__name__)

class _AdviseConnection(object):
    def __init__(self, source, interface, receiver):
        self.cp = None
        self.cookie = None
        self.receiver = None
        self._connect(source, interface, receiver)

    def _connect(self, source, interface, receiver):
        cpc = source.QueryInterface(comtypes.connectionpoints.IConnectionPointContainer)
        self.cp = cpc.FindConnectionPoint(ctypes.byref(interface._iid_))
        logger.debug("Start advise %s", interface)
        self.cookie = self.cp.Advise(receiver)
        self.receiver = receiver

    def disconnect(self):
        if self.cookie:
            self.cp.Unadvise(self.cookie)
            logger.debug("Unadvised %s", self.cp)
            self.cp = None
            self.cookie = None
            del self.receiver

    def __del__(self):
        try:
            if self.cookie is not None:
                self.cp.Unadvise(self.cookie)
        except (comtypes.COMError, WindowsError):
            # Are we sure we want to ignore errors here?
            pass

def FindOutgoingInterface(source):
    """XXX Describe the strategy that is used..."""
    # If the COM object implements IProvideClassInfo2, it is easy to
    # find the default outgoing interface.
    try:
        pci = source.QueryInterface(comtypes.typeinfo.IProvideClassInfo2)
        guid = pci.GetGUID(1)
    except comtypes.COMError:
        pass
    else:
        # another try: block needed?
        try:
            interface = comtypes.com_interface_registry[str(guid)]
        except KeyError:
            tinfo = pci.GetClassInfo()
            tlib, index = tinfo.GetContainingTypeLib()
            GetModule(tlib)
            interface = comtypes.com_interface_registry[str(guid)]
        logger.debug("%s using sinkinterface %s", source, interface)
        return interface

    # If we can find the CLSID of the COM object, we can look for a
    # registered outgoing interface (__clsid has been set by
    # comtypes.client):
    clsid = source.__dict__.get('__clsid')
    try:
        interface = comtypes.com_coclass_registry[clsid]._outgoing_interfaces_[0]
    except KeyError:
        pass
    else:
        logger.debug("%s using sinkinterface from clsid %s", source, interface)
        return interface

##    interface = find_single_connection_interface(source)
##    if interface:
##        return interface

    raise TypeError("cannot determine source interface")

def find_single_connection_interface(source):
    # Enumerate the connection interfaces.  If we find a single one,
    # return it, if there are more, we give up since we cannot
    # determine which one to use.
    cpc = source.QueryInterface(comtypes.connectionpoints.IConnectionPointContainer)
    enum = cpc.EnumConnectionPoints()
    iid = enum.next().GetConnectionInterface()
    try:
        enum.next()
    except StopIteration:
        try:
            interface = comtypes.com_interface_registry[str(iid)]
        except KeyError:
            return None
        else:
            logger.debug("%s using sinkinterface from iid %s", source, interface)
            return interface
    else:
        logger.debug("%s has more than one connection point", source)

    return None

def report_errors(func):
    # This decorator preserves parts of the decorated function
    # signature, so that the comtypes special-casing for the 'this'
    # parameter still works.
    if func.func_code.co_varnames[:2] == ('self', 'this'):
        def error_printer(self, this, *args, **kw):
            try:
                return func(self, this, *args, **kw)
            except:
                traceback.print_exc()
                raise
    else:
        def error_printer(*args, **kw):
            try:
                return func(*args, **kw)
            except:
                traceback.print_exc()
                raise
    return error_printer

from comtypes._comobject import _MethodFinder
class _SinkMethodFinder(_MethodFinder):
    """Special MethodFinder, for finding and decorating event handler
    methods.  Looks for methods on two objects. Also decorates the
    event handlers with 'report_errors' which will print exceptions in
    event handlers.
    """
    def __init__(self, inst, sink):
        super(_SinkMethodFinder, self).__init__(inst)
        self.sink = sink

    def find_method(self, fq_name, mthname):
        impl = self._find_method(fq_name, mthname)
        # Caller of this method catches AttributeError,
        # so we need to be careful in the following code
        # not to raise one...
        try:
            # impl is a bound method, dissect it...
            im_self, im_func = impl.im_self, impl.im_func
            # decorate it with an error printer...
            method = report_errors(im_func)
            # and make a new bound method from it again.
            return comtypes.instancemethod(method,
                                           im_self,
                                           type(im_self))
        except AttributeError, details:
            raise RuntimeError(details)

    def _find_method(self, fq_name, mthname):
        try:
            return super(_SinkMethodFinder, self).find_method(fq_name, mthname)
        except AttributeError:
            try:
                return getattr(self.sink, fq_name)
            except AttributeError:
                return getattr(self.sink, mthname)

def CreateEventReceiver(interface, handler):

    class Sink(comtypes.COMObject):
        _com_interfaces_ = [interface]

        def _get_method_finder_(self, itf):
            # Use a special MethodFinder that will first try 'self',
            # then the sink.
            return _SinkMethodFinder(self, handler)

    sink = Sink()

    # Since our Sink object doesn't have typeinfo, it needs a
    # _dispimpl_ dictionary to dispatch events received via Invoke.
    if issubclass(interface, comtypes.automation.IDispatch) \
           and not hasattr(sink, "_dispimpl_"):
        finder = sink._get_method_finder_(interface)
        dispimpl = sink._dispimpl_ = {}
        for m in interface._methods_:
            restype, mthname, argtypes, paramflags, idlflags, helptext = m
            # Can dispid be at a different index? Should check code generator...
            # ...but hand-written code should also work...
            dispid = idlflags[0]
            impl = finder.get_impl(interface, mthname, paramflags, idlflags)
            # XXX Wouldn't work for 'propget', 'propput', 'propputref'
            # methods - are they allowed on event interfaces?
            dispimpl[(dispid, comtypes.automation.DISPATCH_METHOD)] = impl

    return sink

def GetEvents(source, sink, interface=None):
    """Receive COM events from 'source'.  Events will call methods on
    the 'sink' object.  'interface' is the source interface to use.
    """
    # When called from CreateObject, the sourceinterface has already
    # been determined by the coclass.  Otherwise, the only thing that
    # makes sense is to use IProvideClassInfo2 to get the default
    # source interface.
    if interface is None:
        interface = FindOutgoingInterface(source)

    rcv = CreateEventReceiver(interface, sink)
    return _AdviseConnection(source, interface, rcv)

class EventDumper(object):
    """Universal sink for COM events."""

    def __getattr__(self, name):
        "Create event handler methods on demand"
        if name.startswith("__") and name.endswith("__"):
            raise AttributeError(name)
        print "# event found:", name
        def handler(self, this, *args, **kw):
            # XXX handler is called with 'this'.  Should we really print "None" instead?
            args = (None,) + args
            print "Event %s(%s)" % (name, ", ".join([repr(a) for a in args]))
        return comtypes.instancemethod(handler, self, EventDumper)

def ShowEvents(source, interface=None):
    """Receive COM events from 'source'.  A special event sink will be
    used that first prints the names of events that are found in the
    outgoing interface, and will also print out the events when they
    are fired.
    """
    return comtypes.client.GetEvents(source, sink=EventDumper(), interface=interface)

# This type is used inside 'PumpEvents', but if we create the type
# afresh each time 'PumpEvents' is called we end up creating cyclic
# garbage for each call.  So we define it here instead.
_handles_type = ctypes.c_void_p * 1

def PumpEvents(timeout):
    """This following code waits for 'timeout' seconds in the way
    required for COM, internally doing the correct things depending
    on the COM appartment of the current thread.  It is possible to
    terminate the message loop by pressing CTRL+C, which will raise
    a KeyboardInterrupt.
    """
    # XXX Should there be a way to pass additional event handles which
    # can terminate this function?

    # XXX XXX XXX
    #
    # It may be that I misunderstood the CoWaitForMultipleHandles
    # function.  Is a message loop required in a STA?  Seems so...
    #
    # MSDN says:
    #
    # If the caller resides in a single-thread apartment,
    # CoWaitForMultipleHandles enters the COM modal loop, and the
    # thread's message loop will continue to dispatch messages using
    # the thread's message filter. If no message filter is registered
    # for the thread, the default COM message processing is used.
    #
    # If the calling thread resides in a multithread apartment (MTA),
    # CoWaitForMultipleHandles calls the Win32 function
    # MsgWaitForMultipleObjects.

    hevt = ctypes.windll.kernel32.CreateEventA(None, True, False, None)
    handles = _handles_type(hevt)
    RPC_S_CALLPENDING = -2147417835

##    @ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_uint)
    def HandlerRoutine(dwCtrlType):
        if dwCtrlType == 0: # CTRL+C
            ctypes.windll.kernel32.SetEvent(hevt)
            return 1
        return 0
    HandlerRoutine = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_uint)(HandlerRoutine)

    ctypes.windll.kernel32.SetConsoleCtrlHandler(HandlerRoutine, 1)

    try:
        try:
            res = ctypes.oledll.ole32.CoWaitForMultipleHandles(0,
                                                               int(timeout * 1000),
                                                               len(handles), handles,
                                                               ctypes.byref(ctypes.c_ulong()))
        except WindowsError, details:
            if details.args[0] != RPC_S_CALLPENDING: # timeout expired
                raise
        else:
            raise KeyboardInterrupt
    finally:
        ctypes.windll.kernel32.CloseHandle(hevt)
        ctypes.windll.kernel32.SetConsoleCtrlHandler(HandlerRoutine, 0)
