# -*- coding: utf-8 -*-
"""Built-in template filters used with the ``|`` operator."""
import math
import random
import re
import warnings
from collections import namedtuple
from itertools import chain
from itertools import groupby

from markupsafe import escape
from markupsafe import Markup
from markupsafe import soft_unicode

from ._compat import abc
from ._compat import imap
from ._compat import iteritems
from ._compat import string_types
from ._compat import text_type
from .exceptions import FilterArgumentError
from .runtime import Undefined
from .utils import htmlsafe_json_dumps
from .utils import pformat
from .utils import unicode_urlencode
from .utils import urlize

_word_re = re.compile(r"\w+", re.UNICODE)
_word_beginning_split_re = re.compile(r"([-\s\(\{\[\<]+)", re.UNICODE)


def contextfilter(f):
    """Decorator for marking context dependent filters. The current
    :class:`Context` will be passed as first argument.
    """
    f.contextfilter = True
    return f


def evalcontextfilter(f):
    """Decorator for marking eval-context dependent filters.  An eval
    context object is passed as first argument.  For more information
    about the eval context, see :ref:`eval-context`.

    .. versionadded:: 2.4
    """
    f.evalcontextfilter = True
    return f


def environmentfilter(f):
    """Decorator for marking environment dependent filters.  The current
    :class:`Environment` is passed to the filter as first argument.
    """
    f.environmentfilter = True
    return f


def ignore_case(value):
    """For use as a postprocessor for :func:`make_attrgetter`. Converts strings
    to lowercase and returns other types as-is."""
    return value.lower() if isinstance(value, string_types) else value


def make_attrgetter(environment, attribute, postprocess=None, default=None):
    """Returns a callable that looks up the given attribute from a
    passed object with the rules of the environment.  Dots are allowed
    to access attributes of attributes.  Integer parts in paths are
    looked up as integers.
    """
    attribute = _prepare_attribute_parts(attribute)

    def attrgetter(item):
        for part in attribute:
            item = environment.getitem(item, part)

            if default and isinstance(item, Undefined):
                item = default

        if postprocess is not None:
            item = postprocess(item)

        return item

    return attrgetter


def make_multi_attrgetter(environment, attribute, postprocess=None):
    """Returns a callable that looks up the given comma separated
    attributes from a passed object with the rules of the environment.
    Dots are allowed to access attributes of each attribute.  Integer
    parts in paths are looked up as integers.

    The value returned by the returned callable is a list of extracted
    attribute values.

    Examples of attribute: "attr1,attr2", "attr1.inner1.0,attr2.inner2.0", etc.
    """
    attribute_parts = (
        attribute.split(",") if isinstance(attribute, string_types) else [attribute]
    )
    attribute = [
        _prepare_attribute_parts(attribute_part) for attribute_part in attribute_parts
    ]

    def attrgetter(item):
        items = [None] * len(attribute)
        for i, attribute_part in enumerate(attribute):
            item_i = item
            for part in attribute_part:
                item_i = environment.getitem(item_i, part)

            if postprocess is not None:
                item_i = postprocess(item_i)

            items[i] = item_i
        return items

    return attrgetter


def _prepare_attribute_parts(attr):
    if attr is None:
        return []
    elif isinstance(attr, string_types):
        return [int(x) if x.isdigit() else x for x in attr.split(".")]
    else:
        return [attr]


def do_forceescape(value):
    """Enforce HTML escaping.  This will probably double escape variables."""
    if hasattr(value, "__html__"):
        value = value.__html__()
    return escape(text_type(value))


def do_urlencode(value):
    """Quote data for use in a URL path or query using UTF-8.

    Basic wrapper around :func:`urllib.parse.quote` when given a
    string, or :func:`urllib.parse.urlencode` for a dict or iterable.

    :param value: Data to quote. A string will be quoted directly. A
        dict or iterable of ``(key, value)`` pairs will be joined as a
        query string.

    When given a string, "/" is not quoted. HTTP servers treat "/" and
    "%2F" equivalently in paths. If you need quoted slashes, use the
    ``|replace("/", "%2F")`` filter.

    .. versionadded:: 2.7
    """
    if isinstance(value, string_types) or not isinstance(value, abc.Iterable):
        return unicode_urlencode(value)

    if isinstance(value, dict):
        items = iteritems(value)
    else:
        items = iter(value)

    return u"&".join(
        "%s=%s" % (unicode_urlencode(k, for_qs=True), unicode_urlencode(v, for_qs=True))
        for k, v in items
    )


