"""The runtime functions and state used by compiled templates."""
import functools
import sys
import typing as t
from collections import abc
from itertools import chain

from markupsafe import escape  # noqa: F401
from markupsafe import Markup
from markupsafe import soft_str

from .async_utils import auto_aiter
from .async_utils import auto_await  # noqa: F401
from .exceptions import TemplateNotFound  # noqa: F401
from .exceptions import TemplateRuntimeError  # noqa: F401
from .exceptions import UndefinedError
from .nodes import EvalContext
from .utils import _PassArg
from .utils import concat
from .utils import internalcode
from .utils import missing
from .utils import Namespace  # noqa: F401
from .utils import object_type_repr
from .utils import pass_eval_context

V = t.TypeVar("V")
F = t.TypeVar("F", bound=t.Callable[..., t.Any])

if t.TYPE_CHECKING:
    import logging
    import typing_extensions as te
    from .environment import Environment

    class LoopRenderFunc(te.Protocol):
        def __call__(
            self,
            reciter: t.Iterable[V],
            loop_render_func: "LoopRenderFunc",
            depth: int = 0,
        ) -> str:
            ...


# these variables are exported to the template runtime
exported = [
    "LoopContext",
    "TemplateReference",
    "Macro",
    "Markup",
    "TemplateRuntimeError",
    "missing",
    "escape",
    "markup_join",
    "str_join",
    "identity",
    "TemplateNotFound",
    "Namespace",
    "Undefined",
    "internalcode",
]
async_exported = [
    "AsyncLoopContext",
    "auto_aiter",
    "auto_await",
]


def identity(x: V) -> V:
    """Returns its argument. Useful for certain things in the
    environment.
    """
    return x


def markup_join(seq: t.Iterable[t.Any]) -> str:
    """Concatenation that escapes if necessary and converts to string."""
    buf = []
    iterator = map(soft_str, seq)
    for arg in iterator:
        buf.append(arg)
        if hasattr(arg, "__html__"):
            return Markup("").join(chain(buf, iterator))
    return concat(buf)


def str_join(seq: t.Iterable[t.Any]) -> str:
    """Simple args to string conversion and concatenation."""
    return concat(map(str, seq))


def new_context(
    environment: "Environment",
    template_name: t.Optional[str],
    blocks: t.Dict[str, t.Callable[["Context"], t.Iterator[str]]],
    vars: t.Optional[t.Dict[str, t.Any]] = None,
    shared: bool = False,
    globals: t.Optional[t.MutableMapping[str, t.Any]] = None,
    locals: t.Optional[t.Mapping[str, t.Any]] = None,
) -> "Context":
    """Internal helper for context creation."""
    if vars is None:
        vars = {}
    if shared:
        parent = vars
    else:
        parent = dict(globals or (), **vars)
    if locals:
        # if the parent is shared a copy should be created because
        # we don't want to modify the dict passed
        if shared:
            parent = dict(parent)
        for key, value in locals.items():
            if value is not missing:
                parent[key] = value
    return environment.context_class(
        environment, parent, template_name, blocks, globals=globals
    )


class TemplateReference:
    """The `self` in templates."""

    def __init__(self, context: "Context") -> None:
        self.__context = context

    def __getitem__(self, name: str) -> t.Any:
        blocks = self.__context.blocks[name]
        return BlockReference(name, self.__context, blocks, 0)

    def __repr__(self) -> str:
        return f"<{type(self).__name__} {self.__context.name!r}>"


def _dict_method_all(dict_method: F) -> F:
    @functools.wraps(dict_method)
    def f_all(self: "Context") -> t.Any:
        return dict_method(self.get_all())

    return t.cast(F, f_all)


