# -*- coding: utf-8 -*-
import json
import os
import re
import warnings
from collections import deque
from random import choice
from random import randrange
from string import ascii_letters as _letters
from string import digits as _digits
from threading import Lock

from markupsafe import escape
from markupsafe import Markup

from ._compat import abc
from ._compat import string_types
from ._compat import text_type
from ._compat import url_quote

# special singleton representing missing values for the runtime
missing = type("MissingType", (), {"__repr__": lambda x: "missing"})()

# internal code
internal_code = set()

concat = u"".join

_slash_escape = "\\/" not in json.dumps("/")


def contextfunction(f):
    """This decorator can be used to mark a function or method context callable.
    A context callable is passed the active :class:`Context` as first argument when
    called from the template.  This is useful if a function wants to get access
    to the context or functions provided on the context object.  For example
    a function that returns a sorted list of template variables the current
    template exports could look like this::

        @contextfunction
        def get_exported_names(context):
            return sorted(context.exported_vars)
    """
    f.contextfunction = True
    return f


def evalcontextfunction(f):
    """This decorator can be used to mark a function or method as an eval
    context callable.  This is similar to the :func:`contextfunction`
    but instead of passing the context, an evaluation context object is
    passed.  For more information about the eval context, see
    :ref:`eval-context`.

    .. versionadded:: 2.4
    """
    f.evalcontextfunction = True
    return f


def environmentfunction(f):
    """This decorator can be used to mark a function or method as environment
    callable.  This decorator works exactly like the :func:`contextfunction`
    decorator just that the first argument is the active :class:`Environment`
    and not context.
    """
    f.environmentfunction = True
    return f


def internalcode(f):
    """Marks the function as internally used"""
    internal_code.add(f.__code__)
    return f


def is_undefined(obj):
    """Check if the object passed is undefined.  This does nothing more than
    performing an instance check against :class:`Undefined` but looks nicer.
    This can be used for custom filters or tests that want to react to
    undefined variables.  For example a custom default filter can look like
    this::

        def default(var, default=''):
            if is_undefined(var):
                return default
            return var
    """
    from .runtime import Undefined

    return isinstance(obj, Undefined)


def consume(iterable):
    """Consumes an iterable without doing anything with it."""
    for _ in iterable:
        pass


def clear_caches():
    """Jinja keeps internal caches for environments and lexers.  These are
    used so that Jinja doesn't have to recreate environments and lexers all
    the time.  Normally you don't have to care about that but if you are
    measuring memory consumption you may want to clean the caches.
    """
    from .environment import _spontaneous_environments
    from .lexer import _lexer_cache

    _spontaneous_environments.clear()
    _lexer_cache.clear()


def import_string(import_name, silent=False):
    """Imports an object based on a string.  This is useful if you want to
    use import paths as endpoints or something similar.  An import path can
    be specified either in dotted notation (``xml.sax.saxutils.escape``)
    or with a colon as object delimiter (``xml.sax.saxutils:escape``).

    If the `silent` is True the return value will be `None` if the import
    fails.

    :return: imported object
    """
    try:
        if ":" in import_name:
            module, obj = import_name.split(":", 1)
        elif "." in import_name:
            module, _, obj = import_name.rpartition(".")
        else:
            return __import__(import_name)
        return getattr(__import__(module, None, None, [obj]), obj)
    except (ImportError, AttributeError):
        if not silent:
            raise


def open_if_exists(filename, mode="rb"):
    """Returns a file descriptor for the filename if that file exists,
    otherwise ``None``.
    """
    if not os.path.isfile(filename):
        return None

    return open(filename, mode)


def object_type_repr(obj):
    """Returns the name of the object's type.  For some recognized
    singletons the name of the object is returned instead. (For
    example for `None` and `Ellipsis`).
    """
    if obj is None:
        return "None"
    elif obj is Ellipsis:
        return "Ellipsis"

    cls = type(obj)

    # __builtin__ in 2.x, builtins in 3.x
    if cls.__module__ in ("__builtin__", "builtins"):
        name = cls.__name__
    else:
        name = cls.__module__ + "." + cls.__name__

    return "%s object" % name


def pformat(obj, verbose=False):
    """Prettyprint an object.  Either use the `pretty` library or the
    builtin `pprint`.
    """
    try:
        from pretty import pretty

        return pretty(obj, verbose=verbose)
    except ImportError:
        from pprint import pformat

        return pformat(obj)


