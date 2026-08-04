"""Extension API for adding custom tags and behavior."""
import pprint
import re
import typing as t

from markupsafe import Markup

from . import defaults
from . import nodes
from .environment import Environment
from .exceptions import TemplateAssertionError
from .exceptions import TemplateSyntaxError
from .runtime import concat  # type: ignore
from .runtime import Context
from .runtime import Undefined
from .utils import import_string
from .utils import pass_context

if t.TYPE_CHECKING:
    import typing_extensions as te
    from .lexer import Token
    from .lexer import TokenStream
    from .parser import Parser

    class _TranslationsBasic(te.Protocol):
        def gettext(self, message: str) -> str:
            ...

        def ngettext(self, singular: str, plural: str, n: int) -> str:
            pass

    class _TranslationsContext(_TranslationsBasic):
        def pgettext(self, context: str, message: str) -> str:
            ...

        def npgettext(self, context: str, singular: str, plural: str, n: int) -> str:
            ...

    _SupportedTranslations = t.Union[_TranslationsBasic, _TranslationsContext]


# I18N functions available in Jinja templates. If the I18N library
# provides ugettext, it will be assigned to gettext.
GETTEXT_FUNCTIONS: t.Tuple[str, ...] = (
    "_",
    "gettext",
    "ngettext",
    "pgettext",
    "npgettext",
)
_ws_re = re.compile(r"\s*\n\s*")


class Extension:
    """Extensions can be used to add extra functionality to the Jinja template
    system at the parser level.  Custom extensions are bound to an environment
    but may not store environment specific data on `self`.  The reason for
    this is that an extension can be bound to another environment (for
    overlays) by creating a copy and reassigning the `environment` attribute.

    As extensions are created by the environment they cannot accept any
    arguments for configuration.  One may want to work around that by using
    a factory function, but that is not possible as extensions are identified
    by their import name.  The correct way to configure the extension is
    storing the configuration values on the environment.  Because this way the
    environment ends up acting as central configuration storage the
    attributes may clash which is why extensions have to ensure that the names
    they choose for configuration are not too generic.  ``prefix`` for example
    is a terrible name, ``fragment_cache_prefix`` on the other hand is a good
    name as includes the name of the extension (fragment cache).
    """

    identifier: t.ClassVar[str]

    def __init_subclass__(cls) -> None:
        cls.identifier = f"{cls.__module__}.{cls.__name__}"

    #: if this extension parses this is the list of tags it's listening to.
    tags: t.Set[str] = set()

    #: the priority of that extension.  This is especially useful for
    #: extensions that preprocess values.  A lower value means higher
    #: priority.
    #:
    #: .. versionadded:: 2.4
    priority = 100

    def __init__(self, environment: Environment) -> None:
        self.environment = environment

    def bind(self, environment: Environment) -> "Extension":
        """Create a copy of this extension bound to another environment."""
        rv = object.__new__(self.__class__)
        rv.__dict__.update(self.__dict__)
        rv.environment = environment
        return rv

    def preprocess(
        self, source: str, name: t.Optional[str], filename: t.Optional[str] = None
    ) -> str:
        """This method is called before the actual lexing and can be used to
        preprocess the source.  The `filename` is optional.  The return value
        must be the preprocessed source.
        """
        return source

    def filter_stream(
        self, stream: "TokenStream"
    ) -> t.Union["TokenStream", t.Iterable["Token"]]:
        """It's passed a :class:`~jinja2.lexer.TokenStream` that can be used
        to filter tokens returned.  This method has to return an iterable of
        :class:`~jinja2.lexer.Token`\\s, but it doesn't have to return a
        :class:`~jinja2.lexer.TokenStream`.
        """
        return stream

    def parse(self, parser: "Parser") -> t.Union[nodes.Node, t.List[nodes.Node]]:
        """If any of the :attr:`tags` matched this method is called with the
        parser as first argument.  The token the parser stream is pointing at
        is the name token that matched.  This method has to return one or a
        list of multiple nodes.
        """
        raise NotImplementedError()

    def attr(
        self, name: str, lineno: t.Optional[int] = None
    ) -> nodes.ExtensionAttribute:
        """Return an attribute node for the current extension.  This is useful
        to pass constants on extensions to generated template code.

        ::

            self.attr('_my_attribute', lineno=lineno)
        """
        return nodes.ExtensionAttribute(self.identifier, name, lineno=lineno)

    def call_method(
        self,
        name: str,
        args: t.Optional[t.List[nodes.Expr]] = None,
        kwargs: t.Optional[t.List[nodes.Keyword]] = None,
        dyn_args: t.Optional[nodes.Expr] = None,
        dyn_kwargs: t.Optional[nodes.Expr] = None,
        lineno: t.Optional[int] = None,
    ) -> nodes.Call:
        """Call a method of the extension.  This is a shortcut for
        :meth:`attr` + :class:`jinja2.nodes.Call`.
        """
        if args is None:
            args = []
        if kwargs is None:
            kwargs = []
        return nodes.Call(
            self.attr(name, lineno=lineno),
            args,
            kwargs,
            dyn_args,
            dyn_kwargs,
            lineno=lineno,
        )