@abc.Mapping.register
class Context:
    """The template context holds the variables of a template.  It stores the
    values passed to the template and also the names the template exports.
    Creating instances is neither supported nor useful as it's created
    automatically at various stages of the template evaluation and should not
    be created by hand.

    The context is immutable.  Modifications on :attr:`parent` **must not**
    happen and modifications on :attr:`vars` are allowed from generated
    template code only.  Template filters and global functions marked as
    :func:`pass_context` get the active context passed as first argument
    and are allowed to access the context read-only.

    The template context supports read only dict operations (`get`,
    `keys`, `values`, `items`, `iterkeys`, `itervalues`, `iteritems`,
    `__getitem__`, `__contains__`).  Additionally there is a :meth:`resolve`
    method that doesn't fail with a `KeyError` but returns an
    :class:`Undefined` object for missing variables.
    """

    def __init__(
        self,
        environment: "Environment",
        parent: t.Dict[str, t.Any],
        name: t.Optional[str],
        blocks: t.Dict[str, t.Callable[["Context"], t.Iterator[str]]],
        globals: t.Optional[t.MutableMapping[str, t.Any]] = None,
    ):
        self.parent = parent
        self.vars: t.Dict[str, t.Any] = {}
        self.environment: "Environment" = environment
        self.eval_ctx = EvalContext(self.environment, name)
        self.exported_vars: t.Set[str] = set()
        self.name = name
        self.globals_keys = set() if globals is None else set(globals)

        # create the initial mapping of blocks.  Whenever template inheritance
        # takes place the runtime will update this mapping with the new blocks
        # from the template.
        self.blocks = {k: [v] for k, v in blocks.items()}

    def super(
        self, name: str, current: t.Callable[["Context"], t.Iterator[str]]
    ) -> t.Union["BlockReference", "Undefined"]:
        """Render a parent block."""
        try:
            blocks = self.blocks[name]
            index = blocks.index(current) + 1
            blocks[index]
        except LookupError:
            return self.environment.undefined(
                f"there is no parent block called {name!r}.", name="super"
            )
        return BlockReference(name, self, blocks, index)

    def get(self, key: str, default: t.Any = None) -> t.Any:
        """Look up a variable by name, or return a default if the key is
        not found.

        :param key: The variable name to look up.
        :param default: The value to return if the key is not found.
        """
        try:
            return self[key]
        except KeyError:
            return default

    def resolve(self, key: str) -> t.Union[t.Any, "Undefined"]:
        """Look up a variable by name, or return an :class:`Undefined`
        object if the key is not found.

        If you need to add custom behavior, override
        :meth:`resolve_or_missing`, not this method. The various lookup
        functions use that method, not this one.

        :param key: The variable name to look up.
        """
        rv = self.resolve_or_missing(key)

        if rv is missing:
            return self.environment.undefined(name=key)

        return rv

    def resolve_or_missing(self, key: str) -> t.Any:
        """Look up a variable by name, or return a ``missing`` sentinel
        if the key is not found.

        Override this method to add custom lookup behavior.
        :meth:`resolve`, :meth:`get`, and :meth:`__getitem__` use this
        method. Don't call this method directly.

        :param key: The variable name to look up.
        """
        if key in self.vars:
            return self.vars[key]

        if key in self.parent:
            return self.parent[key]

        return missing

    def get_exported(self) -> t.Dict[str, t.Any]:
        """Get a new dict with the exported variables."""
        return {k: self.vars[k] for k in self.exported_vars}

    def get_all(self) -> t.Dict[str, t.Any]:
        """Return the complete context as dict including the exported
        variables.  For optimizations reasons this might not return an
        actual copy so be careful with using it.
        """
        if not self.vars:
            return self.parent
        if not self.parent:
            return self.vars
        return dict(self.parent, **self.vars)

    @internalcode
    def call(
        __self, __obj: t.Callable, *args: t.Any, **kwargs: t.Any  # noqa: B902
    ) -> t.Union[t.Any, "Undefined"]:
        """Call the callable with the arguments and keyword arguments
        provided but inject the active context or environment as first
        argument if the callable has :func:`pass_context` or
        :func:`pass_environment`.
        """
        if __debug__:
            __traceback_hide__ = True  # noqa

        # Allow callable classes to take a context
        if (
            hasattr(__obj, "__call__")  # noqa: B004
            and _PassArg.from_obj(__obj.__call__) is not None  # type: ignore
        ):
            __obj = __obj.__call__  # type: ignore

        pass_arg = _PassArg.from_obj(__obj)

        if pass_arg is _PassArg.context:
            # the active context should have access to variables set in
            # loops and blocks without mutating the context itself
            if kwargs.get("_loop_vars"):
                __self = __self.derived(kwargs["_loop_vars"])
            if kwargs.get("_block_vars"):
                __self = __self.derived(kwargs["_block_vars"])
            args = (__self,) + args
        elif pass_arg is _PassArg.eval_context:
            args = (__self.eval_ctx,) + args
        elif pass_arg is _PassArg.environment:
            args = (__self.environment,) + args

        kwargs.pop("_block_vars", None)
        kwargs.pop("_loop_vars", None)

        try:
            return __obj(*args, **kwargs)
        except StopIteration:
            return __self.environment.undefined(
                "value was undefined because a callable raised a"
                " StopIteration exception"
            )

    def derived(self, locals: t.Optional[t.Dict[str, t.Any]] = None) -> "Context":
        """Internal helper function to create a derived context.  This is
        used in situations where the system needs a new context in the same
        template that is independent.
        """
        context = new_context(
            self.environment, self.name, {}, self.get_all(), True, None, locals
        )
        context.eval_ctx = self.eval_ctx
        context.blocks.update((k, list(v)) for k, v in self.blocks.items())
        return context

    keys = _dict_method_all(dict.keys)
    values = _dict_method_all(dict.values)
    items = _dict_method_all(dict.items)

    def __contains__(self, name: str) -> bool:
        return name in self.vars or name in self.parent

    def __getitem__(self, key: str) -> t.Any:
        """Look up a variable by name with ``[]`` syntax, or raise a
        ``KeyError`` if the key is not found.
        """
        item = self.resolve_or_missing(key)

        if item is missing:
            raise KeyError(key)

        return item

    def __repr__(self) -> str:
        return f"<{type(self).__name__} {self.get_all()!r} of {self.name!r}>"