def urlize(text, trim_url_limit=None, rel=None, target=None):
    """Converts any URLs in text into clickable links. Works on http://,
    https:// and www. links. Links can have trailing punctuation (periods,
    commas, close-parens) and leading punctuation (opening parens) and
    it'll still do the right thing.

    If trim_url_limit is not None, the URLs in link text will be limited
    to trim_url_limit characters.

    If nofollow is True, the URLs in link text will get a rel="nofollow"
    attribute.

    If target is not None, a target attribute will be added to the link.
    """
    trim_url = (
        lambda x, limit=trim_url_limit: limit is not None
        and (x[:limit] + (len(x) >= limit and "..." or ""))
        or x
    )
    words = re.split(r"(\s+)", text_type(escape(text)))
    rel_attr = rel and ' rel="%s"' % text_type(escape(rel)) or ""
    target_attr = target and ' target="%s"' % escape(target) or ""

    for i, word in enumerate(words):
        head, middle, tail = "", word, ""
        match = re.match(r"^([(<]|&lt;)+", middle)

        if match:
            head = match.group()
            middle = middle[match.end() :]

        # Unlike lead, which is anchored to the start of the string,
        # need to check that the string ends with any of the characters
        # before trying to match all of them, to avoid backtracking.
        if middle.endswith((")", ">", ".", ",", "\n", "&gt;")):
            match = re.search(r"([)>.,\n]|&gt;)+$", middle)

            if match:
                tail = match.group()
                middle = middle[: match.start()]

        if middle.startswith("www.") or (
            "@" not in middle
            and not middle.startswith("http://")
            and not middle.startswith("https://")
            and len(middle) > 0
            and middle[0] in _letters + _digits
            and (
                middle.endswith(".org")
                or middle.endswith(".net")
                or middle.endswith(".com")
            )
        ):
            middle = '<a href="http://%s"%s%s>%s</a>' % (
                middle,
                rel_attr,
                target_attr,
                trim_url(middle),
            )

        if middle.startswith("http://") or middle.startswith("https://"):
            middle = '<a href="%s"%s%s>%s</a>' % (
                middle,
                rel_attr,
                target_attr,
                trim_url(middle),
            )

        if (
            "@" in middle
            and not middle.startswith("www.")
            and ":" not in middle
            and re.match(r"^\S+@\w[\w.-]*\.\w+$", middle)
        ):
            middle = '<a href="mailto:%s">%s</a>' % (middle, middle)

        words[i] = head + middle + tail

    return u"".join(words)


def generate_lorem_ipsum(n=5, html=True, min=20, max=100):
    """Generate some lorem ipsum for the template."""
    from .constants import LOREM_IPSUM_WORDS

    words = LOREM_IPSUM_WORDS.split()
    result = []

    for _ in range(n):
        next_capitalized = True
        last_comma = last_fullstop = 0
        word = None
        last = None
        p = []

        # each paragraph contains out of 20 to 100 words.
        for idx, _ in enumerate(range(randrange(min, max))):
            while True:
                word = choice(words)
                if word != last:
                    last = word
                    break
            if next_capitalized:
                word = word.capitalize()
                next_capitalized = False
            # add commas
            if idx - randrange(3, 8) > last_comma:
                last_comma = idx
                last_fullstop += 2
                word += ","
            # add end of sentences
            if idx - randrange(10, 20) > last_fullstop:
                last_comma = last_fullstop = idx
                word += "."
                next_capitalized = True
            p.append(word)

        # ensure that the paragraph ends with a dot.
        p = u" ".join(p)
        if p.endswith(","):
            p = p[:-1] + "."
        elif not p.endswith("."):
            p += "."
        result.append(p)

    if not html:
        return u"\n\n".join(result)
    return Markup(u"\n".join(u"<p>%s</p>" % escape(x) for x in result))


def unicode_urlencode(obj, charset="utf-8", for_qs=False):
    """Quote a string for use in a URL using the given charset.

    This function is misnamed, it is a wrapper around
    :func:`urllib.parse.quote`.

    :param obj: String or bytes to quote. Other types are converted to
        string then encoded to bytes using the given charset.
    :param charset: Encode text to bytes using this charset.
    :param for_qs: Quote "/" and use "+" for spaces.
    """
    if not isinstance(obj, string_types):
        obj = text_type(obj)

    if isinstance(obj, text_type):
        obj = obj.encode(charset)

    safe = b"" if for_qs else b"/"
    rv = url_quote(obj, safe)

    if not isinstance(rv, text_type):
        rv = rv.decode("utf-8")

    if for_qs:
        rv = rv.replace("%20", "+")

    return rv