@evalcontextfilter
def do_replace(eval_ctx, s, old, new, count=None):
    """Return a copy of the value with all occurrences of a substring
    replaced with a new one. The first argument is the substring
    that should be replaced, the second is the replacement string.
    If the optional third argument ``count`` is given, only the first
    ``count`` occurrences are replaced:

    .. sourcecode:: jinja

        {{ "Hello World"|replace("Hello", "Goodbye") }}
            -> Goodbye World

        {{ "aaaaargh"|replace("a", "d'oh, ", 2) }}
            -> d'oh, d'oh, aaargh
    """
    if count is None:
        count = -1
    if not eval_ctx.autoescape:
        return text_type(s).replace(text_type(old), text_type(new), count)
    if (
        hasattr(old, "__html__")
        or hasattr(new, "__html__")
        and not hasattr(s, "__html__")
    ):
        s = escape(s)
    else:
        s = soft_unicode(s)
    return s.replace(soft_unicode(old), soft_unicode(new), count)


def do_upper(s):
    """Convert a value to uppercase."""
    return soft_unicode(s).upper()


def do_lower(s):
    """Convert a value to lowercase."""
    return soft_unicode(s).lower()


@evalcontextfilter
def do_xmlattr(_eval_ctx, d, autospace=True):
    """Create an SGML/XML attribute string based on the items in a dict.
    All values that are neither `none` nor `undefined` are automatically
    escaped:

    .. sourcecode:: html+jinja

        <ul{{ {'class': 'my_list', 'missing': none,
                'id': 'list-%d'|format(variable)}|xmlattr }}>
        ...
        </ul>

    Results in something like this:

    .. sourcecode:: html

        <ul class="my_list" id="list-42">
        ...
        </ul>

    As you can see it automatically prepends a space in front of the item
    if the filter returned something unless the second parameter is false.
    """
    rv = u" ".join(
        u'%s="%s"' % (escape(key), escape(value))
        for key, value in iteritems(d)
        if value is not None and not isinstance(value, Undefined)
    )
    if autospace and rv:
        rv = u" " + rv
    if _eval_ctx.autoescape:
        rv = Markup(rv)
    return rv


def do_capitalize(s):
    """Capitalize a value. The first character will be uppercase, all others
    lowercase.
    """
    return soft_unicode(s).capitalize()


def do_title(s):
    """Return a titlecased version of the value. I.e. words will start with
    uppercase letters, all remaining characters are lowercase.
    """
    return "".join(
        [
            item[0].upper() + item[1:].lower()
            for item in _word_beginning_split_re.split(soft_unicode(s))
            if item
        ]
    )


def do_dictsort(value, case_sensitive=False, by="key", reverse=False):
    """Sort a dict and yield (key, value) pairs. Because python dicts are
    unsorted you may want to use this function to order them by either
    key or value:

    .. sourcecode:: jinja

        {% for key, value in mydict|dictsort %}
            sort the dict by key, case insensitive

        {% for key, value in mydict|dictsort(reverse=true) %}
            sort the dict by key, case insensitive, reverse order

        {% for key, value in mydict|dictsort(true) %}
            sort the dict by key, case sensitive

        {% for key, value in mydict|dictsort(false, 'value') %}
            sort the dict by value, case insensitive
    """
    if by == "key":
        pos = 0
    elif by == "value":
        pos = 1
    else:
        raise FilterArgumentError('You can only sort by either "key" or "value"')

    def sort_func(item):
        value = item[pos]

        if not case_sensitive:
            value = ignore_case(value)

        return value

    return sorted(value.items(), key=sort_func, reverse=reverse)


@environmentfilter
def do_sort(environment, value, reverse=False, case_sensitive=False, attribute=None):
    """Sort an iterable using Python's :func:`sorted`.

    .. sourcecode:: jinja

        {% for city in cities|sort %}
            ...
        {% endfor %}

    :param reverse: Sort descending instead of ascending.
    :param case_sensitive: When sorting strings, sort upper and lower
        case separately.
    :param attribute: When sorting objects or dicts, an attribute or
        key to sort by. Can use dot notation like ``"address.city"``.
        Can be a list of attributes like ``"age,name"``.

    The sort is stable, it does not change the relative order of
    elements that compare equal. This makes it is possible to chain
    sorts on different attributes and ordering.

    .. sourcecode:: jinja

        {% for user in users|sort(attribute="name")
            |sort(reverse=true, attribute="age") %}
            ...
        {% endfor %}

    As a shortcut to chaining when the direction is the same for all
    attributes, pass a comma separate list of attributes.

    .. sourcecode:: jinja

        {% for user users|sort(attribute="age,name") %}
            ...
        {% endfor %}

    .. versionchanged:: 2.11.0
        The ``attribute`` parameter can be a comma separated list of
        attributes, e.g. ``"age,name"``.

    .. versionchanged:: 2.6
       The ``attribute`` parameter was added.
    """
    key_func = make_multi_attrgetter(
        environment, attribute, postprocess=ignore_case if not case_sensitive else None
    )
    return sorted(value, key=key_func, reverse=reverse)