@pass_context
def _gettext_alias(
    __context: Context, *args: t.Any, **kwargs: t.Any
) -> t.Union[t.Any, Undefined]:
    return __context.call(__context.resolve("gettext"), *args, **kwargs)


def _make_new_gettext(func: t.Callable[[str], str]) -> t.Callable[..., str]:
    @pass_context
    def gettext(__context: Context, __string: str, **variables: t.Any) -> str:
        rv = __context.call(func, __string)
        if __context.eval_ctx.autoescape:
            rv = Markup(rv)
        # Always treat as a format string, even if there are no
        # variables. This makes translation strings more consistent
        # and predictable. This requires escaping
        return rv % variables  # type: ignore

    return gettext


def _make_new_ngettext(func: t.Callable[[str, str, int], str]) -> t.Callable[..., str]:
    @pass_context
    def ngettext(
        __context: Context,
        __singular: str,
        __plural: str,
        __num: int,
        **variables: t.Any,
    ) -> str:
        variables.setdefault("num", __num)
        rv = __context.call(func, __singular, __plural, __num)
        if __context.eval_ctx.autoescape:
            rv = Markup(rv)
        # Always treat as a format string, see gettext comment above.
        return rv % variables  # type: ignore

    return ngettext


def _make_new_pgettext(func: t.Callable[[str, str], str]) -> t.Callable[..., str]:
    @pass_context
    def pgettext(
        __context: Context, __string_ctx: str, __string: str, **variables: t.Any
    ) -> str:
        variables.setdefault("context", __string_ctx)
        rv = __context.call(func, __string_ctx, __string)

        if __context.eval_ctx.autoescape:
            rv = Markup(rv)

        # Always treat as a format string, see gettext comment above.
        return rv % variables  # type: ignore

    return pgettext


def _make_new_npgettext(
    func: t.Callable[[str, str, str, int], str]
) -> t.Callable[..., str]:
    @pass_context
    def npgettext(
        __context: Context,
        __string_ctx: str,
        __singular: str,
        __plural: str,
        __num: int,
        **variables: t.Any,
    ) -> str:
        variables.setdefault("context", __string_ctx)
        variables.setdefault("num", __num)
        rv = __context.call(func, __string_ctx, __singular, __plural, __num)

        if __context.eval_ctx.autoescape:
            rv = Markup(rv)

        # Always treat as a format string, see gettext comment above.
        return rv % variables  # type: ignore

    return npgettext


