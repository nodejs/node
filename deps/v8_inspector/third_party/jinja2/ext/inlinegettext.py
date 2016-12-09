# -*- coding: utf-8 -*-
"""
    Inline Gettext
    ~~~~~~~~~~~~~~

    An example extension for Jinja2 that supports inline gettext calls.
    Requires the i18n extension to be loaded.

    :copyright: (c) 2009 by the Jinja Team.
    :license: BSD.
"""
import re
from jinja2.ext import Extension
from jinja2.lexer import Token, count_newlines
from jinja2.exceptions import TemplateSyntaxError


_outside_re = re.compile(r'\\?(gettext|_)\(')
_inside_re = re.compile(r'\\?[()]')


class InlineGettext(Extension):
    """This extension implements support for inline gettext blocks::

        <h1>_(Welcome)</h1>
        <p>_(This is a paragraph)</p>

    Requires the i18n extension to be loaded and configured.
    """

    def filter_stream(self, stream):
        paren_stack = 0

        for token in stream:
            if token.type is not 'data':
                yield token
                continue

            pos = 0
            lineno = token.lineno

            while 1:
                if not paren_stack:
                    match = _outside_re.search(token.value, pos)
                else:
                    match = _inside_re.search(token.value, pos)
                if match is None:
                    break
                new_pos = match.start()
                if new_pos > pos:
                    preval = token.value[pos:new_pos]
                    yield Token(lineno, 'data', preval)
                    lineno += count_newlines(preval)
                gtok = match.group()
                if gtok[0] == '\\':
                    yield Token(lineno, 'data', gtok[1:])
                elif not paren_stack:
                    yield Token(lineno, 'block_begin', None)
                    yield Token(lineno, 'name', 'trans')
                    yield Token(lineno, 'block_end', None)
                    paren_stack = 1
                else:
                    if gtok == '(' or paren_stack > 1:
                        yield Token(lineno, 'data', gtok)
                    paren_stack += gtok == ')' and -1 or 1
                    if not paren_stack:
                        yield Token(lineno, 'block_begin', None)
                        yield Token(lineno, 'name', 'endtrans')
                        yield Token(lineno, 'block_end', None)
                pos = match.end()

            if pos < len(token.value):
                yield Token(lineno, 'data', token.value[pos:])

        if paren_stack:
            raise TemplateSyntaxError('unclosed gettext expression',
                                      token.lineno, stream.name,
                                      stream.filename)
