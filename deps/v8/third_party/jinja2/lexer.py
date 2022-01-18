# -*- coding: utf-8 -*-
"""Implements a Jinja / Python combination lexer. The ``Lexer`` class
is used to do some preprocessing. It filters out invalid operators like
the bitshift operators we don't allow in templates. It separates
template code and python code in expressions.
"""
import re
from ast import literal_eval
from collections import deque
from operator import itemgetter

from ._compat import implements_iterator
from ._compat import intern
from ._compat import iteritems
from ._compat import text_type
from .exceptions import TemplateSyntaxError
from .utils import LRUCache

# cache for the lexers. Exists in order to be able to have multiple
# environments with the same lexer
_lexer_cache = LRUCache(50)

# static regular expressions
whitespace_re = re.compile(r"\s+", re.U)
newline_re = re.compile(r"(\r\n|\r|\n)")
string_re = re.compile(
    r"('([^'\\]*(?:\\.[^'\\]*)*)'" r'|"([^"\\]*(?:\\.[^"\\]*)*)")', re.S
)
integer_re = re.compile(r"(\d+_)*\d+")
float_re = re.compile(
    r"""
    (?<!\.)  # doesn't start with a .
    (\d+_)*\d+  # digits, possibly _ separated
    (
        (\.(\d+_)*\d+)?  # optional fractional part
        e[+\-]?(\d+_)*\d+  # exponent part
    |
        \.(\d+_)*\d+  # required fractional part
    )
    """,
    re.IGNORECASE | re.VERBOSE,
)

try:
    # check if this Python supports Unicode identifiers
    compile("föö", "<unknown>", "eval")
except SyntaxError:
    # Python 2, no Unicode support, use ASCII identifiers
    name_re = re.compile(r"[a-zA-Z_][a-zA-Z0-9_]*")
    check_ident = False
else:
    # Unicode support, import generated re pattern and set flag to use
    # str.isidentifier to validate during lexing.
    from ._identifier import pattern as name_re

    check_ident = True

# internal the tokens and keep references to them
TOKEN_ADD = intern("add")
TOKEN_ASSIGN = intern("assign")
TOKEN_COLON = intern("colon")
TOKEN_COMMA = intern("comma")
TOKEN_DIV = intern("div")
TOKEN_DOT = intern("dot")
TOKEN_EQ = intern("eq")
TOKEN_FLOORDIV = intern("floordiv")
TOKEN_GT = intern("gt")
TOKEN_GTEQ = intern("gteq")
TOKEN_LBRACE = intern("lbrace")
TOKEN_LBRACKET = intern("lbracket")
TOKEN_LPAREN = intern("lparen")
TOKEN_LT = intern("lt")
TOKEN_LTEQ = intern("lteq")
TOKEN_MOD = intern("mod")
TOKEN_MUL = intern("mul")
TOKEN_NE = intern("ne")
TOKEN_PIPE = intern("pipe")
TOKEN_POW = intern("pow")
TOKEN_RBRACE = intern("rbrace")
TOKEN_RBRACKET = intern("rbracket")
TOKEN_RPAREN = intern("rparen")
TOKEN_SEMICOLON = intern("semicolon")
TOKEN_SUB = intern("sub")
TOKEN_TILDE = intern("tilde")
TOKEN_WHITESPACE = intern("whitespace")
TOKEN_FLOAT = intern("float")
TOKEN_INTEGER = intern("integer")
TOKEN_NAME = intern("name")
TOKEN_STRING = intern("string")
TOKEN_OPERATOR = intern("operator")
TOKEN_BLOCK_BEGIN = intern("block_begin")
TOKEN_BLOCK_END = intern("block_end")
TOKEN_VARIABLE_BEGIN = intern("variable_begin")
TOKEN_VARIABLE_END = intern("variable_end")
TOKEN_RAW_BEGIN = intern("raw_begin")
TOKEN_RAW_END = intern("raw_end")
TOKEN_COMMENT_BEGIN = intern("comment_begin")
TOKEN_COMMENT_END = intern("comment_end")
TOKEN_COMMENT = intern("comment")
TOKEN_LINESTATEMENT_BEGIN = intern("linestatement_begin")
TOKEN_LINESTATEMENT_END = intern("linestatement_end")
TOKEN_LINECOMMENT_BEGIN = intern("linecomment_begin")
TOKEN_LINECOMMENT_END = intern("linecomment_end")
TOKEN_LINECOMMENT = intern("linecomment")
TOKEN_DATA = intern("data")
TOKEN_INITIAL = intern("initial")
TOKEN_EOF = intern("eof")