class InternationalizationExtension(Extension):
    """This extension adds gettext support to Jinja."""

    tags = {"trans"}

    # TODO: the i18n extension is currently reevaluating values in a few
    # situations.  Take this example:
    #   {% trans count=something() %}{{ count }} foo{% pluralize
    #     %}{{ count }} fooss{% endtrans %}
    # something is called twice here.  One time for the gettext value and
    # the other time for the n-parameter of the ngettext function.

    def __init__(self, environment: Environment) -> None:
        super().__init__(environment)
        environment.globals["_"] = _gettext_alias
        environment.extend(
            install_gettext_translations=self._install,
            install_null_translations=self._install_null,
            install_gettext_callables=self._install_callables,
            uninstall_gettext_translations=self._uninstall,
            extract_translations=self._extract,
            newstyle_gettext=False,
        )

    def _install(
        self, translations: "_SupportedTranslations", newstyle: t.Optional[bool] = None
    ) -> None:
        # ugettext and ungettext are preferred in case the I18N library
        # is providing compatibility with older Python versions.
        gettext = getattr(translations, "ugettext", None)
        if gettext is None:
            gettext = translations.gettext
        ngettext = getattr(translations, "ungettext", None)
        if ngettext is None:
            ngettext = translations.ngettext

        pgettext = getattr(translations, "pgettext", None)
        npgettext = getattr(translations, "npgettext", None)
        self._install_callables(
            gettext, ngettext, newstyle=newstyle, pgettext=pgettext, npgettext=npgettext
        )

    def _install_null(self, newstyle: t.Optional[bool] = None) -> None:
        import gettext

        translations = gettext.NullTranslations()

        if hasattr(translations, "pgettext"):
            # Python < 3.8
            pgettext = translations.pgettext  # type: ignore
        else:

            def pgettext(c: str, s: str) -> str:
                return s

        if hasattr(translations, "npgettext"):
            npgettext = translations.npgettext  # type: ignore
        else:

            def npgettext(c: str, s: str, p: str, n: int) -> str:
                return s if n == 1 else p

        self._install_callables(
            gettext=translations.gettext,
            ngettext=translations.ngettext,
            newstyle=newstyle,
            pgettext=pgettext,
            npgettext=npgettext,
        )

    def _install_callables(
        self,
        gettext: t.Callable[[str], str],
        ngettext: t.Callable[[str, str, int], str],
        newstyle: t.Optional[bool] = None,
        pgettext: t.Optional[t.Callable[[str, str], str]] = None,
        npgettext: t.Optional[t.Callable[[str, str, str, int], str]] = None,
    ) -> None:
        if newstyle is not None:
            self.environment.newstyle_gettext = newstyle  # type: ignore
        if self.environment.newstyle_gettext:  # type: ignore
            gettext = _make_new_gettext(gettext)
            ngettext = _make_new_ngettext(ngettext)

            if pgettext is not None:
                pgettext = _make_new_pgettext(pgettext)

            if npgettext is not None:
                npgettext = _make_new_npgettext(npgettext)

        self.environment.globals.update(
            gettext=gettext, ngettext=ngettext, pgettext=pgettext, npgettext=npgettext
        )

    def _uninstall(self, translations: "_SupportedTranslations") -> None:
        for key in ("gettext", "ngettext", "pgettext", "npgettext"):
            self.environment.globals.pop(key, None)

    def _extract(
        self,
        source: t.Union[str, nodes.Template],
        gettext_functions: t.Sequence[str] = GETTEXT_FUNCTIONS,
    ) -> t.Iterator[
        t.Tuple[int, str, t.Union[t.Optional[str], t.Tuple[t.Optional[str], ...]]]
    ]:
        if isinstance(source, str):
            source = self.environment.parse(source)
        return extract_from_ast(source, gettext_functions)

    def parse(self, parser: "Parser") -> t.Union[nodes.Node, t.List[nodes.Node]]:
        """Parse a translatable tag."""
        lineno = next(parser.stream).lineno

        context = None
        context_token = parser.stream.next_if("string")

        if context_token is not None:
            context = context_token.value

        # find all the variables referenced.  Additionally a variable can be
        # defined in the body of the trans block too, but this is checked at
        # a later state.
        plural_expr: t.Optional[nodes.Expr] = None
        plural_expr_assignment: t.Optional[nodes.Assign] = None
        num_called_num = False
        variables: t.Dict[str, nodes.Expr] = {}
        trimmed = None
        while parser.stream.current.type != "block_end":
            if variables:
                parser.stream.expect("comma")

            # skip colon for python compatibility
            if parser.stream.skip_if("colon"):
                break

            token = parser.stream.expect("name")
            if token.value in variables:
                parser.fail(
                    f"translatable variable {token.value!r} defined twice.",
                    token.lineno,
                    exc=TemplateAssertionError,
                )

            # expressions
            if parser.stream.current.type == "assign":
                next(parser.stream)
                variables[token.value] = var = parser.parse_expression()
            elif trimmed is None and token.value in ("trimmed", "notrimmed"):
                trimmed = token.value == "trimmed"
                continue
            else:
                variables[token.value] = var = nodes.Name(token.value, "load")

            if plural_expr is None:
                if isinstance(var, nodes.Call):
                    plural_expr = nodes.Name("_trans", "load")
                    variables[token.value] = plural_expr
                    plural_expr_assignment = nodes.Assign(
                        nodes.Name("_trans", "store"), var
                    )
                else:
                    plural_expr = var
                num_called_num = token.value == "num"

        parser.stream.expect("block_end")

        plural = None
        have_plural = False
        referenced = set()

        # now parse until endtrans or pluralize
        singular_names, singular = self._parse_block(parser, True)
        if singular_names:
            referenced.update(singular_names)
            if plural_expr is None:
                plural_expr = nodes.Name(singular_names[0], "load")
                num_called_num = singular_names[0] == "num"

        # if we have a pluralize block, we parse that too
        if parser.stream.current.test("name:pluralize"):
            have_plural = True
            next(parser.stream)
            if parser.stream.current.type != "block_end":
                token = parser.stream.expect("name")
                if token.value not in variables:
                    parser.fail(
                        f"unknown variable {token.value!r} for pluralization",
                        token.lineno,
                        exc=TemplateAssertionError,
                    )
                plural_expr = variables[token.value]
                num_called_num = token.value == "num"
            parser.stream.expect("block_end")
            plural_names, plural = self._parse_block(parser, False)
            next(parser.stream)
            referenced.update(plural_names)
        else:
            next(parser.stream)

        # register free names as simple name expressions
        for name in referenced:
            if name not in variables:
                variables[name] = nodes.Name(name, "load")

        if not have_plural:
            plural_expr = None
        elif plural_expr is None:
            parser.fail("pluralize without variables", lineno)

        if trimmed is None:
            trimmed = self.environment.policies["ext.i18n.trimmed"]
        if trimmed:
            singular = self._trim_whitespace(singular)
            if plural:
                plural = self._trim_whitespace(plural)

        node = self._make_node(
            singular,
            plural,
            context,
            variables,
            plural_expr,
            bool(referenced),
            num_called_num and have_plural,
        )
        node.set_lineno(lineno)
        if plural_expr_assignment is not None:
            return [plural_expr_assignment, node]
        else:
            return node

    def _trim_whitespace(self, string: str, _ws_re: t.Pattern[str] = _ws_re) -> str:
        return _ws_re.sub(" ", string.strip())

    def _parse_block(
        self, parser: "Parser", allow_pluralize: bool
    ) -> t.Tuple[t.List[str], str]:
        """Parse until the next block tag with a given name."""
        referenced = []
        buf = []

        while True:
            if parser.stream.current.type == "data":
                buf.append(parser.stream.current.value.replace("%", "%%"))
                next(parser.stream)
            elif parser.stream.current.type == "variable_begin":
                next(parser.stream)
                name = parser.stream.expect("name").value
                referenced.append(name)
                buf.append(f"%({name})s")
                parser.stream.expect("variable_end")
            elif parser.stream.current.type == "block_begin":
                next(parser.stream)
                if parser.stream.current.test("name:endtrans"):
                    break
                elif parser.stream.current.test("name:pluralize"):
                    if allow_pluralize:
                        break
                    parser.fail(
                        "a translatable section can have only one pluralize section"
                    )
                parser.fail(
                    "control structures in translatable sections are not allowed"
                )
            elif parser.stream.eos:
                parser.fail("unclosed translation block")
            else:
                raise RuntimeError("internal parser error")

        return referenced, concat(buf)

    def _make_node(
        self,
        singular: str,
        plural: t.Optional[str],
        context: t.Optional[str],
        variables: t.Dict[str, nodes.Expr],
        plural_expr: t.Optional[nodes.Expr],
        vars_referenced: bool,
        num_called_num: bool,
    ) -> nodes.Output:
        """Generates a useful node from the data provided."""
        newstyle = self.environment.newstyle_gettext  # type: ignore
        node: nodes.Expr

        # no variables referenced?  no need to escape for old style
        # gettext invocations only if there are vars.
        if not vars_referenced and not newstyle:
            singular = singular.replace("%%", "%")
            if plural:
                plural = plural.replace("%%", "%")

        func_name = "gettext"
        func_args: t.List[nodes.Expr] = [nodes.Const(singular)]

        if context is not None:
            func_args.insert(0, nodes.Const(context))
            func_name = f"p{func_name}"

        if plural_expr is not None:
            func_name = f"n{func_name}"
            func_args.extend((nodes.Const(plural), plural_expr))

        node = nodes.Call(nodes.Name(func_name, "load"), func_args, [], None, None)

        # in case newstyle gettext is used, the method is powerful
        # enough to handle the variable expansion and autoescape
        # handling itself
        if newstyle:
            for key, value in variables.items():
                # the function adds that later anyways in case num was
                # called num, so just skip it.
                if num_called_num and key == "num":
                    continue
                node.kwargs.append(nodes.Keyword(key, value))

        # otherwise do that here
        else:
            # mark the return value as safe if we are in an
            # environment with autoescaping turned on
            node = nodes.MarkSafeIfAutoescape(node)
            if variables:
                node = nodes.Mod(
                    node,
                    nodes.Dict(
                        [
                            nodes.Pair(nodes.Const(key), value)
                            for key, value in variables.items()
                        ]
                    ),
                )
        return nodes.Output([node])