class LRUCache(object):
    """A simple LRU Cache implementation."""

    # this is fast for small capacities (something below 1000) but doesn't
    # scale.  But as long as it's only used as storage for templates this
    # won't do any harm.

    def __init__(self, capacity):
        self.capacity = capacity
        self._mapping = {}
        self._queue = deque()
        self._postinit()

    def _postinit(self):
        # alias all queue methods for faster lookup
        self._popleft = self._queue.popleft
        self._pop = self._queue.pop
        self._remove = self._queue.remove
        self._wlock = Lock()
        self._append = self._queue.append

    def __getstate__(self):
        return {
            "capacity": self.capacity,
            "_mapping": self._mapping,
            "_queue": self._queue,
        }

    def __setstate__(self, d):
        self.__dict__.update(d)
        self._postinit()

    def __getnewargs__(self):
        return (self.capacity,)

    def copy(self):
        """Return a shallow copy of the instance."""
        rv = self.__class__(self.capacity)
        rv._mapping.update(self._mapping)
        rv._queue.extend(self._queue)
        return rv

    def get(self, key, default=None):
        """Return an item from the cache dict or `default`"""
        try:
            return self[key]
        except KeyError:
            return default

    def setdefault(self, key, default=None):
        """Set `default` if the key is not in the cache otherwise
        leave unchanged. Return the value of this key.
        """
        try:
            return self[key]
        except KeyError:
            self[key] = default
            return default

    def clear(self):
        """Clear the cache."""
        self._wlock.acquire()
        try:
            self._mapping.clear()
            self._queue.clear()
        finally:
            self._wlock.release()

    def __contains__(self, key):
        """Check if a key exists in this cache."""
        return key in self._mapping

    def __len__(self):
        """Return the current size of the cache."""
        return len(self._mapping)

    def __repr__(self):
        return "<%s %r>" % (self.__class__.__name__, self._mapping)

    def __getitem__(self, key):
        """Get an item from the cache. Moves the item up so that it has the
        highest priority then.

        Raise a `KeyError` if it does not exist.
        """
        self._wlock.acquire()
        try:
            rv = self._mapping[key]
            if self._queue[-1] != key:
                try:
                    self._remove(key)
                except ValueError:
                    # if something removed the key from the container
                    # when we read, ignore the ValueError that we would
                    # get otherwise.
                    pass
                self._append(key)
            return rv
        finally:
            self._wlock.release()

    def __setitem__(self, key, value):
        """Sets the value for an item. Moves the item up so that it
        has the highest priority then.
        """
        self._wlock.acquire()
        try:
            if key in self._mapping:
                self._remove(key)
            elif len(self._mapping) == self.capacity:
                del self._mapping[self._popleft()]
            self._append(key)
            self._mapping[key] = value
        finally:
            self._wlock.release()

    def __delitem__(self, key):
        """Remove an item from the cache dict.
        Raise a `KeyError` if it does not exist.
        """
        self._wlock.acquire()
        try:
            del self._mapping[key]
            try:
                self._remove(key)
            except ValueError:
                pass
        finally:
            self._wlock.release()

    def items(self):
        """Return a list of items."""
        result = [(key, self._mapping[key]) for key in list(self._queue)]
        result.reverse()
        return result

    def iteritems(self):
        """Iterate over all items."""
        warnings.warn(
            "'iteritems()' will be removed in version 3.0. Use"
            " 'iter(cache.items())' instead.",
            DeprecationWarning,
            stacklevel=2,
        )
        return iter(self.items())

    def values(self):
        """Return a list of all values."""
        return [x[1] for x in self.items()]

    def itervalue(self):
        """Iterate over all values."""
        warnings.warn(
            "'itervalue()' will be removed in version 3.0. Use"
            " 'iter(cache.values())' instead.",
            DeprecationWarning,
            stacklevel=2,
        )
        return iter(self.values())

    def itervalues(self):
        """Iterate over all values."""
        warnings.warn(
            "'itervalues()' will be removed in version 3.0. Use"
            " 'iter(cache.values())' instead.",
            DeprecationWarning,
            stacklevel=2,
        )
        return iter(self.values())

    def keys(self):
        """Return a list of all keys ordered by most recent usage."""
        return list(self)

    def iterkeys(self):
        """Iterate over all keys in the cache dict, ordered by
        the most recent usage.
        """
        warnings.warn(
            "'iterkeys()' will be removed in version 3.0. Use"
            " 'iter(cache.keys())' instead.",
            DeprecationWarning,
            stacklevel=2,
        )
        return iter(self)

    def __iter__(self):
        return reversed(tuple(self._queue))

    def __reversed__(self):
        """Iterate over the keys in the cache dict, oldest items
        coming first.
        """
        return iter(tuple(self._queue))

    __copy__ = copy


abc.MutableMapping.register(LRUCache)


