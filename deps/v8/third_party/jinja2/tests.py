"""Built-in template tests used with the ``is`` operator."""
import operator
import typing as t
from collections import abc
from numbers import Number

from .runtime import Undefined
from .utils import pass_environment

if t.TYPE_CHECKING:
    from .environment import Environment


def test_odd(value: int) -> bool:
    """Return true if the variable is odd."""
    return value % 2 == 1


def test_even(value: int) -> bool:
    """Return true if the variable is even."""
    return value % 2 == 0


def test_divisibleby(value: int, num: int) -> bool:
    """Check if a variable is divisible by a number."""
    return value % num == 0


def test_defined(value: t.Any) -> bool:
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


def test_undefined(value: t.Any) -> bool:
    """Like :func:`defined` but the other way round."""
    return isinstance(value, Undefined)


@pass_environment
def test_filter(env: "Environment", value: str) -> bool:
    """Check if a filter exists by name. Useful if a filter may be
    optionally available.

    .. code-block:: jinja

        {% if 'markdown' is filter %}
            {{ value | markdown }}
        {% else %}
            {{ value }}
        {% endif %}

    .. versionadded:: 3.0
    """
    return value in env.filters


@pass_environment
def test_test(env: "Environment", value: str) -> bool:
    """Check if a test exists by name. Useful if a test may be
    optionally available.

    .. code-block:: jinja

        {% if 'loud' is test %}
            {% if value is loud %}
                {{ value|upper }}
            {% else %}
                {{ value|lower }}
            {% endif %}
        {% else %}
            {{ value }}
        {% endif %}

    .. versionadded:: 3.0
    """
    return value in env.tests


def test_none(value: t.Any) -> bool:
    """Return true if the variable is none."""
    return value is None


def test_boolean(value: t.Any) -> bool:
    """Return true if the object is a boolean value.

    .. versionadded:: 2.11
    """
    return value is True or value is False


def test_false(value: t.Any) -> bool:
    """Return true if the object is False.

    .. versionadded:: 2.11
    """
    return value is False


def test_true(value: t.Any) -> bool:
    """Return true if the object is True.

    .. versionadded:: 2.11
    """
    return value is True


# NOTE: The existing 'number' test matches booleans and floats
def test_integer(value: t.Any) -> bool:
    """Return true if the object is an integer.

    .. versionadded:: 2.11
    """
    return isinstance(value, int) and value is not True and value is not False


# NOTE: The existing 'number' test matches booleans and integers
def test_float(value: t.Any) -> bool:
    """Return true if the object is a float.

    .. versionadded:: 2.11
    """
    return isinstance(value, float)


def test_lower(value: str) -> bool:
    """Return true if the variable is lowercased."""
    return str(value).islower()


def test_upper(value: str) -> bool:
    """Return true if the variable is uppercased."""
    return str(value).isupper()


def test_string(value: t.Any) -> bool:
    """Return true if the object is a string."""
    return isinstance(value, str)


def test_mapping(value: t.Any) -> bool:
    """Return true if the object is a mapping (dict etc.).

    .. versionadded:: 2.6
    """
    return isinstance(value, abc.Mapping)


def test_number(value: t.Any) -> bool:
    """Return true if the variable is a number."""
    return isinstance(value, Number)


def test_sequence(value: t.Any) -> bool:
    """Return true if the variable is a sequence. Sequences are variables
    that are iterable.
    """
    try:
        len(value)
        value.__getitem__
    except Exception:
        return False

    return True


def test_sameas(value: t.Any, other: t.Any) -> bool:
    """Check if an object points to the same memory address than another
    object:

    .. sourcecode:: jinja

        {% if foo.attribute is sameas false %}
            the foo attribute really is the `False` singleton
        {% endif %}
    """
    return value is other


def test_iterable(value: t.Any) -> bool:
    """Check if it's possible to iterate over an object."""
    try:
        iter(value)
    except TypeError:
        return False

    return True


def test_escaped(value: t.Any) -> bool:
    """Check if the value is escaped."""
    return hasattr(value, "__html__")


def test_in(value: t.Any, seq: t.Container) -> bool:
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
    "filter": test_filter,
    "test": test_test,
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
    "callable": callable,
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