class ExprStmtExtension(Extension):
    """Adds a `do` tag to Jinja that works like the print statement just
    that it doesn't print the return value.
    """

    tags = {"do"}

    def parse(self, parser: "Parser") -> nodes.ExprStmt:
        node = nodes.ExprStmt(lineno=next(parser.stream).lineno)
        node.node = parser.parse_tuple()
        return node


class LoopControlExtension(Extension):
    """Adds break and continue to the template engine."""

    tags = {"break", "continue"}

    def parse(self, parser: "Parser") -> t.Union[nodes.Break, nodes.Continue]:
        token = next(parser.stream)
        if token.value == "break":
            return nodes.Break(lineno=token.lineno)
        return nodes.Continue(lineno=token.lineno)


class DebugExtension(Extension):
    """A ``{% debug %}`` tag that dumps the available variables,
    filters, and tests.

    .. code-block:: html+jinja

        <pre>{% debug %}</pre>

    .. code-block:: text

        {'context': {'cycler': <class 'jinja2.utils.Cycler'>,
                     ...,
                     'namespace': <class 'jinja2.utils.Namespace'>},
         'filters': ['abs', 'attr', 'batch', 'capitalize', 'center', 'count', 'd',
                     ..., 'urlencode', 'urlize', 'wordcount', 'wordwrap', 'xmlattr'],
         'tests': ['!=', '<', '<=', '==', '>', '>=', 'callable', 'defined',
                   ..., 'odd', 'sameas', 'sequence', 'string', 'undefined', 'upper']}

    .. versionadded:: 2.11.0
    """

    tags = {"debug"}

    def parse(self, parser: "Parser") -> nodes.Output:
        lineno = parser.stream.expect("name:debug").lineno
        context = nodes.ContextReference()
        result = self.call_method("_render", [context], lineno=lineno)
        return nodes.Output([result], lineno=lineno)

    def _render(self, context: Context) -> str:
        result = {
            "context": context.get_all(),
            "filters": sorted(self.environment.filters.keys()),
            "tests": sorted(self.environment.tests.keys()),
        }

        # Set the depth since the intent is to show the top few names.
        return pprint.pformat(result, depth=3, compact=True)


