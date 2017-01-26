import comtypes
import comtypes.automation

from comtypes.automation import IEnumVARIANT
from comtypes.automation import DISPATCH_METHOD
from comtypes.automation import DISPATCH_PROPERTYGET
from comtypes.automation import DISPATCH_PROPERTYPUT
from comtypes.automation import DISPATCH_PROPERTYPUTREF

from comtypes.automation import DISPID_VALUE
from comtypes.automation import DISPID_NEWENUM

from comtypes.typeinfo import FUNC_PUREVIRTUAL, FUNC_DISPATCH


class FuncDesc(object):
    """Stores important FUNCDESC properties by copying them from a
    real FUNCDESC instance.
    """
    def __init__(self, **kw):
        self.__dict__.update(kw)

# What is missing?
#
# Should NamedProperty support __call__()?

_all_slice = slice(None, None, None)


class NamedProperty(object):
    def __init__(self, disp, get, put, putref):
        self.get = get
        self.put = put
        self.putref = putref
        self.disp = disp

    def __getitem__(self, arg):
        if self.get is None:
            raise TypeError("unsubscriptable object")
        if isinstance(arg, tuple):
            return self.disp._comobj._invoke(self.get.memid,
                                             self.get.invkind,
                                             0,
                                             *arg)
        elif arg == _all_slice:
            return self.disp._comobj._invoke(self.get.memid,
                                             self.get.invkind,
                                             0)
        return self.disp._comobj._invoke(self.get.memid,
                                         self.get.invkind,
                                         0,
                                         *[arg])

    def __call__(self, *args):
        if self.get is None:
            raise TypeError("object is not callable")
        return self.disp._comobj._invoke(self.get.memid,
                                            self.get.invkind,
                                            0,
                                            *args)

    def __setitem__(self, name, value):
        # See discussion in Dispatch.__setattr__ below.
        if self.put is None and self.putref is None:
            raise TypeError("object does not support item assignment")
        if comtypes._is_object(value):
            descr = self.putref or self.put
        else:
            descr = self.put or self.putref
        if isinstance(name, tuple):
            self.disp._comobj._invoke(descr.memid,
                                      descr.invkind,
                                      0,
                                      *(name + (value,)))
        elif name == _all_slice:
            self.disp._comobj._invoke(descr.memid,
                                      descr.invkind,
                                      0,
                                      value)
        else:
            self.disp._comobj._invoke(descr.memid,
                                      descr.invkind,
                                      0,
                                      name,
                                      value)

    def __iter__(self):
        """ Explicitly disallow iteration. """
        msg = "%r is not iterable" % self.disp
        raise TypeError(msg)


# The following 'Dispatch' class, returned from
#    CreateObject(progid, dynamic=True)
# differ in behaviour from objects created with
#    CreateObject(progid, dynamic=False)
# (let us call the latter 'Custom' objects for this discussion):
#
#
# 1. Dispatch objects support __call__(), custom objects do not
#
# 2. Custom objects method support named arguments, Dispatch
#    objects do not (could be added, would probably be expensive)

class Dispatch(object):
    """Dynamic dispatch for an object the exposes type information.
    Binding at runtime is done via ITypeComp::Bind calls.
    """
    def __init__(self, comobj, tinfo):
        self.__dict__["_comobj"] = comobj
        self.__dict__["_tinfo"] = tinfo
        self.__dict__["_tcomp"] = tinfo.GetTypeComp()
        self.__dict__["_tdesc"] = {}