@environmentfilter
def do_unique(environment, value, case_sensitive=False, attribute=None):
    """Returns a list of unique items from the given iterable.

    .. sourcecode:: jinja

        {{ ['foo', 'bar', 'foobar', 'FooBar']|unique|list }}
            -> ['foo', 'bar', 'foobar']

    The unique items are yielded in the same order as their first occurrence in
    the iterable passed to the filter.

    :param case_sensitive: Treat upper and lower case strings as distinct.
    :param attribute: Filter objects with unique values for this attribute.
    """
    getter = make_attrgetter(
        environment, attribute, postprocess=ignore_case if not case_sensitive else None
    )
    seen = set()

    for item in value:
        key = getter(item)

        if key not in seen:
            seen.add(key)
            yield item


def _min_or_max(environment, value, func, case_sensitive, attribute):
    it = iter(value)

    try:
        first = next(it)
    except StopIteration:
        return environment.undefined("No aggregated item, sequence was empty.")

    key_func = make_attrgetter(
        environment, attribute, postprocess=ignore_case if not case_sensitive else None
    )
    return func(chain([first], it), key=key_func)


@environmentfilter
def do_min(environment, value, case_sensitive=False, attribute=None):
    """Return the smallest item from the sequence.

    .. sourcecode:: jinja

        {{ [1, 2, 3]|min }}
            -> 1

    :param case_sensitive: Treat upper and lower case strings as distinct.
    :param attribute: Get the object with the min value of this attribute.
    """
    return _min_or_max(environment, value, min, case_sensitive, attribute)


@environmentfilter
def do_max(environment, value, case_sensitive=False, attribute=None):
    """Return the largest item from the sequence.

    .. sourcecode:: jinja

        {{ [1, 2, 3]|max }}
            -> 3

    :param case_sensitive: Treat upper and lower case strings as distinct.
    :param attribute: Get the object with the max value of this attribute.
    """
    return _min_or_max(environment, value, max, case_sensitive, attribute)


def do_default(value, default_value=u"", boolean=False):
    """If the value is undefined it will return the passed default value,
    otherwise the value of the variable:

    .. sourcecode:: jinja

        {{ my_variable|default('my_variable is not defined') }}

    This will output the value of ``my_variable`` if the variable was
    defined, otherwise ``'my_variable is not defined'``. If you want
    to use default with variables that evaluate to false you have to
    set the second parameter to `true`:

    .. sourcecode:: jinja

        {{ ''|default('the string was empty', true) }}

    .. versionchanged:: 2.11
       It's now possible to configure the :class:`~jinja2.Environment` with
       :class:`~jinja2.ChainableUndefined` to make the `default` filter work
       on nested elements and attributes that may contain undefined values
       in the chain without getting an :exc:`~jinja2.UndefinedError`.
    """
    if isinstance(value, Undefined) or (boolean and not value):
        return default_value
    return value


@evalcontextfilter
def do_join(eval_ctx, value, d=u"", attribute=None):
    """Return a string which is the concatenation of the strings in the
    sequence. The separator between elements is an empty string per
    default, you can define it with the optional parameter:

    .. sourcecode:: jinja

        {{ [1, 2, 3]|join('|') }}
            -> 1|2|3

        {{ [1, 2, 3]|join }}
            -> 123

    It is also possible to join certain attributes of an object:

    .. sourcecode:: jinja

        {{ users|join(', ', attribute='username') }}

    .. versionadded:: 2.6
       The `attribute` parameter was added.
    """
    if attribute is not None:
        value = imap(make_attrgetter(eval_ctx.environment, attribute), value)

    # no automatic escaping?  joining is a lot easier then
    if not eval_ctx.autoescape:
        return text_type(d).join(imap(text_type, value))

    # if the delimiter doesn't have an html representation we check
    # if any of the items has.  If yes we do a coercion to Markup
    if not hasattr(d, "__html__"):
        value = list(value)
        do_escape = False
        for idx, item in enumerate(value):
            if hasattr(item, "__html__"):
                do_escape = True
            else:
                value[idx] = text_type(item)
        if do_escape:
            d = escape(d)
        else:
            d = text_type(d)
        return d.join(value)

    # no html involved, to normal joining
    return soft_unicode(d).join(imap(soft_unicode, value))


def do_center(value, width=80):
    """Centers the value in a field of a given width."""
    return text_type(value).center(width)


@environmentfilter
def do_first(environment, seq):
    """Return the first item of a sequence."""
    try:
        return next(iter(seq))
    except StopIteration:
        return environment.undefined("No first item, sequence was empty.")


@environmentfilter
def do_last(environment, seq):
    """
    Return the last item of a sequence.

    Note: Does not work with generators. You may want to explicitly
    convert it to a list:

    .. sourcecode:: jinja

        {{ data | selectattr('name', '==', 'Jinja') | list | last }}
    """
    try:
        return next(iter(reversed(seq)))
    except StopIteration:
        return environment.undefined("No last item, sequence was empty.")