class BlockReference:
    """One block on a template reference."""

    def __init__(
        self,
        name: str,
        context: "Context",
        stack: t.List[t.Callable[["Context"], t.Iterator[str]]],
        depth: int,
    ) -> None:
        self.name = name
        self._context = context
        self._stack = stack
        self._depth = depth

    @property
    def super(self) -> t.Union["BlockReference", "Undefined"]:
        """Super the block."""
        if self._depth + 1 >= len(self._stack):
            return self._context.environment.undefined(
                f"there is no parent block called {self.name!r}.", name="super"
            )
        return BlockReference(self.name, self._context, self._stack, self._depth + 1)

    @internalcode
    async def _async_call(self) -> str:
        rv = concat(
            [x async for x in self._stack[self._depth](self._context)]  # type: ignore
        )

        if self._context.eval_ctx.autoescape:
            return Markup(rv)

        return rv

    @internalcode
    def __call__(self) -> str:
        if self._context.environment.is_async:
            return self._async_call()  # type: ignore

        rv = concat(self._stack[self._depth](self._context))

        if self._context.eval_ctx.autoescape:
            return Markup(rv)

        return rv


class LoopContext:
    """A wrapper iterable for dynamic ``for`` loops, with information
    about the loop and iteration.
    """

    #: Current iteration of the loop, starting at 0.
    index0 = -1

    _length: t.Optional[int] = None
    _after: t.Any = missing
    _current: t.Any = missing
    _before: t.Any = missing
    _last_changed_value: t.Any = missing

    def __init__(
        self,
        iterable: t.Iterable[V],
        undefined: t.Type["Undefined"],
        recurse: t.Optional["LoopRenderFunc"] = None,
        depth0: int = 0,
    ) -> None:
        """
        :param iterable: Iterable to wrap.
        :param undefined: :class:`Undefined` class to use for next and
            previous items.
        :param recurse: The function to render the loop body when the
            loop is marked recursive.
        :param depth0: Incremented when looping recursively.
        """
        self._iterable = iterable
        self._iterator = self._to_iterator(iterable)
        self._undefined = undefined
        self._recurse = recurse
        #: How many levels deep a recursive loop currently is, starting at 0.
        self.depth0 = depth0

    @staticmethod
    def _to_iterator(iterable: t.Iterable[V]) -> t.Iterator[V]:
        return iter(iterable)

    @property
    def length(self) -> int:
        """Length of the iterable.

        If the iterable is a generator or otherwise does not have a
        size, it is eagerly evaluated to get a size.
        """
        if self._length is not None:
            return self._length

        try:
            self._length = len(self._iterable)  # type: ignore
        except TypeError:
            iterable = list(self._iterator)
            self._iterator = self._to_iterator(iterable)
            self._length = len(iterable) + self.index + (self._after is not missing)

        return self._length

    def __len__(self) -> int:
        return self.length

    @property
    def depth(self) -> int:
        """How many levels deep a recursive loop currently is, starting at 1."""
        return self.depth0 + 1

    @property
    def index(self) -> int:
        """Current iteration of the loop, starting at 1."""
        return self.index0 + 1

    @property
    def revindex0(self) -> int:
        """Number of iterations from the end of the loop, ending at 0.

        Requires calculating :attr:`length`.
        """
        return self.length - self.index

    @property
    def revindex(self) -> int:
        """Number of iterations from the end of the loop, ending at 1.

        Requires calculating :attr:`length`.
        """
        return self.length - self.index0

    @property
    def first(self) -> bool:
        """Whether this is the first iteration of the loop."""
        return self.index0 == 0

    def _peek_next(self) -> t.Any:
        """Return the next element in the iterable, or :data:`missing`
        if the iterable is exhausted. Only peeks one item ahead, caching
        the result in :attr:`_last` for use in subsequent checks. The
        cache is reset when :meth:`__next__` is called.
        """
        if self._after is not missing:
            return self._after

        self._after = next(self._iterator, missing)
        return self._after

    @property
    def last(self) -> bool:
        """Whether this is the last iteration of the loop.

        Causes the iterable to advance early. See
        :func:`itertools.groupby` for issues this can cause.
        The :func:`groupby` filter avoids that issue.
        """
        return self._peek_next() is missing

    @property
    def previtem(self) -> t.Union[t.Any, "Undefined"]:
        """The item in the previous iteration. Undefined during the
        first iteration.
        """
        if self.first:
            return self._undefined("there is no previous item")

        return self._before

    @property
    def nextitem(self) -> t.Union[t.Any, "Undefined"]:
        """The item in the next iteration. Undefined during the last
        iteration.

        Causes the iterable to advance early. See
        :func:`itertools.groupby` for issues this can cause.
        The :func:`jinja-filters.groupby` filter avoids that issue.
        """
        rv = self._peek_next()

        if rv is missing:
            return self._undefined("there is no next item")

        return rv

    def cycle(self, *args: V) -> V:
        """Return a value from the given args, cycling through based on
        the current :attr:`index0`.

        :param args: One or more values to cycle through.
        """
        if not args:
            raise TypeError("no items for cycling given")

        return args[self.index0 % len(args)]

    def changed(self, *value: t.Any) -> bool:
        """Return ``True`` if previously called with a different value
        (including when called for the first time).

        :param value: One or more values to compare to the last call.
        """
        if self._last_changed_value != value:
            self._last_changed_value = value
            return True

        return False

    def __iter__(self) -> "LoopContext":
        return self

    def __next__(self) -> t.Tuple[t.Any, "LoopContext"]:
        if self._after is not missing:
            rv = self._after
            self._after = missing
        else:
            rv = next(self._iterator)

        self.index0 += 1
        self._before = self._current
        self._current = rv
        return rv, self

    @internalcode
    def __call__(self, iterable: t.Iterable[V]) -> str:
        """When iterating over nested data, render the body of the loop
        recursively with the given inner iterable data.

        The loop must have the ``recursive`` marker for this to work.
        """
        if self._recurse is None:
            raise TypeError(
                "The loop must have the 'recursive' marker to be called recursively."
            )

        return self._recurse(iterable, self._recurse, depth=self.depth)

    def __repr__(self) -> str:
        return f"<{type(self).__name__} {self.index}/{self.length}>"


