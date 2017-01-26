import threading
import array
from ctypes import (POINTER, Structure, byref, cast, c_long, memmove, pointer,
                    sizeof)
from comtypes import _safearray, IUnknown, com_interface_registry, npsupport
from comtypes.patcher import Patch

numpy = npsupport.numpy
_safearray_type_cache = {}


class _SafeArrayAsNdArrayContextManager(object):
    '''Context manager allowing safe arrays to be extracted as ndarrays.

    This is thread-safe.

    Example
    -------

    This works in python >= 2.5
    >>> with safearray_as_ndarray:
    >>>     my_arr = com_object.AsSafeArray
    >>> type(my_arr)
    numpy.ndarray

    '''
    thread_local = threading.local()

    def __enter__(self):
        try:
            self.thread_local.count += 1
        except AttributeError:
            self.thread_local.count = 1

    def __exit__(self, exc_type, exc_value, traceback):
        self.thread_local.count -= 1

    def __nonzero__(self):
        '''True if context manager is currently entered on given thread.

        '''
        return bool(getattr(self.thread_local, 'count', 0))


# Global _SafeArrayAsNdArrayContextManager
safearray_as_ndarray = _SafeArrayAsNdArrayContextManager()


################################################################
# This is THE PUBLIC function: the gateway to the SAFEARRAY functionality.
def _midlSAFEARRAY(itemtype):
    """This function mimics the 'SAFEARRAY(aType)' IDL idiom.  It
    returns a subtype of SAFEARRAY, instances will be built with a
    typecode VT_...  corresponding to the aType, which must be one of
    the supported ctypes.
    """
    try:
        return POINTER(_safearray_type_cache[itemtype])
    except KeyError:
        sa_type = _make_safearray_type(itemtype)
        _safearray_type_cache[itemtype] = sa_type
        return POINTER(sa_type)


def _make_safearray_type(itemtype):
    # Create and return a subclass of tagSAFEARRAY
    from comtypes.automation import _ctype_to_vartype, VT_RECORD, \
         VT_UNKNOWN, IDispatch, VT_DISPATCH

    meta = type(_safearray.tagSAFEARRAY)
    sa_type = meta.__new__(meta,
                           "SAFEARRAY_%s" % itemtype.__name__,
                           (_safearray.tagSAFEARRAY,), {})

    try:
        vartype = _ctype_to_vartype[itemtype]
        extra = None
    except KeyError:
        if issubclass(itemtype, Structure):
            try:
                guids = itemtype._recordinfo_
            except AttributeError:
                extra = None
            else:
                from comtypes.typeinfo import GetRecordInfoFromGuids
                extra = GetRecordInfoFromGuids(*guids)
            vartype = VT_RECORD
        elif issubclass(itemtype, POINTER(IDispatch)):
            vartype = VT_DISPATCH
            extra = pointer(itemtype._iid_)
        elif issubclass(itemtype, POINTER(IUnknown)):
            vartype = VT_UNKNOWN
            extra = pointer(itemtype._iid_)
        else:
            raise TypeError(itemtype)

    @Patch(POINTER(sa_type))
    class _(object):
        # Should explain the ideas how SAFEARRAY is used in comtypes
        _itemtype_ = itemtype  # a ctypes type
        _vartype_ = vartype  # a VARTYPE value: VT_...
        _needsfree = False

        @classmethod
        def create(cls, value, extra=None):
            """Create a POINTER(SAFEARRAY_...) instance of the correct
            type; value is an object containing the items to store.

            Python lists, tuples, and array.array instances containing
            compatible item types can be passed to create
            one-dimensional arrays.  To create multidimensional arrys,
            numpy arrays must be passed.
            """
            if npsupport.isndarray(value):
                return cls.create_from_ndarray(value, extra)

            # For VT_UNKNOWN or VT_DISPATCH, extra must be a pointer to
            # the GUID of the interface.
            #
            # For VT_RECORD, extra must be a pointer to an IRecordInfo
            # describing the record.

            # XXX How to specify the lbound (3. parameter to CreateVectorEx)?
            # XXX How to write tests for lbound != 0?
            pa = _safearray.SafeArrayCreateVectorEx(cls._vartype_,
                                                    0,
                                                    len(value),
                                                    extra)
            if not pa:
                if cls._vartype_ == VT_RECORD and extra is None:
                    raise TypeError("Cannot create SAFEARRAY type VT_RECORD without IRecordInfo.")
                # Hm, there may be other reasons why the creation fails...
                raise MemoryError()
            # We now have a POINTER(tagSAFEARRAY) instance which we must cast
            # to the correct type:
            pa = cast(pa, cls)
            # Now, fill the data in:
            ptr = POINTER(cls._itemtype_)()  # container for the values
            _safearray.SafeArrayAccessData(pa, byref(ptr))
            try:
                if isinstance(value, array.array):
                    addr, n = value.buffer_info()
                    nbytes = len(value) * sizeof(cls._itemtype_)
                    memmove(ptr, addr, nbytes)
                else:
                    for index, item in enumerate(value):
                        ptr[index] = item
            finally:
                _safearray.SafeArrayUnaccessData(pa)
            return pa

        @classmethod
        def create_from_ndarray(cls, value, extra, lBound=0):
            from comtypes.automation import VARIANT
            # If processing VARIANT, makes sure the array type is correct.
            if cls._itemtype_ is VARIANT:
                if value.dtype != npsupport.VARIANT_dtype:
                    value = _ndarray_to_variant_array(value)
            else:
                ai = value.__array_interface__
                if ai["version"] != 3:
                    raise TypeError("only __array_interface__ version 3 supported")
                if cls._itemtype_ != numpy.ctypeslib._typecodes[ai["typestr"]]:
                    raise TypeError("Wrong array item type")

            # SAFEARRAYs have Fortran order; convert the numpy array if needed
            if not value.flags.f_contiguous:
                value = numpy.array(value, order="F")

            # For VT_UNKNOWN or VT_DISPATCH, extra must be a pointer to
            # the GUID of the interface.
            #
            # For VT_RECORD, extra must be a pointer to an IRecordInfo
            # describing the record.
            rgsa = (_safearray.SAFEARRAYBOUND * value.ndim)()
            nitems = 1
            for i, d in enumerate(value.shape):
                nitems *= d
                rgsa[i].cElements = d
                rgsa[i].lBound = lBound
            pa = _safearray.SafeArrayCreateEx(cls._vartype_,
                                              value.ndim,  # cDims
                                              rgsa,  # rgsaBound
                                              extra)  # pvExtra
            if not pa:
                if cls._vartype_ == VT_RECORD and extra is None:
                    raise TypeError("Cannot create SAFEARRAY type VT_RECORD without IRecordInfo.")
                # Hm, there may be other reasons why the creation fails...
                raise MemoryError()
            # We now have a POINTER(tagSAFEARRAY) instance which we must cast
            # to the correct type:
            pa = cast(pa, cls)
            # Now, fill the data in:
            ptr = POINTER(cls._itemtype_)()  # pointer to the item values
            _safearray.SafeArrayAccessData(pa, byref(ptr))
            try:
                nbytes = nitems * sizeof(cls._itemtype_)
                memmove(ptr, value.ctypes.data, nbytes)
            finally:
                _safearray.SafeArrayUnaccessData(pa)
            return pa

        @classmethod
        def from_param(cls, value):
            if not isinstance(value, cls):
                value = cls.create(value, extra)
                value._needsfree = True
            return value

        def __getitem__(self, index):
            # pparray[0] returns the whole array contents.
            if index != 0:
                raise IndexError("Only index 0 allowed")
            return self.unpack()

        def __setitem__(self, index, value):
            # XXX Need this to implement [in, out] safearrays in COM servers!