@contextfilter
def do_random(context, seq):
    """Return a random item from the sequence."""
    try:
        return random.choice(seq)
    except IndexError:
        return context.environment.undefined("No random item, sequence was empty.")


def do_filesizeformat(value, binary=False):
    """Format the value like a 'human-readable' file size (i.e. 13 kB,
    4.1 MB, 102 Bytes, etc).  Per default decimal prefixes are used (Mega,
    Giga, etc.), if the second parameter is set to `True` the binary
    prefixes are used (Mebi, Gibi).
    """
    bytes = float(value)
    base = binary and 1024 or 1000
    prefixes = [
        (binary and "KiB" or "kB"),
        (binary and "MiB" or "MB"),
        (binary and "GiB" or "GB"),
        (binary and "TiB" or "TB"),
        (binary and "PiB" or "PB"),
        (binary and "EiB" or "EB"),
        (binary and "ZiB" or "ZB"),
        (binary and "YiB" or "YB"),
    ]
    if bytes == 1:
        return "1 Byte"
    elif bytes < base:
        return "%d Bytes" % bytes
    else:
        for i, prefix in enumerate(prefixes):
            unit = base ** (i + 2)
            if bytes < unit:
                return "%.1f %s" % ((base * bytes / unit), prefix)
        return "%.1f %s" % ((base * bytes / unit), prefix)


def do_pprint(value, verbose=False):
    """Pretty print a variable. Useful for debugging.

    With Jinja 1.2 onwards you can pass it a parameter.  If this parameter
    is truthy the output will be more verbose (this requires `pretty`)
    """
    return pformat(value, verbose=verbose)


@evalcontextfilter
def do_urlize(
    eval_ctx, value, trim_url_limit=None, nofollow=False, target=None, rel=None
):
    """Converts URLs in plain text into clickable links.

    If you pass the filter an additional integer it will shorten the urls
    to that number. Also a third argument exists that makes the urls
    "nofollow":

    .. sourcecode:: jinja

        {{ mytext|urlize(40, true) }}
            links are shortened to 40 chars and defined with rel="nofollow"

    If *target* is specified, the ``target`` attribute will be added to the
    ``<a>`` tag:

    .. sourcecode:: jinja

       {{ mytext|urlize(40, target='_blank') }}

    .. versionchanged:: 2.8+
       The *target* parameter was added.
    """
    policies = eval_ctx.environment.policies
    rel = set((rel or "").split() or [])
    if nofollow:
        rel.add("nofollow")
    rel.update((policies["urlize.rel"] or "").split())
    if target is None:
        target = policies["urlize.target"]
    rel = " ".join(sorted(rel)) or None
    rv = urlize(value, trim_url_limit, rel=rel, target=target)
    if eval_ctx.autoescape:
        rv = Markup(rv)
    return rv


def do_indent(s, width=4, first=False, blank=False, indentfirst=None):
    """Return a copy of the string with each line indented by 4 spaces. The
    first line and blank lines are not indented by default.

    :param width: Number of spaces to indent by.
    :param first: Don't skip indenting the first line.
    :param blank: Don't skip indenting empty lines.

    .. versionchanged:: 2.10
        Blank lines are not indented by default.

        Rename the ``indentfirst`` argument to ``first``.
    """
    if indentfirst is not None:
        warnings.warn(
            "The 'indentfirst' argument is renamed to 'first' and will"
            " be removed in version 3.0.",
            DeprecationWarning,
            stacklevel=2,
        )
        first = indentfirst

    indention = u" " * width
    newline = u"\n"

    if isinstance(s, Markup):
        indention = Markup(indention)
        newline = Markup(newline)

    s += newline  # this quirk is necessary for splitlines method

    if blank:
        rv = (newline + indention).join(s.splitlines())
    else:
        lines = s.splitlines()
        rv = lines.pop(0)

        if lines:
            rv += newline + newline.join(
                indention + line if line else line for line in lines
            )

    if first:
        rv = indention + rv

    return rv