class AsyncLoopContext(LoopContext):
    _iterator: t.AsyncIterator[t.Any]  # type: ignore

    @staticmethod
    def _to_iterator(  # type: ignore
        iterable: t.Union[t.Iterable[V], t.AsyncIterable[V]]
    ) -> t.AsyncIterator[V]:
        return auto_aiter(iterable)

    @property
    async def length(self) -> int:  # type: ignore
        if self._length is not None:
            return self._length

        try:
            self._length = len(self._iterable)  # type: ignore
        except TypeError:
            iterable = [x async for x in self._iterator]
            self._iterator = self._to_iterator(iterable)
            self._length = len(iterable) + self.index + (self._after is not missing)

        return self._length

    @property
    async def revindex0(self) -> int:  # type: ignore
        return await self.length - self.index

    @property
    async def revindex(self) -> int:  # type: ignore
        return await self.length - self.index0

    async def _peek_next(self) -> t.Any:
        if self._after is not missing:
            return self._after

        try:
            self._after = await self._iterator.__anext__()
        except StopAsyncIteration:
            self._after = missing

        return self._after

    @property
    async def last(self) -> bool:  # type: ignore
        return await self._peek_next() is missing

    @property
    async def nextitem(self) -> t.Union[t.Any, "Undefined"]:
        rv = await self._peek_next()

        if rv is missing:
            return self._undefined("there is no next item")

        return rv

    def __aiter__(self) -> "AsyncLoopContext":
        return self

    async def __anext__(self) -> t.Tuple[t.Any, "AsyncLoopContext"]:
        if self._after is not missing:
            rv = self._after
            self._after = missing
        else:
            rv = await self._iterator.__anext__()

        self.index0 += 1
        self._before = self._current
        self._current = rv
        return rv, self


