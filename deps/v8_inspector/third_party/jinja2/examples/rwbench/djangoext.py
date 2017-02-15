# -*- coding: utf-8 -*-
from rwbench import ROOT
from os.path import join
from django.conf import settings
settings.configure(
    TEMPLATE_DIRS=(join(ROOT, 'django'),),
    TEMPLATE_LOADERS=(
        ('django.template.loaders.cached.Loader', (
            'django.template.loaders.filesystem.Loader',
        )),
    )
)
from django.template import loader as django_loader, Context as DjangoContext, \
     Node, NodeList, Variable, TokenParser
from django import template as django_template_module
from django.template import Library


# for django extensions.  We monkey patch our extensions in so that
# we don't have to initialize a more complex django setup.
django_extensions = django_template_module.Library()
django_template_module.builtins.append(django_extensions)


from rwbench import dateformat
django_extensions.filter(dateformat)


def var_or_none(x):
    if x is not None:
        return Variable(x)


# and more django extensions
@django_extensions.tag
def input_field(parser, token):
    p = TokenParser(token.contents)
    args = [p.value()]
    while p.more():
        args.append(p.value())
    return InputFieldNode(*args)


@django_extensions.tag
def textarea(parser, token):
    p = TokenParser(token.contents)
    args = [p.value()]
    while p.more():
        args.append(p.value())
    return TextareaNode(*args)


@django_extensions.tag
def form(parser, token):
    p = TokenParser(token.contents)
    args = []
    while p.more():
        args.append(p.value())
    body = parser.parse(('endform',))
    parser.delete_first_token()
    return FormNode(body, *args)


class InputFieldNode(Node):

    def __init__(self, name, type=None, value=None):
        self.name = var_or_none(name)
        self.type = var_or_none(type)
        self.value = var_or_none(value)

    def render(self, context):
        name = self.name.resolve(context)
        type = 'text'
        value = ''
        if self.type is not None:
            type = self.type.resolve(context)
        if self.value is not None:
            value = self.value.resolve(context)
        tmpl = django_loader.get_template('_input_field.html')
        return tmpl.render(DjangoContext({
            'name':     name,
            'type':     type,
            'value':    value
        }))


class TextareaNode(Node):

    def __init__(self, name, rows=None, cols=None, value=None):
        self.name = var_or_none(name)
        self.rows = var_or_none(rows)
        self.cols = var_or_none(cols)
        self.value = var_or_none(value)

    def render(self, context):
        name = self.name.resolve(context)
        rows = 10
        cols = 40
        value = ''
        if self.rows is not None:
            rows = int(self.rows.resolve(context))
        if self.cols is not None:
            cols = int(self.cols.resolve(context))
        if self.value is not None:
            value = self.value.resolve(context)
        tmpl = django_loader.get_template('_textarea.html')
        return tmpl.render(DjangoContext({
            'name':     name,
            'rows':     rows,
            'cols':     cols,
            'value':    value
        }))


class FormNode(Node):

    def __init__(self, body, action=None, method=None):
        self.body = body
        self.action = action
        self.method = method

    def render(self, context):
        body = self.body.render(context)
        action = ''
        method = 'post'
        if self.action is not None:
            action = self.action.resolve(context)
        if self.method is not None:
            method = self.method.resolve(context)
        tmpl = django_loader.get_template('_form.html')
        return tmpl.render(DjangoContext({
            'body':     body,
            'action':   action,
            'method':   method
        }))