@environmentfilter
def do_truncate(env, s, length=255, killwords=False, end="...", leeway=None):
    """Return a truncated copy of the string. The length is specified
    with the first parameter which defaults to ``255``. If the second
    parameter is ``true`` the filter will cut the text at length. Otherwise
    it will discard the last word. If the text was in fact
    truncated it will append an ellipsis sign (``"..."``). If you want a
    different ellipsis sign than ``"..."`` you can specify it using the
    third parameter. Strings that only exceed the length by the tolerance
    margin given in the fourth parameter will not be truncated.

    .. sourcecode:: jinja

        {{ "foo bar baz qux"|truncate(9) }}
            -> "foo..."
        {{ "foo bar baz qux"|truncate(9, True) }}
            -> "foo ba..."
        {{ "foo bar baz qux"|truncate(11) }}
            -> "foo bar baz qux"
        {{ "foo bar baz qux"|truncate(11, False, '...', 0) }}
            -> "foo bar..."

    The default leeway on newer Jinja versions is 5 and was 0 before but
    can be reconfigured globally.
    """
    if leeway is None:
        leeway = env.policies["truncate.leeway"]
    assert length >= len(end), "expected length >= %s, got %s" % (len(end), length)
    assert leeway >= 0, "expected leeway >= 0, got %s" % leeway
    if len(s) <= length + leeway:
        return s
    if killwords:
        return s[: length - len(end)] + end
    result = s[: length - len(end)].rsplit(" ", 1)[0]
    return result + end


@environmentfilter
def do_wordwrap(
    environment,
    s,
    width=79,
    break_long_words=True,
    wrapstring=None,
    break_on_hyphens=True,
):
    """Wrap a string to the given width. Existing newlines are treated
    as paragraphs to be wrapped separately.

    :param s: Original text to wrap.
    :param width: Maximum length of wrapped lines.
    :param break_long_words: If a word is longer than ``width``, break
        it across lines.
    :param break_on_hyphens: If a word contains hyphens, it may be split
        across lines.
    :param wrapstring: String to join each wrapped line. Defaults to
        :attr:`Environment.newline_sequence`.

    .. versionchanged:: 2.11
        Existing newlines are treated as paragraphs wrapped separately.

    .. versionchanged:: 2.11
        Added the ``break_on_hyphens`` parameter.

    .. versionchanged:: 2.7
        Added the ``wrapstring`` parameter.
    """

    import textwrap

    if not wrapstring:
        wrapstring = environment.newline_sequence

    # textwrap.wrap doesn't consider existing newlines when wrapping.
    # If the string has a newline before width, wrap will still insert
    # a newline at width, resulting in a short line. Instead, split and
    # wrap each paragraph individually.
    return wrapstring.join(
        [
            wrapstring.join(
                textwrap.wrap(
                    line,
                    width=width,
                    expand_tabs=False,
                    replace_whitespace=False,
                    break_long_words=break_long_words,
                    break_on_hyphens=break_on_hyphens,
                )
            )
            for line in s.splitlines()
        ]
    )


def do_wordcount(s):
    """Count the words in that string."""
    return len(_word_re.findall(soft_unicode(s)))


def do_int(value, default=0, base=10):
    """Convert the value into an integer. If the
    conversion doesn't work it will return ``0``. You can
    override this default using the first parameter. You
    can also override the default base (10) in the second
    parameter, which handles input with prefixes such as
    0b, 0o and 0x for bases 2, 8 and 16 respectively.
    The base is ignored for decimal numbers and non-string values.
    """
    try:
        if isinstance(value, string_types):
            return int(value, base)
        return int(value)
    except (TypeError, ValueError):
        # this quirk is necessary so that "42.23"|int gives 42.
        try:
            return int(float(value))
        except (TypeError, ValueError):
            return default


def do_float(value, default=0.0):
    """Convert the value into a floating point number. If the
    conversion doesn't work it will return ``0.0``. You can
    override this default using the first parameter.
    """
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def do_format(value, *args, **kwargs):
    """Apply the given values to a `printf-style`_ format string, like
    ``string % values``.

    .. sourcecode:: jinja

        {{ "%s, %s!"|format(greeting, name) }}
        Hello, World!

    In most cases it should be more convenient and efficient to use the
    ``%`` operator or :meth:`str.format`.

    .. code-block:: text

        {{ "%s, %s!" % (greeting, name) }}
        {{ "{}, {}!".format(greeting, name) }}

    .. _printf-style: https://docs.python.org/library/stdtypes.html
        #printf-style-string-formatting
    """
    if args and kwargs:
        raise FilterArgumentError(
            "can't handle positional and keyword arguments at the same time"
        )
    return soft_unicode(value) % (kwargs or args)


def do_trim(value, chars=None):
    """Strip leading and trailing characters, by default whitespace."""
    return soft_unicode(value).strip(chars)


def do_striptags(value):
    """Strip SGML/XML tags and replace adjacent whitespace by one space."""
    if hasattr(value, "__html__"):
        value = value.__html__()
    return Markup(text_type(value)).striptags()


