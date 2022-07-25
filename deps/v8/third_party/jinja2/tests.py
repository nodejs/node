# -*- coding: utf-8 -*-
"""Built-in template tests used with the ``is`` operator."""
import decimal
import operator
import re

from ._compat import abc
from ._compat import integer_types
from ._compat import string_types
from ._compat import text_type
from .runtime import Undefined

number_re = re.compile(r"^-?\d+(\.\d+)?$")
regex_type = type(number_re)
test_callable = callable


def test_odd(value):
    """Return true if the variable is odd."""
    return value % 2 == 1


def test_even(value):
    """Return true if the variable is even."""
    return value % 2 == 0


def test_divisibleby(value, num):
    """Check if a variable is divisible by a number."""
    return value % num == 0


def test_defined(value):
    """Return true if the variable is defined:

    .. sourcecode:: jinja

        {% if variable is defined %}
            value of variable: {{ variable }}
        {% else %}
            variable is not defined
        {% endif %}

    See the :func:`default` filter for a simple way to set undefined
    variables.
    """
    return not isinstance(value, Undefined)


def test_undefined(value):
    """Like :func:`defined` but the other way round."""
    return isinstance(value, Undefined)


def test_none(value):
    """Return true if the variable is none."""
    return value is None


def test_boolean(value):
    """Return true if the object is a boolean value.

    .. versionadded:: 2.11
    """
    return value is True or value is False


def test_false(value):
    """Return true if the object is False.

    .. versionadded:: 2.11
    """
    return value is False


def test_true(value):
    """Return true if the object is True.

    .. versionadded:: 2.11
    """
    return value is True


# NOTE: The existing 'number' test matches booleans and floats
def test_integer(value):
    """Return true if the object is an integer.

    .. versionadded:: 2.11
    """
    return isinstance(value, integer_types) and value is not True and value is not False


# NOTE: The existing 'number' test matches booleans and integers
def test_float(value):
    """Return true if the object is a float.

    .. versionadded:: 2.11
    """
    return isinstance(value, float)


def test_lower(value):
    """Return true if the variable is lowercased."""
    return text_type(value).islower()


def test_upper(value):
    """Return true if the variable is uppercased."""
    return text_type(value).isupper()


def test_string(value):
    """Return true if the object is a string."""
    return isinstance(value, string_types)


def test_mapping(value):
    """Return true if the object is a mapping (dict etc.).

    .. versionadded:: 2.6
    """
    return isinstance(value, abc.Mapping)


def test_number(value):
    """Return true if the variable is a number."""
    return isinstance(value, integer_types + (float, complex, decimal.Decimal))


def test_sequence(value):
    """Return true if the variable is a sequence. Sequences are variables
    that are iterable.
    """
    try:
        len(value)
        value.__getitem__
    except Exception:
        return False
    return True


def test_sameas(value, other):
    """Check if an object points to the same memory address than another
    object:

    .. sourcecode:: jinja

        {% if foo.attribute is sameas false %}
            the foo attribute really is the `False` singleton
        {% endif %}
    """
    return value is other


def test_iterable(value):
    """Check if it's possible to iterate over an object."""
    try:
        iter(value)
    except TypeError:
        return False
    return True


def test_escaped(value):
    """Check if the value is escaped."""
    return hasattr(value, "__html__")


def test_in(value, seq):
    """Check if value is in seq.

    .. versionadded:: 2.10
    """
    return value in seq


TESTS = {
    "odd": test_odd,
    "even": test_even,
    "divisibleby": test_divisibleby,
    "defined": test_defined,
    "undefined": test_undefined,
    "none": test_none,
    "boolean": test_boolean,
    "false": test_false,
    "true": test_true,
    "integer": test_integer,
    "float": test_float,
    "lower": test_lower,
    "upper": test_upper,
    "string": test_string,
    "mapping": test_mapping,
    "number": test_number,
    "sequence": test_sequence,
    "iterable": test_iterable,
    "callable": test_callable,
    "sameas": test_sameas,
    "escaped": test_escaped,
    "in": test_in,
    "==": operator.eq,
    "eq": operator.eq,
    "equalto": operator.eq,
    "!=": operator.ne,
    "ne": operator.ne,
    ">": operator.gt,
    "gt": operator.gt,
    "greaterthan": operator.gt,
    "ge": operator.ge,
    ">=": operator.ge,
    "<": operator.lt,
    "lt": operator.lt,
    "lessthan": operator.lt,
    "<=": operator.le,
    "le": operator.le,
}