class Macro:
    """Wraps a macro function."""

    def __init__(
        self,
        environment: "Environment",
        func: t.Callable[..., str],
        name: str,
        arguments: t.List[str],
        catch_kwargs: bool,
        catch_varargs: bool,
        caller: bool,
        default_autoescape: t.Optional[bool] = None,
    ):
        self._environment = environment
        self._func = func
        self._argument_count = len(arguments)
        self.name = name
        self.arguments = arguments
        self.catch_kwargs = catch_kwargs
        self.catch_varargs = catch_varargs
        self.caller = caller
        self.explicit_caller = "caller" in arguments

        if default_autoescape is None:
            if callable(environment.autoescape):
                default_autoescape = environment.autoescape(None)
            else:
                default_autoescape = environment.autoescape

        self._default_autoescape = default_autoescape

    @internalcode
    @pass_eval_context
    def __call__(self, *args: t.Any, **kwargs: t.Any) -> str:
        # This requires a bit of explanation,  In the past we used to
        # decide largely based on compile-time information if a macro is
        # safe or unsafe.  While there was a volatile mode it was largely
        # unused for deciding on escaping.  This turns out to be
        # problematic for macros because whether a macro is safe depends not
        # on the escape mode when it was defined, but rather when it was used.
        #
        # Because however we export macros from the module system and
        # there are historic callers that do not pass an eval context (and
        # will continue to not pass one), we need to perform an instance
        # check here.
        #
        # This is considered safe because an eval context is not a valid
        # argument to callables otherwise anyway.  Worst case here is
        # that if no eval context is passed we fall back to the compile
        # time autoescape flag.
        if args and isinstance(args[0], EvalContext):
            autoescape = args[0].autoescape
            args = args[1:]
        else:
            autoescape = self._default_autoescape

        # try to consume the positional arguments
        arguments = list(args[: self._argument_count])
        off = len(arguments)

        # For information why this is necessary refer to the handling
        # of caller in the `macro_body` handler in the compiler.
        found_caller = False

        # if the number of arguments consumed is not the number of
        # arguments expected we start filling in keyword arguments
        # and defaults.
        if off != self._argument_count:
            for name in self.arguments[len(arguments) :]:
                try:
                    value = kwargs.pop(name)
                except KeyError:
                    value = missing
                if name == "caller":
                    found_caller = True
                arguments.append(value)
        else:
            found_caller = self.explicit_caller

        # it's important that the order of these arguments does not change
        # if not also changed in the compiler's `function_scoping` method.
        # the order is caller, keyword arguments, positional arguments!
        if self.caller and not found_caller:
            caller = kwargs.pop("caller", None)
            if caller is None:
                caller = self._environment.undefined("No caller defined", name="caller")
            arguments.append(caller)

        if self.catch_kwargs:
            arguments.append(kwargs)
        elif kwargs:
            if "caller" in kwargs:
                raise TypeError(
                    f"macro {self.name!r} was invoked with two values for the special"
                    " caller argument. This is most likely a bug."
                )
            raise TypeError(
                f"macro {self.name!r} takes no keyword argument {next(iter(kwargs))!r}"
            )
        if self.catch_varargs:
            arguments.append(args[self._argument_count :])
        elif len(args) > self._argument_count:
            raise TypeError(
                f"macro {self.name!r} takes not more than"
                f" {len(self.arguments)} argument(s)"
            )

        return self._invoke(arguments, autoescape)

    async def _async_invoke(self, arguments: t.List[t.Any], autoescape: bool) -> str:
        rv = await self._func(*arguments)  # type: ignore

        if autoescape:
            return Markup(rv)

        return rv  # type: ignore

    def _invoke(self, arguments: t.List[t.Any], autoescape: bool) -> str:
        if self._environment.is_async:
            return self._async_invoke(arguments, autoescape)  # type: ignore

        rv = self._func(*arguments)

        if autoescape:
            rv = Markup(rv)

        return rv

    def __repr__(self) -> str:
        name = "anonymous" if self.name is None else repr(self.name)
        return f"<{type(self).__name__} {name}>"