def do_slice(value, slices, fill_with=None):
    """Slice an iterator and return a list of lists containing
    those items. Useful if you want to create a div containing
    three ul tags that represent columns:

    .. sourcecode:: html+jinja

        <div class="columnwrapper">
          {%- for column in items|slice(3) %}
            <ul class="column-{{ loop.index }}">
            {%- for item in column %}
              <li>{{ item }}</li>
            {%- endfor %}
            </ul>
          {%- endfor %}
        </div>

    If you pass it a second argument it's used to fill missing
    values on the last iteration.
    """
    seq = list(value)
    length = len(seq)
    items_per_slice = length // slices
    slices_with_extra = length % slices
    offset = 0
    for slice_number in range(slices):
        start = offset + slice_number * items_per_slice
        if slice_number < slices_with_extra:
            offset += 1
        end = offset + (slice_number + 1) * items_per_slice
        tmp = seq[start:end]
        if fill_with is not None and slice_number >= slices_with_extra:
            tmp.append(fill_with)
        yield tmp


def do_batch(value, linecount, fill_with=None):
    """
    A filter that batches items. It works pretty much like `slice`
    just the other way round. It returns a list of lists with the
    given number of items. If you provide a second parameter this
    is used to fill up missing items. See this example:

    .. sourcecode:: html+jinja

        <table>
        {%- for row in items|batch(3, '&nbsp;') %}
          <tr>
          {%- for column in row %}
            <td>{{ column }}</td>
          {%- endfor %}
          </tr>
        {%- endfor %}
        </table>
    """
    tmp = []
    for item in value:
        if len(tmp) == linecount:
            yield tmp
            tmp = []
        tmp.append(item)
    if tmp:
        if fill_with is not None and len(tmp) < linecount:
            tmp += [fill_with] * (linecount - len(tmp))
        yield tmp


def do_round(value, precision=0, method="common"):
    """Round the number to a given precision. The first
    parameter specifies the precision (default is ``0``), the
    second the rounding method:

    - ``'common'`` rounds either up or down
    - ``'ceil'`` always rounds up
    - ``'floor'`` always rounds down

    If you don't specify a method ``'common'`` is used.

    .. sourcecode:: jinja

        {{ 42.55|round }}
            -> 43.0
        {{ 42.55|round(1, 'floor') }}
            -> 42.5

    Note that even if rounded to 0 precision, a float is returned.  If
    you need a real integer, pipe it through `int`:

    .. sourcecode:: jinja

        {{ 42.55|round|int }}
            -> 43
    """
    if method not in {"common", "ceil", "floor"}:
        raise FilterArgumentError("method must be common, ceil or floor")
    if method == "common":
        return round(value, precision)
    func = getattr(math, method)
    return func(value * (10 ** precision)) / (10 ** precision)


# Use a regular tuple repr here.  This is what we did in the past and we
# really want to hide this custom type as much as possible.  In particular
# we do not want to accidentally expose an auto generated repr in case
# people start to print this out in comments or something similar for
# debugging.
_GroupTuple = namedtuple("_GroupTuple", ["grouper", "list"])
_GroupTuple.__repr__ = tuple.__repr__
_GroupTuple.__str__ = tuple.__str__


@environmentfilter
def do_groupby(environment, value, attribute):
    """Group a sequence of objects by an attribute using Python's
    :func:`itertools.groupby`. The attribute can use dot notation for
    nested access, like ``"address.city"``. Unlike Python's ``groupby``,
    the values are sorted first so only one group is returned for each
    unique value.

    For example, a list of ``User`` objects with a ``city`` attribute
    can be rendered in groups. In this example, ``grouper`` refers to
    the ``city`` value of the group.

    .. sourcecode:: html+jinja

        <ul>{% for city, items in users|groupby("city") %}
          <li>{{ city }}
            <ul>{% for user in items %}
              <li>{{ user.name }}
            {% endfor %}</ul>
          </li>
        {% endfor %}</ul>

    ``groupby`` yields namedtuples of ``(grouper, list)``, which
    can be used instead of the tuple unpacking above. ``grouper`` is the
    value of the attribute, and ``list`` is the items with that value.

    .. sourcecode:: html+jinja

        <ul>{% for group in users|groupby("city") %}
          <li>{{ group.grouper }}: {{ group.list|join(", ") }}
        {% endfor %}</ul>

    .. versionchanged:: 2.6
        The attribute supports dot notation for nested access.
    """
    expr = make_attrgetter(environment, attribute)
    return [
        _GroupTuple(key, list(values))
        for key, values in groupby(sorted(value, key=expr), expr)
    ]


@environmentfilter
def do_sum(environment, iterable, attribute=None, start=0):
    """Returns the sum of a sequence of numbers plus the value of parameter
    'start' (which defaults to 0).  When the sequence is empty it returns
    start.

    It is also possible to sum up only certain attributes:

    .. sourcecode:: jinja

        Total: {{ items|sum(attribute='price') }}

    .. versionchanged:: 2.6
       The `attribute` parameter was added to allow suming up over
       attributes.  Also the `start` parameter was moved on to the right.
    """
    if attribute is not None:
        iterable = imap(make_attrgetter(environment, attribute), iterable)
    return sum(iterable, start)