##            print "__setitem__", index, value
            raise TypeError("Setting items not allowed")

        def __ctypes_from_outparam__(self):
            self._needsfree = True
            return self[0]

        def __del__(self, _SafeArrayDestroy=_safearray.SafeArrayDestroy):
            if self._needsfree:
                _SafeArrayDestroy(self)

        def _get_size(self, dim):
            "Return the number of elements for dimension 'dim'"
            ub = _safearray.SafeArrayGetUBound(self, dim) + 1
            lb = _safearray.SafeArrayGetLBound(self, dim)
            return ub - lb

        def unpack(self):
            """Unpack a POINTER(SAFEARRAY_...) into a Python tuple or ndarray."""
            dim = _safearray.SafeArrayGetDim(self)

            if dim == 1:
                num_elements = self._get_size(1)
                result = self._get_elements_raw(num_elements)
                if safearray_as_ndarray:
                    import numpy
                    return numpy.asarray(result)
                return tuple(result)
            elif dim == 2:
                # get the number of elements in each dimension
                rows, cols = self._get_size(1), self._get_size(2)
                # get all elements
                result = self._get_elements_raw(rows * cols)
                # this must be reshaped and transposed because it is
                # flat, and in VB order
                if safearray_as_ndarray:
                    import numpy
                    return numpy.asarray(result).reshape((cols, rows)).T
                result = [tuple(result[r::rows]) for r in range(rows)]
                return tuple(result)
            else:
                lowerbounds = [_safearray.SafeArrayGetLBound(self, d)
                               for d in range(1, dim+1)]
                indexes = (c_long * dim)(*lowerbounds)
                upperbounds = [_safearray.SafeArrayGetUBound(self, d)
                               for d in range(1, dim+1)]
                row = self._get_row(0, indexes, lowerbounds, upperbounds)
                if safearray_as_ndarray:
                    import numpy
                    return numpy.asarray(row)
                return row

        def _get_elements_raw(self, num_elements):
            """Returns a flat list or ndarray containing ALL elements in
            the safearray."""
            from comtypes.automation import VARIANT
            # XXX Not sure this is true:
            # For VT_UNKNOWN and VT_DISPATCH, we should retrieve the
            # interface iid by SafeArrayGetIID().
            ptr = POINTER(self._itemtype_)()  # container for the values
            _safearray.SafeArrayAccessData(self, byref(ptr))
            try:
                if self._itemtype_ == VARIANT:
                    # We have to loop over each item, so we get no
                    # speedup by creating an ndarray here.
                    return [i.value for i in ptr[:num_elements]]
                elif issubclass(self._itemtype_, POINTER(IUnknown)):
                    iid = _safearray.SafeArrayGetIID(self)
                    itf = com_interface_registry[str(iid)]
                    # COM interface pointers retrieved from array
                    # must be AddRef()'d if non-NULL.
                    elems = ptr[:num_elements]
                    result = []
                    # We have to loop over each item, so we get no
                    # speedup by creating an ndarray here.
                    for p in elems:
                        if bool(p):
                            p.AddRef()
                            result.append(p.QueryInterface(itf))
                        else:
                            # return a NULL-interface pointer.
                            result.append(POINTER(itf)())
                    return result
                else:
                    # If the safearray element are NOT native python
                    # objects, the containing safearray must be kept
                    # alive until all the elements are destroyed.
                    if not issubclass(self._itemtype_, Structure):
                        # Create an ndarray if requested. This is where
                        # we can get the most speed-up.
                        # XXX Only try to convert types known to
                        #     numpy.ctypeslib.
                        if (safearray_as_ndarray and self._itemtype_ in
                                numpy.ctypeslib._typecodes.values()):
                            arr = numpy.ctypeslib.as_array(ptr,
                                                           (num_elements,))
                            return arr.copy()
                        return ptr[:num_elements]

                    def keep_safearray(v):
                        v.__keepref = self
                        return v
                    return [keep_safearray(x) for x in ptr[:num_elements]]
            finally:
                _safearray.SafeArrayUnaccessData(self)

        def _get_row(self, dim, indices, lowerbounds, upperbounds):
            # loop over the index of dimension 'dim'
            # we have to restore the index of the dimension we're looping over
            restore = indices[dim]

            result = []
            obj = self._itemtype_()
            pobj = byref(obj)
            if dim+1 == len(indices):
                # It should be faster to lock the array and get a whole row at once?
                # How to calculate the pointer offset?
                for i in range(indices[dim], upperbounds[dim]+1):
                    indices[dim] = i
                    _safearray.SafeArrayGetElement(self, indices, pobj)
                    result.append(obj.value)
            else:
                for i in range(indices[dim], upperbounds[dim]+1):
                    indices[dim] = i
                    result.append(self._get_row(dim+1, indices, lowerbounds, upperbounds))
            indices[dim] = restore
            return tuple(result) # for compatibility with pywin32.

    @Patch(POINTER(POINTER(sa_type)))
    class __(object):

        @classmethod
        def from_param(cls, value):
            if isinstance(value, cls._type_):
                return byref(value)
            return byref(cls._type_.create(value, extra))

        def __setitem__(self, index, value):
            # create an LP_SAFEARRAY_... instance
            pa = self._type_.create(value, extra)
            # XXX Must we destroy the currently contained data?
            # fill it into self
            super(POINTER(POINTER(sa_type)), self).__setitem__(index, pa)

    return sa_type