# bind operators to token types
operators = {
    "+": TOKEN_ADD,
    "-": TOKEN_SUB,
    "/": TOKEN_DIV,
    "//": TOKEN_FLOORDIV,
    "*": TOKEN_MUL,
    "%": TOKEN_MOD,
    "**": TOKEN_POW,
    "~": TOKEN_TILDE,
    "[": TOKEN_LBRACKET,
    "]": TOKEN_RBRACKET,
    "(": TOKEN_LPAREN,
    ")": TOKEN_RPAREN,
    "{": TOKEN_LBRACE,
    "}": TOKEN_RBRACE,
    "==": TOKEN_EQ,
    "!=": TOKEN_NE,
    ">": TOKEN_GT,
    ">=": TOKEN_GTEQ,
    "<": TOKEN_LT,
    "<=": TOKEN_LTEQ,
    "=": TOKEN_ASSIGN,
    ".": TOKEN_DOT,
    ":": TOKEN_COLON,
    "|": TOKEN_PIPE,
    ",": TOKEN_COMMA,
    ";": TOKEN_SEMICOLON,
}

reverse_operators = dict([(v, k) for k, v in iteritems(operators)])
assert len(operators) == len(reverse_operators), "operators dropped"
operator_re = re.compile(
    "(%s)" % "|".join(re.escape(x) for x in sorted(operators, key=lambda x: -len(x)))
)

ignored_tokens = frozenset(
    [
        TOKEN_COMMENT_BEGIN,
        TOKEN_COMMENT,
        TOKEN_COMMENT_END,
        TOKEN_WHITESPACE,
        TOKEN_LINECOMMENT_BEGIN,
        TOKEN_LINECOMMENT_END,
        TOKEN_LINECOMMENT,
    ]
)
ignore_if_empty = frozenset(
    [TOKEN_WHITESPACE, TOKEN_DATA, TOKEN_COMMENT, TOKEN_LINECOMMENT]
)


def _describe_token_type(token_type):
    if token_type in reverse_operators:
        return reverse_operators[token_type]
    return {
        TOKEN_COMMENT_BEGIN: "begin of comment",
        TOKEN_COMMENT_END: "end of comment",
        TOKEN_COMMENT: "comment",
        TOKEN_LINECOMMENT: "comment",
        TOKEN_BLOCK_BEGIN: "begin of statement block",
        TOKEN_BLOCK_END: "end of statement block",
        TOKEN_VARIABLE_BEGIN: "begin of print statement",
        TOKEN_VARIABLE_END: "end of print statement",
        TOKEN_LINESTATEMENT_BEGIN: "begin of line statement",
        TOKEN_LINESTATEMENT_END: "end of line statement",
        TOKEN_DATA: "template data / text",
        TOKEN_EOF: "end of template",
    }.get(token_type, token_type)


def describe_token(token):
    """Returns a description of the token."""
    if token.type == TOKEN_NAME:
        return token.value
    return _describe_token_type(token.type)


def describe_token_expr(expr):
    """Like `describe_token` but for token expressions."""
    if ":" in expr:
        type, value = expr.split(":", 1)
        if type == TOKEN_NAME:
            return value
    else:
        type = expr
    return _describe_token_type(type)


def count_newlines(value):
    """Count the number of newline characters in the string.  This is
    useful for extensions that filter a stream.
    """
    return len(newline_re.findall(value))


def compile_rules(environment):
    """Compiles all the rules from the environment into a list of rules."""
    e = re.escape
    rules = [
        (
            len(environment.comment_start_string),
            TOKEN_COMMENT_BEGIN,
            e(environment.comment_start_string),
        ),
        (
            len(environment.block_start_string),
            TOKEN_BLOCK_BEGIN,
            e(environment.block_start_string),
        ),
        (
            len(environment.variable_start_string),
            TOKEN_VARIABLE_BEGIN,
            e(environment.variable_start_string),
        ),
    ]

    if environment.line_statement_prefix is not None:
        rules.append(
            (
                len(environment.line_statement_prefix),
                TOKEN_LINESTATEMENT_BEGIN,
                r"^[ \t\v]*" + e(environment.line_statement_prefix),
            )
        )
    if environment.line_comment_prefix is not None:
        rules.append(
            (
                len(environment.line_comment_prefix),
                TOKEN_LINECOMMENT_BEGIN,
                r"(?:^|(?<=\S))[^\S\r\n]*" + e(environment.line_comment_prefix),
            )
        )

    return [x[1:] for x in sorted(rules, reverse=True)]