def do_list(value):
    """Convert the value into a list.  If it was a string the returned list
    will be a list of characters.
    """
    return list(value)


def do_mark_safe(value):
    """Mark the value as safe which means that in an environment with automatic
    escaping enabled this variable will not be escaped.
    """
    return Markup(value)


def do_mark_unsafe(value):
    """Mark a value as unsafe.  This is the reverse operation for :func:`safe`."""
    return text_type(value)


def do_reverse(value):
    """Reverse the object or return an iterator that iterates over it the other
    way round.
    """
    if isinstance(value, string_types):
        return value[::-1]
    try:
        return reversed(value)
    except TypeError:
        try:
            rv = list(value)
            rv.reverse()
            return rv
        except TypeError:
            raise FilterArgumentError("argument must be iterable")


@environmentfilter
def do_attr(environment, obj, name):
    """Get an attribute of an object.  ``foo|attr("bar")`` works like
    ``foo.bar`` just that always an attribute is returned and items are not
    looked up.

    See :ref:`Notes on subscriptions <notes-on-subscriptions>` for more details.
    """
    try:
        name = str(name)
    except UnicodeError:
        pass
    else:
        try:
            value = getattr(obj, name)
        except AttributeError:
            pass
        else:
            if environment.sandboxed and not environment.is_safe_attribute(
                obj, name, value
            ):
                return environment.unsafe_undefined(obj, name)
            return value
    return environment.undefined(obj=obj, name=name)


@contextfilter
def do_map(*args, **kwargs):
    """Applies a filter on a sequence of objects or looks up an attribute.
    This is useful when dealing with lists of objects but you are really
    only interested in a certain value of it.

    The basic usage is mapping on an attribute.  Imagine you have a list
    of users but you are only interested in a list of usernames:

    .. sourcecode:: jinja

        Users on this page: {{ users|map(attribute='username')|join(', ') }}

    You can specify a ``default`` value to use if an object in the list
    does not have the given attribute.

    .. sourcecode:: jinja

        {{ users|map(attribute="username", default="Anonymous")|join(", ") }}

    Alternatively you can let it invoke a filter by passing the name of the
    filter and the arguments afterwards.  A good example would be applying a
    text conversion filter on a sequence:

    .. sourcecode:: jinja

        Users on this page: {{ titles|map('lower')|join(', ') }}

    Similar to a generator comprehension such as:

    .. code-block:: python

        (u.username for u in users)
        (u.username or "Anonymous" for u in users)
        (do_lower(x) for x in titles)

    .. versionchanged:: 2.11.0
        Added the ``default`` parameter.

    .. versionadded:: 2.7
    """
    seq, func = prepare_map(args, kwargs)
    if seq:
        for item in seq:
            yield func(item)


@contextfilter
def do_select(*args, **kwargs):
    """Filters a sequence of objects by applying a test to each object,
    and only selecting the objects with the test succeeding.

    If no test is specified, each object will be evaluated as a boolean.

    Example usage:

    .. sourcecode:: jinja

        {{ numbers|select("odd") }}
        {{ numbers|select("odd") }}
        {{ numbers|select("divisibleby", 3) }}
        {{ numbers|select("lessthan", 42) }}
        {{ strings|select("equalto", "mystring") }}

    Similar to a generator comprehension such as:

    .. code-block:: python

        (n for n in numbers if test_odd(n))
        (n for n in numbers if test_divisibleby(n, 3))

    .. versionadded:: 2.7
    """
    return select_or_reject(args, kwargs, lambda x: x, False)


@contextfilter
def do_reject(*args, **kwargs):
    """Filters a sequence of objects by applying a test to each object,
    and rejecting the objects with the test succeeding.

    If no test is specified, each object will be evaluated as a boolean.

    Example usage:

    .. sourcecode:: jinja

        {{ numbers|reject("odd") }}

    Similar to a generator comprehension such as:

    .. code-block:: python

        (n for n in numbers if not test_odd(n))

    .. versionadded:: 2.7
    """
    return select_or_reject(args, kwargs, lambda x: not x, False)


@contextfilter
def do_selectattr(*args, **kwargs):
    """Filters a sequence of objects by applying a test to the specified
    attribute of each object, and only selecting the objects with the
    test succeeding.

    If no test is specified, the attribute's value will be evaluated as
    a boolean.

    Example usage:

    .. sourcecode:: jinja

        {{ users|selectattr("is_active") }}
        {{ users|selectattr("email", "none") }}

    Similar to a generator comprehension such as:

    .. code-block:: python

        (u for user in users if user.is_active)
        (u for user in users if test_none(user.email))

    .. versionadded:: 2.7
    """
    return select_or_reject(args, kwargs, lambda x: x, True)