##        self.__dict__["_iid"] = tinfo.GetTypeAttr().guid

    def __bind(self, name, invkind):
        """Bind (name, invkind) and return a FuncDesc instance or
        None.  Results (even unsuccessful ones) are cached."""
        # We could cache the info in the class instead of the
        # instance, but we would need an additional key for that:
        # self._iid
        try:
            return self._tdesc[(name, invkind)]
        except KeyError:
            try:
                descr = self._tcomp.Bind(name, invkind)[1]
            except comtypes.COMError:
                info = None
            else:
                # Using a separate instance to store interesting
                # attributes of descr avoids that the typecomp instance is
                # kept alive...
                info = FuncDesc(memid=descr.memid,
                                invkind=descr.invkind,
                                cParams=descr.cParams,
                                funckind=descr.funckind)
            self._tdesc[(name, invkind)] = info
            return info

    def QueryInterface(self, *args):
        "QueryInterface is forwarded to the real com object."
        return self._comobj.QueryInterface(*args)

    def __cmp__(self, other):
        if not isinstance(other, Dispatch):
            return 1
        return cmp(self._comobj, other._comobj)

    def __eq__(self, other):
        return isinstance(other, Dispatch) and \
               self._comobj == other._comobj

    def __hash__(self):
        return hash(self._comobj)

    def __getattr__(self, name):
        """Get a COM attribute."""
        if name.startswith("__") and name.endswith("__"):
            raise AttributeError(name)
        # check for propget or method
        descr = self.__bind(name, DISPATCH_METHOD | DISPATCH_PROPERTYGET)
        if descr is None:
            raise AttributeError(name)
        if descr.invkind == DISPATCH_PROPERTYGET:
            # DISPATCH_PROPERTYGET
            if descr.funckind == FUNC_DISPATCH:
                if descr.cParams == 0:
                    return self._comobj._invoke(descr.memid, descr.invkind, 0)
            elif descr.funckind == FUNC_PUREVIRTUAL:
                # FUNC_PUREVIRTUAL descriptions contain the property
                # itself as a parameter.
                if descr.cParams == 1:
                    return self._comobj._invoke(descr.memid, descr.invkind, 0)
            else:
                raise RuntimeError("funckind %d not yet implemented" % descr.funckind)
            put = self.__bind(name, DISPATCH_PROPERTYPUT)
            putref = self.__bind(name, DISPATCH_PROPERTYPUTREF)
            return NamedProperty(self, descr, put, putref)
        else:
            # DISPATCH_METHOD
            def caller(*args):
                return self._comobj._invoke(descr.memid, descr.invkind, 0, *args)
            try:
                caller.__name__ = name
            except TypeError:
                # In Python 2.3, __name__ is readonly
                pass
            return caller

    def __setattr__(self, name, value):
        # Hm, this can be a propput, a propputref, or 'both' property.
        # (Or nothing at all.)
        #
        # Whether propput or propputref is called will depend on what
        # is available, and on the type of 'value' as determined by
        # comtypes._is_object(value).
        #
        # I think that the following table MAY be correct; although I
        # have no idea whether the cases marked (?) are really valid.
        #
        #  invkind available  |  _is_object(value) | invkind we should use
        #  ---------------------------------------------------------------
        #     put             |     True           |   put      (?)
        #     put             |     False          |   put
        #     putref          |     True           |   putref
        #     putref          |     False          |   putref   (?)
        #     put, putref     |     True           |   putref
        #     put, putref     |     False          |   put
        put = self.__bind(name, DISPATCH_PROPERTYPUT)
        putref = self.__bind(name, DISPATCH_PROPERTYPUTREF)
        if not put and not putref:
            raise AttributeError(name)
        if comtypes._is_object(value):
            descr = putref or put
        else:
            descr = put or putref
        if descr.cParams == 1:
            self._comobj._invoke(descr.memid, descr.invkind, 0, value)
            return
        raise AttributeError(name)

    def __call__(self, *args):
        return self._comobj._invoke(DISPID_VALUE,
                                    DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                                    0,
                                    *args)

    def __getitem__(self, arg):
        if isinstance(arg, tuple):
            args = arg
        elif arg == _all_slice:
            args = ()
        else:
            args = (arg,)

        try:
            return self._comobj._invoke(DISPID_VALUE,
                                        DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                                        0,
                                        *args)
        except comtypes.COMError:
            return iter(self)[arg]

    def __setitem__(self, name, value):
        if comtypes._is_object(value):
            invkind = DISPATCH_PROPERTYPUTREF
        else:
            invkind = DISPATCH_PROPERTYPUT

        if isinstance(name, tuple):
            args = name + (value,)
        elif name == _all_slice:
            args = (value,)
        else:
            args = (name, value)
        return self._comobj._invoke(DISPID_VALUE,
                                    invkind,
                                    0,
                                    *args)

    def __iter__(self):
        punk = self._comobj._invoke(DISPID_NEWENUM,
                                    DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                                    0)
        enum = punk.QueryInterface(IEnumVARIANT)
        enum._dynamic = True
        return enum