def extract_from_ast(
    ast: nodes.Template,
    gettext_functions: t.Sequence[str] = GETTEXT_FUNCTIONS,
    babel_style: bool = True,
) -> t.Iterator[
    t.Tuple[int, str, t.Union[t.Optional[str], t.Tuple[t.Optional[str], ...]]]
]:
    """Extract localizable strings from the given template node.  Per
    default this function returns matches in babel style that means non string
    parameters as well as keyword arguments are returned as `None`.  This
    allows Babel to figure out what you really meant if you are using
    gettext functions that allow keyword arguments for placeholder expansion.
    If you don't want that behavior set the `babel_style` parameter to `False`
    which causes only strings to be returned and parameters are always stored
    in tuples.  As a consequence invalid gettext calls (calls without a single
    string parameter or string parameters after non-string parameters) are
    skipped.

    This example explains the behavior:

    >>> from jinja2 import Environment
    >>> env = Environment()
    >>> node = env.parse('{{ (_("foo"), _(), ngettext("foo", "bar", 42)) }}')
    >>> list(extract_from_ast(node))
    [(1, '_', 'foo'), (1, '_', ()), (1, 'ngettext', ('foo', 'bar', None))]
    >>> list(extract_from_ast(node, babel_style=False))
    [(1, '_', ('foo',)), (1, 'ngettext', ('foo', 'bar'))]

    For every string found this function yields a ``(lineno, function,
    message)`` tuple, where:

    * ``lineno`` is the number of the line on which the string was found,
    * ``function`` is the name of the ``gettext`` function used (if the
      string was extracted from embedded Python code), and
    *   ``message`` is the string, or a tuple of strings for functions
         with multiple string arguments.

    This extraction function operates on the AST and is because of that unable
    to extract any comments.  For comment support you have to use the babel
    extraction interface or extract comments yourself.
    """
    out: t.Union[t.Optional[str], t.Tuple[t.Optional[str], ...]]

    for node in ast.find_all(nodes.Call):
        if (
            not isinstance(node.node, nodes.Name)
            or node.node.name not in gettext_functions
        ):
            continue

        strings: t.List[t.Optional[str]] = []

        for arg in node.args:
            if isinstance(arg, nodes.Const) and isinstance(arg.value, str):
                strings.append(arg.value)
            else:
                strings.append(None)

        for _ in node.kwargs:
            strings.append(None)
        if node.dyn_args is not None:
            strings.append(None)
        if node.dyn_kwargs is not None:
            strings.append(None)

        if not babel_style:
            out = tuple(x for x in strings if x is not None)

            if not out:
                continue
        else:
            if len(strings) == 1:
                out = strings[0]
            else:
                out = tuple(strings)

        yield node.lineno, node.node.name, out