@contextfilter
def do_rejectattr(*args, **kwargs):
    """Filters a sequence of objects by applying a test to the specified
    attribute of each object, and rejecting the objects with the test
    succeeding.

    If no test is specified, the attribute's value will be evaluated as
    a boolean.

    .. sourcecode:: jinja

        {{ users|rejectattr("is_active") }}
        {{ users|rejectattr("email", "none") }}

    Similar to a generator comprehension such as:

    .. code-block:: python

        (u for user in users if not user.is_active)
        (u for user in users if not test_none(user.email))

    .. versionadded:: 2.7
    """
    return select_or_reject(args, kwargs, lambda x: not x, True)


@evalcontextfilter
def do_tojson(eval_ctx, value, indent=None):
    """Dumps a structure to JSON so that it's safe to use in ``<script>``
    tags.  It accepts the same arguments and returns a JSON string.  Note that
    this is available in templates through the ``|tojson`` filter which will
    also mark the result as safe.  Due to how this function escapes certain
    characters this is safe even if used outside of ``<script>`` tags.

    The following characters are escaped in strings:

    -   ``<``
    -   ``>``
    -   ``&``
    -   ``'``

    This makes it safe to embed such strings in any place in HTML with the
    notable exception of double quoted attributes.  In that case single
    quote your attributes or HTML escape it in addition.

    The indent parameter can be used to enable pretty printing.  Set it to
    the number of spaces that the structures should be indented with.

    Note that this filter is for use in HTML contexts only.

    .. versionadded:: 2.9
    """
    policies = eval_ctx.environment.policies
    dumper = policies["json.dumps_function"]
    options = policies["json.dumps_kwargs"]
    if indent is not None:
        options = dict(options)
        options["indent"] = indent
    return htmlsafe_json_dumps(value, dumper=dumper, **options)


def prepare_map(args, kwargs):
    context = args[0]
    seq = args[1]
    default = None

    if len(args) == 2 and "attribute" in kwargs:
        attribute = kwargs.pop("attribute")
        default = kwargs.pop("default", None)
        if kwargs:
            raise FilterArgumentError(
                "Unexpected keyword argument %r" % next(iter(kwargs))
            )
        func = make_attrgetter(context.environment, attribute, default=default)
    else:
        try:
            name = args[2]
            args = args[3:]
        except LookupError:
            raise FilterArgumentError("map requires a filter argument")

        def func(item):
            return context.environment.call_filter(
                name, item, args, kwargs, context=context
            )

    return seq, func


def prepare_select_or_reject(args, kwargs, modfunc, lookup_attr):
    context = args[0]
    seq = args[1]
    if lookup_attr:
        try:
            attr = args[2]
        except LookupError:
            raise FilterArgumentError("Missing parameter for attribute name")
        transfunc = make_attrgetter(context.environment, attr)
        off = 1
    else:
        off = 0

        def transfunc(x):
            return x

    try:
        name = args[2 + off]
        args = args[3 + off :]

        def func(item):
            return context.environment.call_test(name, item, args, kwargs)

    except LookupError:
        func = bool

    return seq, lambda item: modfunc(func(transfunc(item)))


def select_or_reject(args, kwargs, modfunc, lookup_attr):
    seq, func = prepare_select_or_reject(args, kwargs, modfunc, lookup_attr)
    if seq:
        for item in seq:
            if func(item):
                yield item


FILTERS = {
    "abs": abs,
    "attr": do_attr,
    "batch": do_batch,
    "capitalize": do_capitalize,
    "center": do_center,
    "count": len,
    "d": do_default,
    "default": do_default,
    "dictsort": do_dictsort,
    "e": escape,
    "escape": escape,
    "filesizeformat": do_filesizeformat,
    "first": do_first,
    "float": do_float,
    "forceescape": do_forceescape,
    "format": do_format,
    "groupby": do_groupby,
    "indent": do_indent,
    "int": do_int,
    "join": do_join,
    "last": do_last,
    "length": len,
    "list": do_list,
    "lower": do_lower,
    "map": do_map,
    "min": do_min,
    "max": do_max,
    "pprint": do_pprint,
    "random": do_random,
    "reject": do_reject,
    "rejectattr": do_rejectattr,
    "replace": do_replace,
    "reverse": do_reverse,
    "round": do_round,
    "safe": do_mark_safe,
    "select": do_select,
    "selectattr": do_selectattr,
    "slice": do_slice,
    "sort": do_sort,
    "string": soft_unicode,
    "striptags": do_striptags,
    "sum": do_sum,
    "title": do_title,
    "trim": do_trim,
    "truncate": do_truncate,
    "unique": do_unique,
    "upper": do_upper,
    "urlencode": do_urlencode,
    "urlize": do_urlize,
    "wordcount": do_wordcount,
    "wordwrap": do_wordwrap,
    "xmlattr": do_xmlattr,
    "tojson": do_tojson,
}