class Failure(object):
    """Class that raises a `TemplateSyntaxError` if called.
    Used by the `Lexer` to specify known errors.
    """

    def __init__(self, message, cls=TemplateSyntaxError):
        self.message = message
        self.error_class = cls

    def __call__(self, lineno, filename):
        raise self.error_class(self.message, lineno, filename)


class Token(tuple):
    """Token class."""

    __slots__ = ()
    lineno, type, value = (property(itemgetter(x)) for x in range(3))

    def __new__(cls, lineno, type, value):
        return tuple.__new__(cls, (lineno, intern(str(type)), value))

    def __str__(self):
        if self.type in reverse_operators:
            return reverse_operators[self.type]
        elif self.type == "name":
            return self.value
        return self.type

    def test(self, expr):
        """Test a token against a token expression.  This can either be a
        token type or ``'token_type:token_value'``.  This can only test
        against string values and types.
        """
        # here we do a regular string equality check as test_any is usually
        # passed an iterable of not interned strings.
        if self.type == expr:
            return True
        elif ":" in expr:
            return expr.split(":", 1) == [self.type, self.value]
        return False

    def test_any(self, *iterable):
        """Test against multiple token expressions."""
        for expr in iterable:
            if self.test(expr):
                return True
        return False

    def __repr__(self):
        return "Token(%r, %r, %r)" % (self.lineno, self.type, self.value)


@implements_iterator
class TokenStreamIterator(object):
    """The iterator for tokenstreams.  Iterate over the stream
    until the eof token is reached.
    """

    def __init__(self, stream):
        self.stream = stream

    def __iter__(self):
        return self

    def __next__(self):
        token = self.stream.current
        if token.type is TOKEN_EOF:
            self.stream.close()
            raise StopIteration()
        next(self.stream)
        return token


@implements_iterator
class TokenStream(object):
    """A token stream is an iterable that yields :class:`Token`\\s.  The
    parser however does not iterate over it but calls :meth:`next` to go
    one token ahead.  The current active token is stored as :attr:`current`.
    """

    def __init__(self, generator, name, filename):
        self._iter = iter(generator)
        self._pushed = deque()
        self.name = name
        self.filename = filename
        self.closed = False
        self.current = Token(1, TOKEN_INITIAL, "")
        next(self)

    def __iter__(self):
        return TokenStreamIterator(self)

    def __bool__(self):
        return bool(self._pushed) or self.current.type is not TOKEN_EOF

    __nonzero__ = __bool__  # py2

    @property
    def eos(self):
        """Are we at the end of the stream?"""
        return not self

    def push(self, token):
        """Push a token back to the stream."""
        self._pushed.append(token)

    def look(self):
        """Look at the next token."""
        old_token = next(self)
        result = self.current
        self.push(result)
        self.current = old_token
        return result

    def skip(self, n=1):
        """Got n tokens ahead."""
        for _ in range(n):
            next(self)

    def next_if(self, expr):
        """Perform the token test and return the token if it matched.
        Otherwise the return value is `None`.
        """
        if self.current.test(expr):
            return next(self)

    def skip_if(self, expr):
        """Like :meth:`next_if` but only returns `True` or `False`."""
        return self.next_if(expr) is not None

    def __next__(self):
        """Go one token ahead and return the old one.

        Use the built-in :func:`next` instead of calling this directly.
        """
        rv = self.current
        if self._pushed:
            self.current = self._pushed.popleft()
        elif self.current.type is not TOKEN_EOF:
            try:
                self.current = next(self._iter)
            except StopIteration:
                self.close()
        return rv

    def close(self):
        """Close the stream."""
        self.current = Token(self.current.lineno, TOKEN_EOF, "")
        self._iter = None
        self.closed = True

    def expect(self, expr):
        """Expect a given token type and return it.  This accepts the same
        argument as :meth:`jinja2.lexer.Token.test`.
        """
        if not self.current.test(expr):
            expr = describe_token_expr(expr)
            if self.current.type is TOKEN_EOF:
                raise TemplateSyntaxError(
                    "unexpected end of template, expected %r." % expr,
                    self.current.lineno,
                    self.name,
                    self.filename,
                )
            raise TemplateSyntaxError(
                "expected token %r, got %r" % (expr, describe_token(self.current)),
                self.current.lineno,
                self.name,
                self.filename,
            )
        try:
            return self.current
        finally:
            next(self)