def _ndarray_to_variant_array(value):
    """ Convert an ndarray to VARIANT_dtype array """
    # Check that variant arrays are supported
    if npsupport.VARIANT_dtype is None:
        msg = "VARIANT ndarrays require NumPy 1.7 or newer."
        raise RuntimeError(msg)

    # special cases
    if numpy.issubdtype(value.dtype, npsupport.datetime64):
        return _datetime64_ndarray_to_variant_array(value)

    from comtypes.automation import VARIANT
    # Empty array
    varr = numpy.zeros(value.shape, npsupport.VARIANT_dtype, order='F')
    # Convert each value to a variant and put it in the array.
    varr.flat = [VARIANT(v) for v in value.flat]
    return varr


def _datetime64_ndarray_to_variant_array(value):
    """ Convert an ndarray of datetime64 to VARIANT_dtype array """
    # The OLE automation date format is a floating point value, counting days
    # since midnight 30 December 1899. Hours and minutes are represented as
    # fractional days.
    from comtypes.automation import VT_DATE
    value = numpy.array(value, "datetime64[ns]")
    value = value - npsupport.com_null_date64
    # Convert to days
    value = value / numpy.timedelta64(1, 'D')
    varr = numpy.zeros(value.shape, npsupport.VARIANT_dtype, order='F')
    varr['vt'] = VT_DATE
    varr['_']['VT_R8'].flat = value.flat
    return varr