class Undefined:
    """The default undefined type.  This undefined type can be printed and
    iterated over, but every other access will raise an :exc:`UndefinedError`:

    >>> foo = Undefined(name='foo')
    >>> str(foo)
    ''
    >>> not foo
    True
    >>> foo + 42
    Traceback (most recent call last):
      ...
    jinja2.exceptions.UndefinedError: 'foo' is undefined
    """

    __slots__ = (
        "_undefined_hint",
        "_undefined_obj",
        "_undefined_name",
        "_undefined_exception",
    )

    def __init__(
        self,
        hint: t.Optional[str] = None,
        obj: t.Any = missing,
        name: t.Optional[str] = None,
        exc: t.Type[TemplateRuntimeError] = UndefinedError,
    ) -> None:
        self._undefined_hint = hint
        self._undefined_obj = obj
        self._undefined_name = name
        self._undefined_exception = exc

    @property
    def _undefined_message(self) -> str:
        """Build a message about the undefined value based on how it was
        accessed.
        """
        if self._undefined_hint:
            return self._undefined_hint

        if self._undefined_obj is missing:
            return f"{self._undefined_name!r} is undefined"

        if not isinstance(self._undefined_name, str):
            return (
                f"{object_type_repr(self._undefined_obj)} has no"
                f" element {self._undefined_name!r}"
            )

        return (
            f"{object_type_repr(self._undefined_obj)!r} has no"
            f" attribute {self._undefined_name!r}"
        )

    @internalcode
    def _fail_with_undefined_error(
        self, *args: t.Any, **kwargs: t.Any
    ) -> "te.NoReturn":
        """Raise an :exc:`UndefinedError` when operations are performed
        on the undefined value.
        """
        raise self._undefined_exception(self._undefined_message)

    @internalcode
    def __getattr__(self, name: str) -> t.Any:
        if name[:2] == "__":
            raise AttributeError(name)

        return self._fail_with_undefined_error()

    __add__ = __radd__ = __sub__ = __rsub__ = _fail_with_undefined_error
    __mul__ = __rmul__ = __div__ = __rdiv__ = _fail_with_undefined_error
    __truediv__ = __rtruediv__ = _fail_with_undefined_error
    __floordiv__ = __rfloordiv__ = _fail_with_undefined_error
    __mod__ = __rmod__ = _fail_with_undefined_error
    __pos__ = __neg__ = _fail_with_undefined_error
    __call__ = __getitem__ = _fail_with_undefined_error
    __lt__ = __le__ = __gt__ = __ge__ = _fail_with_undefined_error
    __int__ = __float__ = __complex__ = _fail_with_undefined_error
    __pow__ = __rpow__ = _fail_with_undefined_error

    def __eq__(self, other: t.Any) -> bool:
        return type(self) is type(other)

    def __ne__(self, other: t.Any) -> bool:
        return not self.__eq__(other)

    def __hash__(self) -> int:
        return id(type(self))

    def __str__(self) -> str:
        return ""

    def __len__(self) -> int:
        return 0

    def __iter__(self) -> t.Iterator[t.Any]:
        yield from ()

    async def __aiter__(self) -> t.AsyncIterator[t.Any]:
        for _ in ():
            yield

    def __bool__(self) -> bool:
        return False

    def __repr__(self) -> str:
        return "Undefined"