def get_lexer(environment):
    """Return a lexer which is probably cached."""
    key = (
        environment.block_start_string,
        environment.block_end_string,
        environment.variable_start_string,
        environment.variable_end_string,
        environment.comment_start_string,
        environment.comment_end_string,
        environment.line_statement_prefix,
        environment.line_comment_prefix,
        environment.trim_blocks,
        environment.lstrip_blocks,
        environment.newline_sequence,
        environment.keep_trailing_newline,
    )
    lexer = _lexer_cache.get(key)
    if lexer is None:
        lexer = Lexer(environment)
        _lexer_cache[key] = lexer
    return lexer


class OptionalLStrip(tuple):
    """A special tuple for marking a point in the state that can have
    lstrip applied.
    """

    __slots__ = ()

    # Even though it looks like a no-op, creating instances fails
    # without this.
    def __new__(cls, *members, **kwargs):
        return super(OptionalLStrip, cls).__new__(cls, members)


class Lexer(object):
    """Class that implements a lexer for a given environment. Automatically
    created by the environment class, usually you don't have to do that.

    Note that the lexer is not automatically bound to an environment.
    Multiple environments can share the same lexer.
    """

    def __init__(self, environment):
        # shortcuts
        e = re.escape

        def c(x):
            return re.compile(x, re.M | re.S)

        # lexing rules for tags
        tag_rules = [
            (whitespace_re, TOKEN_WHITESPACE, None),
            (float_re, TOKEN_FLOAT, None),
            (integer_re, TOKEN_INTEGER, None),
            (name_re, TOKEN_NAME, None),
            (string_re, TOKEN_STRING, None),
            (operator_re, TOKEN_OPERATOR, None),
        ]

        # assemble the root lexing rule. because "|" is ungreedy
        # we have to sort by length so that the lexer continues working
        # as expected when we have parsing rules like <% for block and
        # <%= for variables. (if someone wants asp like syntax)
        # variables are just part of the rules if variable processing
        # is required.
        root_tag_rules = compile_rules(environment)

        # block suffix if trimming is enabled
        block_suffix_re = environment.trim_blocks and "\\n?" or ""

        # If lstrip is enabled, it should not be applied if there is any
        # non-whitespace between the newline and block.
        self.lstrip_unless_re = c(r"[^ \t]") if environment.lstrip_blocks else None

        self.newline_sequence = environment.newline_sequence
        self.keep_trailing_newline = environment.keep_trailing_newline

        # global lexing rules
        self.rules = {
            "root": [
                # directives
                (
                    c(
                        "(.*?)(?:%s)"
                        % "|".join(
                            [
                                r"(?P<raw_begin>%s(\-|\+|)\s*raw\s*(?:\-%s\s*|%s))"
                                % (
                                    e(environment.block_start_string),
                                    e(environment.block_end_string),
                                    e(environment.block_end_string),
                                )
                            ]
                            + [
                                r"(?P<%s>%s(\-|\+|))" % (n, r)
                                for n, r in root_tag_rules
                            ]
                        )
                    ),
                    OptionalLStrip(TOKEN_DATA, "#bygroup"),
                    "#bygroup",
                ),
                # data
                (c(".+"), TOKEN_DATA, None),
            ],
            # comments
            TOKEN_COMMENT_BEGIN: [
                (
                    c(
                        r"(.*?)((?:\-%s\s*|%s)%s)"
                        % (
                            e(environment.comment_end_string),
                            e(environment.comment_end_string),
                            block_suffix_re,
                        )
                    ),
                    (TOKEN_COMMENT, TOKEN_COMMENT_END),
                    "#pop",
                ),
                (c("(.)"), (Failure("Missing end of comment tag"),), None),
            ],
            # blocks
            TOKEN_BLOCK_BEGIN: [
                (
                    c(
                        r"(?:\-%s\s*|%s)%s"
                        % (
                            e(environment.block_end_string),
                            e(environment.block_end_string),
                            block_suffix_re,
                        )
                    ),
                    TOKEN_BLOCK_END,
                    "#pop",
                ),
            ]
            + tag_rules,
            # variables
            TOKEN_VARIABLE_BEGIN: [
                (
                    c(
                        r"\-%s\s*|%s"
                        % (
                            e(environment.variable_end_string),
                            e(environment.variable_end_string),
                        )
                    ),
                    TOKEN_VARIABLE_END,
                    "#pop",
                )
            ]
            + tag_rules,
            # raw block
            TOKEN_RAW_BEGIN: [
                (
                    c(
                        r"(.*?)((?:%s(\-|\+|))\s*endraw\s*(?:\-%s\s*|%s%s))"
                        % (
                            e(environment.block_start_string),
                            e(environment.block_end_string),
                            e(environment.block_end_string),
                            block_suffix_re,
                        )
                    ),
                    OptionalLStrip(TOKEN_DATA, TOKEN_RAW_END),
                    "#pop",
                ),
                (c("(.)"), (Failure("Missing end of raw directive"),), None),
            ],
            # line statements
            TOKEN_LINESTATEMENT_BEGIN: [
                (c(r"\s*(\n|$)"), TOKEN_LINESTATEMENT_END, "#pop")
            ]
            + tag_rules,
            # line comments
            TOKEN_LINECOMMENT_BEGIN: [
                (
                    c(r"(.*?)()(?=\n|$)"),
                    (TOKEN_LINECOMMENT, TOKEN_LINECOMMENT_END),
                    "#pop",
                )
            ],
        }

    def _normalize_newlines(self, value):
        """Called for strings and template data to normalize it to unicode."""
        return newline_re.sub(self.newline_sequence, value)

    def tokenize(self, source, name=None, filename=None, state=None):
        """Calls tokeniter + tokenize and wraps it in a token stream."""
        stream = self.tokeniter(source, name, filename, state)
        return TokenStream(self.wrap(stream, name, filename), name, filename)

    def wrap(self, stream, name=None, filename=None):
        """This is called with the stream as returned by `tokenize` and wraps
        every token in a :class:`Token` and converts the value.
        """
        for lineno, token, value in stream:
            if token in ignored_tokens:
                continue
            elif token == TOKEN_LINESTATEMENT_BEGIN:
                token = TOKEN_BLOCK_BEGIN
            elif token == TOKEN_LINESTATEMENT_END:
                token = TOKEN_BLOCK_END
            # we are not interested in those tokens in the parser
            elif token in (TOKEN_RAW_BEGIN, TOKEN_RAW_END):
                continue
            elif token == TOKEN_DATA:
                value = self._normalize_newlines(value)
            elif token == "keyword":
                token = value
            elif token == TOKEN_NAME:
                value = str(value)
                if check_ident and not value.isidentifier():
                    raise TemplateSyntaxError(
                        "Invalid character in identifier", lineno, name, filename
                    )
            elif token == TOKEN_STRING:
                # try to unescape string
                try:
                    value = (
                        self._normalize_newlines(value[1:-1])
                        .encode("ascii", "backslashreplace")
                        .decode("unicode-escape")
                    )
                except Exception as e:
                    msg = str(e).split(":")[-1].strip()
                    raise TemplateSyntaxError(msg, lineno, name, filename)
            elif token == TOKEN_INTEGER:
                value = int(value.replace("_", ""))
            elif token == TOKEN_FLOAT:
                # remove all "_" first to support more Python versions
                value = literal_eval(value.replace("_", ""))
            elif token == TOKEN_OPERATOR:
                token = operators[value]
            yield Token(lineno, token, value)

    def tokeniter(self, source, name, filename=None, state=None):
        """This method tokenizes the text and returns the tokens in a
        generator.  Use this method if you just want to tokenize a template.
        """
        source = text_type(source)
        lines = source.splitlines()
        if self.keep_trailing_newline and source:
            for newline in ("\r\n", "\r", "\n"):
                if source.endswith(newline):
                    lines.append("")
                    break
        source = "\n".join(lines)
        pos = 0
        lineno = 1
        stack = ["root"]
        if state is not None and state != "root":
            assert state in ("variable", "block"), "invalid state"
            stack.append(state + "_begin")
        statetokens = self.rules[stack[-1]]
        source_length = len(source)
        balancing_stack = []
        lstrip_unless_re = self.lstrip_unless_re
        newlines_stripped = 0
        line_starting = True

        while 1:
            # tokenizer loop
            for regex, tokens, new_state in statetokens:
                m = regex.match(source, pos)
                # if no match we try again with the next rule
                if m is None:
                    continue

                # we only match blocks and variables if braces / parentheses
                # are balanced. continue parsing with the lower rule which
                # is the operator rule. do this only if the end tags look
                # like operators
                if balancing_stack and tokens in (
                    TOKEN_VARIABLE_END,
                    TOKEN_BLOCK_END,
                    TOKEN_LINESTATEMENT_END,
                ):
                    continue

                # tuples support more options
                if isinstance(tokens, tuple):
                    groups = m.groups()

                    if isinstance(tokens, OptionalLStrip):
                        # Rule supports lstrip. Match will look like
                        # text, block type, whitespace control, type, control, ...
                        text = groups[0]

                        # Skipping the text and first type, every other group is the
                        # whitespace control for each type. One of the groups will be
                        # -, +, or empty string instead of None.
                        strip_sign = next(g for g in groups[2::2] if g is not None)

                        if strip_sign == "-":
                            # Strip all whitespace between the text and the tag.
                            stripped = text.rstrip()
                            newlines_stripped = text[len(stripped) :].count("\n")
                            groups = (stripped,) + groups[1:]
                        elif (
                            # Not marked for preserving whitespace.
                            strip_sign != "+"
                            # lstrip is enabled.
                            and lstrip_unless_re is not None
                            # Not a variable expression.
                            and not m.groupdict().get(TOKEN_VARIABLE_BEGIN)
                        ):
                            # The start of text between the last newline and the tag.
                            l_pos = text.rfind("\n") + 1
                            if l_pos > 0 or line_starting:
                                # If there's only whitespace between the newline and the
                                # tag, strip it.
                                if not lstrip_unless_re.search(text, l_pos):
                                    groups = (text[:l_pos],) + groups[1:]

                    for idx, token in enumerate(tokens):
                        # failure group
                        if token.__class__ is Failure:
                            raise token(lineno, filename)
                        # bygroup is a bit more complex, in that case we
                        # yield for the current token the first named
                        # group that matched
                        elif token == "#bygroup":
                            for key, value in iteritems(m.groupdict()):
                                if value is not None:
                                    yield lineno, key, value
                                    lineno += value.count("\n")
                                    break
                            else:
                                raise RuntimeError(
                                    "%r wanted to resolve "
                                    "the token dynamically"
                                    " but no group matched" % regex
                                )
                        # normal group
                        else:
                            data = groups[idx]
                            if data or token not in ignore_if_empty:
                                yield lineno, token, data
                            lineno += data.count("\n") + newlines_stripped
                            newlines_stripped = 0

                # strings as token just are yielded as it.
                else:
                    data = m.group()
                    # update brace/parentheses balance
                    if tokens == TOKEN_OPERATOR:
                        if data == "{":
                            balancing_stack.append("}")
                        elif data == "(":
                            balancing_stack.append(")")
                        elif data == "[":
                            balancing_stack.append("]")
                        elif data in ("}", ")", "]"):
                            if not balancing_stack:
                                raise TemplateSyntaxError(
                                    "unexpected '%s'" % data, lineno, name, filename
                                )
                            expected_op = balancing_stack.pop()
                            if expected_op != data:
                                raise TemplateSyntaxError(
                                    "unexpected '%s', "
                                    "expected '%s'" % (data, expected_op),
                                    lineno,
                                    name,
                                    filename,
                                )
                    # yield items
                    if data or tokens not in ignore_if_empty:
                        yield lineno, tokens, data
                    lineno += data.count("\n")

                line_starting = m.group()[-1:] == "\n"

                # fetch new position into new variable so that we can check
                # if there is a internal parsing error which would result
                # in an infinite loop
                pos2 = m.end()

                # handle state changes
                if new_state is not None:
                    # remove the uppermost state
                    if new_state == "#pop":
                        stack.pop()
                    # resolve the new state by group checking
                    elif new_state == "#bygroup":
                        for key, value in iteritems(m.groupdict()):
                            if value is not None:
                                stack.append(key)
                                break
                        else:
                            raise RuntimeError(
                                "%r wanted to resolve the "
                                "new state dynamically but"
                                " no group matched" % regex
                            )
                    # direct state name given
                    else:
                        stack.append(new_state)
                    statetokens = self.rules[stack[-1]]
                # we are still at the same position and no stack change.
                # this means a loop without break condition, avoid that and
                # raise error
                elif pos2 == pos:
                    raise RuntimeError(
                        "%r yielded empty string without stack change" % regex
                    )
                # publish new function and start again
                pos = pos2
                break
            # if loop terminated without break we haven't found a single match
            # either we are at the end of the file or we have a problem
            else:
                # end of text
                if pos >= source_length:
                    return
                # something went wrong
                raise TemplateSyntaxError(
                    "unexpected char %r at %d" % (source[pos], pos),
                    lineno,
                    name,
                    filename,
                )
