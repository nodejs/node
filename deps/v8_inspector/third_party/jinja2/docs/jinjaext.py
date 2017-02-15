# -*- coding: utf-8 -*-
"""
    Jinja Documentation Extensions
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Support for automatically documenting filters and tests.

    :copyright: Copyright 2008 by Armin Ronacher.
    :license: BSD.
"""
import collections
import os
import re
import inspect
import jinja2
from itertools import islice
from types import BuiltinFunctionType
from docutils import nodes
from docutils.statemachine import ViewList
from sphinx.ext.autodoc import prepare_docstring
from sphinx.application import TemplateBridge
from pygments.style import Style
from pygments.token import Keyword, Name, Comment, String, Error, \
     Number, Operator, Generic
from jinja2 import Environment, FileSystemLoader


def parse_rst(state, content_offset, doc):
    node = nodes.section()
    # hack around title style bookkeeping
    surrounding_title_styles = state.memo.title_styles
    surrounding_section_level = state.memo.section_level
    state.memo.title_styles = []
    state.memo.section_level = 0
    state.nested_parse(doc, content_offset, node, match_titles=1)
    state.memo.title_styles = surrounding_title_styles
    state.memo.section_level = surrounding_section_level
    return node.children


class JinjaStyle(Style):
    title = 'Jinja Style'
    default_style = ""
    styles = {
        Comment:                    'italic #aaaaaa',
        Comment.Preproc:            'noitalic #B11414',
        Comment.Special:            'italic #505050',

        Keyword:                    'bold #B80000',
        Keyword.Type:               '#808080',

        Operator.Word:              'bold #B80000',

        Name.Builtin:               '#333333',
        Name.Function:              '#333333',
        Name.Class:                 'bold #333333',
        Name.Namespace:             'bold #333333',
        Name.Entity:                'bold #363636',
        Name.Attribute:             '#686868',
        Name.Tag:                   'bold #686868',
        Name.Decorator:             '#686868',

        String:                     '#AA891C',
        Number:                     '#444444',

        Generic.Heading:            'bold #000080',
        Generic.Subheading:         'bold #800080',
        Generic.Deleted:            '#aa0000',
        Generic.Inserted:           '#00aa00',
        Generic.Error:              '#aa0000',
        Generic.Emph:               'italic',
        Generic.Strong:             'bold',
        Generic.Prompt:             '#555555',
        Generic.Output:             '#888888',
        Generic.Traceback:          '#aa0000',

        Error:                      '#F00 bg:#FAA'
    }


_sig_re = re.compile(r'^[a-zA-Z_][a-zA-Z0-9_]*(\(.*?\))')


def format_function(name, aliases, func):
    lines = inspect.getdoc(func).splitlines()
    signature = '()'
    if isinstance(func, BuiltinFunctionType):
        match = _sig_re.match(lines[0])
        if match is not None:
            del lines[:1 + bool(lines and not lines[0])]
            signature = match.group(1)
    else:
        try:
            argspec = inspect.getargspec(func)
            if getattr(func, 'environmentfilter', False) or \
               getattr(func, 'contextfilter', False) or \
               getattr(func, 'evalcontextfilter', False):
                del argspec[0][0]
            signature = inspect.formatargspec(*argspec)
        except:
            pass
    result = ['.. function:: %s%s' % (name, signature), '']
    result.extend('    ' + line for line in lines)
    if aliases:
        result.extend(('', '    :aliases: %s' % ', '.join(
                      '``%s``' % x for x in sorted(aliases))))
    return result


def dump_functions(mapping):
    def directive(dirname, arguments, options, content, lineno,
                      content_offset, block_text, state, state_machine):
        reverse_mapping = {}
        for name, func in mapping.items():
            reverse_mapping.setdefault(func, []).append(name)
        filters = []
        for func, names in reverse_mapping.items():
            aliases = sorted(names, key=lambda x: len(x))
            name = aliases.pop()
            filters.append((name, aliases, func))
        filters.sort()

        result = ViewList()
        for name, aliases, func in filters:
            for item in format_function(name, aliases, func):
                result.append(item, '<jinjaext>')

        node = nodes.paragraph()
        state.nested_parse(result, content_offset, node)
        return node.children
    return directive


from jinja2.defaults import DEFAULT_FILTERS, DEFAULT_TESTS
jinja_filters = dump_functions(DEFAULT_FILTERS)
jinja_tests = dump_functions(DEFAULT_TESTS)


def jinja_nodes(dirname, arguments, options, content, lineno,
                content_offset, block_text, state, state_machine):
    from jinja2.nodes import Node
    doc = ViewList()
    def walk(node, indent):
        p = ' ' * indent
        sig = ', '.join(node.fields)
        doc.append(p + '.. autoclass:: %s(%s)' % (node.__name__, sig), '')
        if node.abstract:
            members = []
            for key, name in node.__dict__.items():
                if not key.startswith('_') and \
                   not hasattr(node.__base__, key) and isinstance(name, collections.Callable):
                    members.append(key)
            if members:
                members.sort()
                doc.append('%s :members: %s' % (p, ', '.join(members)), '')
        if node.__base__ != object:
            doc.append('', '')
            doc.append('%s :Node type: :class:`%s`' %
                       (p, node.__base__.__name__), '')
        doc.append('', '')
        children = node.__subclasses__()
        children.sort(key=lambda x: x.__name__.lower())
        for child in children:
            walk(child, indent)
    walk(Node, 0)
    return parse_rst(state, content_offset, doc)


def inject_toc(app, doctree, docname):
    titleiter = iter(doctree.traverse(nodes.title))
    try:
        # skip first title, we are not interested in that one
        next(titleiter)
        title = next(titleiter)
        # and check if there is at least another title
        next(titleiter)
    except StopIteration:
        return
    tocnode = nodes.section('')
    tocnode['classes'].append('toc')
    toctitle = nodes.section('')
    toctitle['classes'].append('toctitle')
    toctitle.append(nodes.title(text='Table Of Contents'))
    tocnode.append(toctitle)
    tocnode += doctree.document.settings.env.get_toc_for(docname)[0][1]
    title.parent.insert(title.parent.children.index(title), tocnode)


def setup(app):
    app.add_directive('jinjafilters', jinja_filters, 0, (0, 0, 0))
    app.add_directive('jinjatests', jinja_tests, 0, (0, 0, 0))
    app.add_directive('jinjanodes', jinja_nodes, 0, (0, 0, 0))
    # uncomment for inline toc.  links are broken unfortunately
    ##app.connect('doctree-resolved', inject_toc)