def select_autoescape(
    enabled_extensions=("html", "htm", "xml"),
    disabled_extensions=(),
    default_for_string=True,
    default=False,
):
    """Intelligently sets the initial value of autoescaping based on the
    filename of the template.  This is the recommended way to configure
    autoescaping if you do not want to write a custom function yourself.

    If you want to enable it for all templates created from strings or
    for all templates with `.html` and `.xml` extensions::

        from jinja2 import Environment, select_autoescape
        env = Environment(autoescape=select_autoescape(
            enabled_extensions=('html', 'xml'),
            default_for_string=True,
        ))

    Example configuration to turn it on at all times except if the template
    ends with `.txt`::

        from jinja2 import Environment, select_autoescape
        env = Environment(autoescape=select_autoescape(
            disabled_extensions=('txt',),
            default_for_string=True,
            default=True,
        ))

    The `enabled_extensions` is an iterable of all the extensions that
    autoescaping should be enabled for.  Likewise `disabled_extensions` is
    a list of all templates it should be disabled for.  If a template is
    loaded from a string then the default from `default_for_string` is used.
    If nothing matches then the initial value of autoescaping is set to the
    value of `default`.

    For security reasons this function operates case insensitive.

    .. versionadded:: 2.9
    """
    enabled_patterns = tuple("." + x.lstrip(".").lower() for x in enabled_extensions)
    disabled_patterns = tuple("." + x.lstrip(".").lower() for x in disabled_extensions)

    def autoescape(template_name):
        if template_name is None:
            return default_for_string
        template_name = template_name.lower()
        if template_name.endswith(enabled_patterns):
            return True
        if template_name.endswith(disabled_patterns):
            return False
        return default

    return autoescape


def htmlsafe_json_dumps(obj, dumper=None, **kwargs):
    """Works exactly like :func:`dumps` but is safe for use in ``<script>``
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
    """
    if dumper is None:
        dumper = json.dumps
    rv = (
        dumper(obj, **kwargs)
        .replace(u"<", u"\\u003c")
        .replace(u">", u"\\u003e")
        .replace(u"&", u"\\u0026")
        .replace(u"'", u"\\u0027")
    )
    return Markup(rv)


class Cycler(object):
    """Cycle through values by yield them one at a time, then restarting
    once the end is reached. Available as ``cycler`` in templates.

    Similar to ``loop.cycle``, but can be used outside loops or across
    multiple loops. For example, render a list of folders and files in a
    list, alternating giving them "odd" and "even" classes.

    .. code-block:: html+jinja

        {% set row_class = cycler("odd", "even") %}
        <ul class="browser">
        {% for folder in folders %}
          <li class="folder {{ row_class.next() }}">{{ folder }}
        {% endfor %}
        {% for file in files %}
          <li class="file {{ row_class.next() }}">{{ file }}
        {% endfor %}
        </ul>

    :param items: Each positional argument will be yielded in the order
        given for each cycle.

    .. versionadded:: 2.1
    """

    def __init__(self, *items):
        if not items:
            raise RuntimeError("at least one item has to be provided")
        self.items = items
        self.pos = 0

    def reset(self):
        """Resets the current item to the first item."""
        self.pos = 0

    @property
    def current(self):
        """Return the current item. Equivalent to the item that will be
        returned next time :meth:`next` is called.
        """
        return self.items[self.pos]

    def next(self):
        """Return the current item, then advance :attr:`current` to the
        next item.
        """
        rv = self.current
        self.pos = (self.pos + 1) % len(self.items)
        return rv

    __next__ = next


class Joiner(object):
    """A joining helper for templates."""

    def __init__(self, sep=u", "):
        self.sep = sep
        self.used = False

    def __call__(self):
        if not self.used:
            self.used = True
            return u""
        return self.sep


class Namespace(object):
    """A namespace object that can hold arbitrary attributes.  It may be
    initialized from a dictionary or with keyword arguments."""

    def __init__(*args, **kwargs):  # noqa: B902
        self, args = args[0], args[1:]
        self.__attrs = dict(*args, **kwargs)

    def __getattribute__(self, name):
        # __class__ is needed for the awaitable check in async mode
        if name in {"_Namespace__attrs", "__class__"}:
            return object.__getattribute__(self, name)
        try:
            return self.__attrs[name]
        except KeyError:
            raise AttributeError(name)

    def __setitem__(self, name, value):
        self.__attrs[name] = value

    def __repr__(self):
        return "<Namespace %r>" % self.__attrs


# does this python version support async for in and async generators?
try:
    exec("async def _():\n async for _ in ():\n  yield _")
    have_async_gen = True
except SyntaxError:
    have_async_gen = False


def soft_unicode(s):
    from markupsafe import soft_unicode

    warnings.warn(
        "'jinja2.utils.soft_unicode' will be removed in version 3.0."
        " Use 'markupsafe.soft_unicode' instead.",
        DeprecationWarning,
        stacklevel=2,
    )
    return soft_unicode(s)