class _CommentFinder:
    """Helper class to find comments in a token stream.  Can only
    find comments for gettext calls forwards.  Once the comment
    from line 4 is found, a comment for line 1 will not return a
    usable value.
    """

    def __init__(
        self, tokens: t.Sequence[t.Tuple[int, str, str]], comment_tags: t.Sequence[str]
    ) -> None:
        self.tokens = tokens
        self.comment_tags = comment_tags
        self.offset = 0
        self.last_lineno = 0

    def find_backwards(self, offset: int) -> t.List[str]:
        try:
            for _, token_type, token_value in reversed(
                self.tokens[self.offset : offset]
            ):
                if token_type in ("comment", "linecomment"):
                    try:
                        prefix, comment = token_value.split(None, 1)
                    except ValueError:
                        continue
                    if prefix in self.comment_tags:
                        return [comment.rstrip()]
            return []
        finally:
            self.offset = offset

    def find_comments(self, lineno: int) -> t.List[str]:
        if not self.comment_tags or self.last_lineno > lineno:
            return []
        for idx, (token_lineno, _, _) in enumerate(self.tokens[self.offset :]):
            if token_lineno > lineno:
                return self.find_backwards(self.offset + idx)
        return self.find_backwards(len(self.tokens))


def babel_extract(
    fileobj: t.BinaryIO,
    keywords: t.Sequence[str],
    comment_tags: t.Sequence[str],
    options: t.Dict[str, t.Any],
) -> t.Iterator[
    t.Tuple[
        int, str, t.Union[t.Optional[str], t.Tuple[t.Optional[str], ...]], t.List[str]
    ]
]:
    """Babel extraction method for Jinja templates.

    .. versionchanged:: 2.3
       Basic support for translation comments was added.  If `comment_tags`
       is now set to a list of keywords for extraction, the extractor will
       try to find the best preceding comment that begins with one of the
       keywords.  For best results, make sure to not have more than one
       gettext call in one line of code and the matching comment in the
       same line or the line before.

    .. versionchanged:: 2.5.1
       The `newstyle_gettext` flag can be set to `True` to enable newstyle
       gettext calls.

    .. versionchanged:: 2.7
       A `silent` option can now be provided.  If set to `False` template
       syntax errors are propagated instead of being ignored.

    :param fileobj: the file-like object the messages should be extracted from
    :param keywords: a list of keywords (i.e. function names) that should be
                     recognized as translation functions
    :param comment_tags: a list of translator tags to search for and include
                         in the results.
    :param options: a dictionary of additional options (optional)
    :return: an iterator over ``(lineno, funcname, message, comments)`` tuples.
             (comments will be empty currently)
    """
    extensions: t.Dict[t.Type[Extension], None] = {}

    for extension_name in options.get("extensions", "").split(","):
        extension_name = extension_name.strip()

        if not extension_name:
            continue

        extensions[import_string(extension_name)] = None

    if InternationalizationExtension not in extensions:
        extensions[InternationalizationExtension] = None

    def getbool(options: t.Mapping[str, str], key: str, default: bool = False) -> bool:
        return options.get(key, str(default)).lower() in {"1", "on", "yes", "true"}

    silent = getbool(options, "silent", True)
    environment = Environment(
        options.get("block_start_string", defaults.BLOCK_START_STRING),
        options.get("block_end_string", defaults.BLOCK_END_STRING),
        options.get("variable_start_string", defaults.VARIABLE_START_STRING),
        options.get("variable_end_string", defaults.VARIABLE_END_STRING),
        options.get("comment_start_string", defaults.COMMENT_START_STRING),
        options.get("comment_end_string", defaults.COMMENT_END_STRING),
        options.get("line_statement_prefix") or defaults.LINE_STATEMENT_PREFIX,
        options.get("line_comment_prefix") or defaults.LINE_COMMENT_PREFIX,
        getbool(options, "trim_blocks", defaults.TRIM_BLOCKS),
        getbool(options, "lstrip_blocks", defaults.LSTRIP_BLOCKS),
        defaults.NEWLINE_SEQUENCE,
        getbool(options, "keep_trailing_newline", defaults.KEEP_TRAILING_NEWLINE),
        tuple(extensions),
        cache_size=0,
        auto_reload=False,
    )

    if getbool(options, "trimmed"):
        environment.policies["ext.i18n.trimmed"] = True
    if getbool(options, "newstyle_gettext"):
        environment.newstyle_gettext = True  # type: ignore

    source = fileobj.read().decode(options.get("encoding", "utf-8"))
    try:
        node = environment.parse(source)
        tokens = list(environment.lex(environment.preprocess(source)))
    except TemplateSyntaxError:
        if not silent:
            raise
        # skip templates with syntax errors
        return

    finder = _CommentFinder(tokens, comment_tags)
    for lineno, func, message in extract_from_ast(node, keywords):
        yield lineno, func, message, finder.find_comments(lineno)


#: nicer import names
i18n = InternationalizationExtension
do = ExprStmtExtension
loopcontrols = LoopControlExtension
debug = DebugExtension
