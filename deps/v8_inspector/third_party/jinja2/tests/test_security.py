# -*- coding: utf-8 -*-
"""
    jinja2.testsuite.security
    ~~~~~~~~~~~~~~~~~~~~~~~~~

    Checks the sandbox and other security features.

    :copyright: (c) 2010 by the Jinja Team.
    :license: BSD, see LICENSE for more details.
"""
import pytest

from jinja2 import Environment
from jinja2.sandbox import SandboxedEnvironment, \
     ImmutableSandboxedEnvironment, unsafe
from jinja2 import Markup, escape
from jinja2.exceptions import SecurityError, TemplateSyntaxError, \
     TemplateRuntimeError
from jinja2._compat import text_type


class PrivateStuff(object):

    def bar(self):
        return 23

    @unsafe
    def foo(self):
        return 42

    def __repr__(self):
        return 'PrivateStuff'


class PublicStuff(object):
    bar = lambda self: 23
    _foo = lambda self: 42

    def __repr__(self):
        return 'PublicStuff'


@pytest.mark.sandbox
class TestSandbox():

    def test_unsafe(self, env):
        env = SandboxedEnvironment()
        pytest.raises(SecurityError, env.from_string("{{ foo.foo() }}").render,
                      foo=PrivateStuff())
        assert env.from_string("{{ foo.bar() }}").render(foo=PrivateStuff()) == '23'

        pytest.raises(SecurityError,
                      env.from_string("{{ foo._foo() }}").render,
                      foo=PublicStuff())
        assert env.from_string("{{ foo.bar() }}").render(foo=PublicStuff()) == '23'
        assert env.from_string("{{ foo.__class__ }}").render(foo=42) == ''
        assert env.from_string("{{ foo.func_code }}").render(foo=lambda:None) == ''
        # security error comes from __class__ already.
        pytest.raises(SecurityError, env.from_string(
            "{{ foo.__class__.__subclasses__() }}").render, foo=42)

    def test_immutable_environment(self, env):
        env = ImmutableSandboxedEnvironment()
        pytest.raises(SecurityError, env.from_string(
            '{{ [].append(23) }}').render)
        pytest.raises(SecurityError, env.from_string(
            '{{ {1:2}.clear() }}').render)

    def test_restricted(self, env):
        env = SandboxedEnvironment()
        pytest.raises(TemplateSyntaxError, env.from_string,
                      "{% for item.attribute in seq %}...{% endfor %}")
        pytest.raises(TemplateSyntaxError, env.from_string,
                      "{% for foo, bar.baz in seq %}...{% endfor %}")

    def test_markup_operations(self, env):
        # adding two strings should escape the unsafe one
        unsafe = '<script type="application/x-some-script">alert("foo");</script>'
        safe = Markup('<em>username</em>')
        assert unsafe + safe == text_type(escape(unsafe)) + text_type(safe)

        # string interpolations are safe to use too
        assert Markup('<em>%s</em>') % '<bad user>' == \
            '<em>&lt;bad user&gt;</em>'
        assert Markup('<em>%(username)s</em>') % {
            'username': '<bad user>'
        } == '<em>&lt;bad user&gt;</em>'

        # an escaped object is markup too
        assert type(Markup('foo') + 'bar') is Markup

        # and it implements __html__ by returning itself
        x = Markup("foo")
        assert x.__html__() is x

        # it also knows how to treat __html__ objects
        class Foo(object):
            def __html__(self):
                return '<em>awesome</em>'

            def __unicode__(self):
                return 'awesome'
        assert Markup(Foo()) == '<em>awesome</em>'
        assert Markup('<strong>%s</strong>') % Foo() == \
            '<strong><em>awesome</em></strong>'

        # escaping and unescaping
        assert escape('"<>&\'') == '&#34;&lt;&gt;&amp;&#39;'
        assert Markup("<em>Foo &amp; Bar</em>").striptags() == "Foo & Bar"
        assert Markup("&lt;test&gt;").unescape() == "<test>"

    def test_template_data(self, env):
        env = Environment(autoescape=True)
        t = env.from_string('{% macro say_hello(name) %}'
                            '<p>Hello {{ name }}!</p>{% endmacro %}'
                            '{{ say_hello("<blink>foo</blink>") }}')
        escaped_out = '<p>Hello &lt;blink&gt;foo&lt;/blink&gt;!</p>'
        assert t.render() == escaped_out
        assert text_type(t.module) == escaped_out
        assert escape(t.module) == escaped_out
        assert t.module.say_hello('<blink>foo</blink>') == escaped_out
        assert escape(t.module.say_hello('<blink>foo</blink>')) == escaped_out

    def test_attr_filter(self, env):
        env = SandboxedEnvironment()
        tmpl = env.from_string('{{ cls|attr("__subclasses__")() }}')
        pytest.raises(SecurityError, tmpl.render, cls=int)

    def test_binary_operator_intercepting(self, env):
        def disable_op(left, right):
            raise TemplateRuntimeError('that operator so does not work')
        for expr, ctx, rv in ('1 + 2', {}, '3'), ('a + 2', {'a': 2}, '4'):
            env = SandboxedEnvironment()
            env.binop_table['+'] = disable_op
            t = env.from_string('{{ %s }}' % expr)
            assert t.render(ctx) == rv
            env.intercepted_binops = frozenset(['+'])
            t = env.from_string('{{ %s }}' % expr)
            try:
                t.render(ctx)
            except TemplateRuntimeError as e:
                pass
            else:
                assert False, 'expected runtime error'

    def test_unary_operator_intercepting(self, env):
        def disable_op(arg):
            raise TemplateRuntimeError('that operator so does not work')
        for expr, ctx, rv in ('-1', {}, '-1'), ('-a', {'a': 2}, '-2'):
            env = SandboxedEnvironment()
            env.unop_table['-'] = disable_op
            t = env.from_string('{{ %s }}' % expr)
            assert t.render(ctx) == rv
            env.intercepted_unops = frozenset(['-'])
            t = env.from_string('{{ %s }}' % expr)
            try:
                t.render(ctx)
            except TemplateRuntimeError as e:
                pass
            else:
                assert False, 'expected runtime error'
