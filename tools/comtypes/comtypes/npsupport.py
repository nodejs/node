""" Consolidation of numpy support utilities. """
import sys

try:
    import numpy
except ImportError:
    numpy = None


HAVE_NUMPY = numpy is not None

is_64bits = sys.maxsize > 2**32


def _make_variant_dtype():
    """ Create a dtype for VARIANT. This requires support for Unions, which is
    available in numpy version 1.7 or greater.

    This does not support the decimal type.

    Returns None if the dtype cannot be created.

    """

    # pointer typecode
    ptr_typecode = '<u8' if is_64bits else '<u4'

    _tagBRECORD_format = [
        ('pvRecord', ptr_typecode),
        ('pRecInfo', ptr_typecode),
    ]

    # overlapping typecodes only allowed in numpy version 1.7 or greater
    U_VARIANT_format = dict(
        names=[
            'VT_BOOL', 'VT_I1', 'VT_I2', 'VT_I4', 'VT_I8', 'VT_INT', 'VT_UI1',
            'VT_UI2', 'VT_UI4', 'VT_UI8', 'VT_UINT', 'VT_R4', 'VT_R8', 'VT_CY',
            'c_wchar_p', 'c_void_p', 'pparray', 'bstrVal', '_tagBRECORD',
        ],
        formats=[
            '<i2', '<i1', '<i2', '<i4', '<i8', '<i4', '<u1', '<u2', '<u4',
            '<u8', '<u4', '<f4', '<f8', '<i8', ptr_typecode, ptr_typecode,
            ptr_typecode, ptr_typecode, _tagBRECORD_format,
        ],
        offsets=[0] * 19  # This is what makes it a union
    )

    tagVARIANT_format = [
        ("vt", '<u2'),
        ("wReserved1", '<u2'),
        ("wReserved2", '<u2'),
        ("wReserved3", '<u2'),
        ("_", U_VARIANT_format),
    ]

    return numpy.dtype(tagVARIANT_format)


def isndarray(value):
    """ Check if a value is an ndarray.

    This cannot succeed if numpy is not available.

    """
    if not HAVE_NUMPY:
        return False
    return isinstance(value, numpy.ndarray)


def isdatetime64(value):
    """ Check if a value is a datetime64.

    This cannot succeed if datetime64 is not available.

    """
    if not HAVE_NUMPY:
        return False
    return isinstance(value, datetime64)


com_null_date64 = None
datetime64 = None
VARIANT_dtype = None

if HAVE_NUMPY:

    # dtype for VARIANT. This allows for packing of variants into an array, and
    # subsequent conversion to a multi-dimensional safearray.
    try:
        VARIANT_dtype = _make_variant_dtype()
    except ValueError:
        pass

    # This simplifies dependent modules
    try:
        from numpy import datetime64
    except ImportError:
        pass
    else:
        try:
            # This does not work on numpy 1.6
            com_null_date64 = datetime64("1899-12-30T00:00:00", "ns")
        except TypeError:
            pass