def make_logging_undefined(
    logger: t.Optional["logging.Logger"] = None, base: t.Type[Undefined] = Undefined
) -> t.Type[Undefined]:
    """Given a logger object this returns a new undefined class that will
    log certain failures.  It will log iterations and printing.  If no
    logger is given a default logger is created.

    Example::

        logger = logging.getLogger(__name__)
        LoggingUndefined = make_logging_undefined(
            logger=logger,
            base=Undefined
        )

    .. versionadded:: 2.8

    :param logger: the logger to use.  If not provided, a default logger
                   is created.
    :param base: the base class to add logging functionality to.  This
                 defaults to :class:`Undefined`.
    """
    if logger is None:
        import logging

        logger = logging.getLogger(__name__)
        logger.addHandler(logging.StreamHandler(sys.stderr))

    def _log_message(undef: Undefined) -> None:
        logger.warning(  # type: ignore
            "Template variable warning: %s", undef._undefined_message
        )

    class LoggingUndefined(base):  # type: ignore
        __slots__ = ()

        def _fail_with_undefined_error(  # type: ignore
            self, *args: t.Any, **kwargs: t.Any
        ) -> "te.NoReturn":
            try:
                super()._fail_with_undefined_error(*args, **kwargs)
            except self._undefined_exception as e:
                logger.error("Template variable error: %s", e)  # type: ignore
                raise e

        def __str__(self) -> str:
            _log_message(self)
            return super().__str__()  # type: ignore

        def __iter__(self) -> t.Iterator[t.Any]:
            _log_message(self)
            return super().__iter__()  # type: ignore

        def __bool__(self) -> bool:
            _log_message(self)
            return super().__bool__()  # type: ignore

    return LoggingUndefined


class ChainableUndefined(Undefined):
    """An undefined that is chainable, where both ``__getattr__`` and
    ``__getitem__`` return itself rather than raising an
    :exc:`UndefinedError`.

    >>> foo = ChainableUndefined(name='foo')
    >>> str(foo.bar['baz'])
    ''
    >>> foo.bar['baz'] + 42
    Traceback (most recent call last):
      ...
    jinja2.exceptions.UndefinedError: 'foo' is undefined

    .. versionadded:: 2.11.0
    """

    __slots__ = ()

    def __html__(self) -> str:
        return str(self)

    def __getattr__(self, _: str) -> "ChainableUndefined":
        return self

    __getitem__ = __getattr__  # type: ignore


class DebugUndefined(Undefined):
    """An undefined that returns the debug info when printed.

    >>> foo = DebugUndefined(name='foo')
    >>> str(foo)
    '{{ foo }}'
    >>> not foo
    True
    >>> foo + 42
    Traceback (most recent call last):
      ...
    jinja2.exceptions.UndefinedError: 'foo' is undefined
    """

    __slots__ = ()

    def __str__(self) -> str:
        if self._undefined_hint:
            message = f"undefined value printed: {self._undefined_hint}"

        elif self._undefined_obj is missing:
            message = self._undefined_name  # type: ignore

        else:
            message = (
                f"no such element: {object_type_repr(self._undefined_obj)}"
                f"[{self._undefined_name!r}]"
            )

        return f"{{{{ {message} }}}}"


class StrictUndefined(Undefined):
    """An undefined that barks on print and iteration as well as boolean
    tests and all kinds of comparisons.  In other words: you can do nothing
    with it except checking if it's defined using the `defined` test.

    >>> foo = StrictUndefined(name='foo')
    >>> str(foo)
    Traceback (most recent call last):
      ...
    jinja2.exceptions.UndefinedError: 'foo' is undefined
    >>> not foo
    Traceback (most recent call last):
      ...
    jinja2.exceptions.UndefinedError: 'foo' is undefined
    >>> foo + 42
    Traceback (most recent call last):
      ...
    jinja2.exceptions.UndefinedError: 'foo' is undefined
    """

    __slots__ = ()
    __iter__ = __str__ = __len__ = Undefined._fail_with_undefined_error
    __eq__ = __ne__ = __bool__ = __hash__ = Undefined._fail_with_undefined_error
    __contains__ = Undefined._fail_with_undefined_error


# Remove slots attributes, after the metaclass is applied they are
# unneeded and contain wrong data for subclasses.
del (
    Undefined.__slots__,
    ChainableUndefined.__slots__,
    DebugUndefined.__slots__,
    StrictUndefined.__slots__,
)
