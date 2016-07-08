# -*- coding: utf-8 -*-
"""
    jinja2.testsuite.regression
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Tests corner cases and bugs.

    :copyright: (c) 2010 by the Jinja Team.
    :license: BSD, see LICENSE for more details.
"""
import pytest

from jinja2 import Template, Environment, DictLoader, TemplateSyntaxError, \
     TemplateNotFound, PrefixLoader
from jinja2._compat import text_type


@pytest.mark.regression
class TestCorner():

    def test_assigned_scoping(self, env):
        t = env.from_string('''
        {%- for item in (1, 2, 3, 4) -%}
            [{{ item }}]
        {%- endfor %}
        {{- item -}}
        ''')
        assert t.render(item=42) == '[1][2][3][4]42'

        t = env.from_string('''
        {%- for item in (1, 2, 3, 4) -%}
            [{{ item }}]
        {%- endfor %}
        {%- set item = 42 %}
        {{- item -}}
        ''')
        assert t.render() == '[1][2][3][4]42'

        t = env.from_string('''
        {%- set item = 42 %}
        {%- for item in (1, 2, 3, 4) -%}
            [{{ item }}]
        {%- endfor %}
        {{- item -}}
        ''')
        assert t.render() == '[1][2][3][4]42'

    def test_closure_scoping(self, env):
        t = env.from_string('''
        {%- set wrapper = "<FOO>" %}
        {%- for item in (1, 2, 3, 4) %}
            {%- macro wrapper() %}[{{ item }}]{% endmacro %}
            {{- wrapper() }}
        {%- endfor %}
        {{- wrapper -}}
        ''')
        assert t.render() == '[1][2][3][4]<FOO>'

        t = env.from_string('''
        {%- for item in (1, 2, 3, 4) %}
            {%- macro wrapper() %}[{{ item }}]{% endmacro %}
            {{- wrapper() }}
        {%- endfor %}
        {%- set wrapper = "<FOO>" %}
        {{- wrapper -}}
        ''')
        assert t.render() == '[1][2][3][4]<FOO>'

        t = env.from_string('''
        {%- for item in (1, 2, 3, 4) %}
            {%- macro wrapper() %}[{{ item }}]{% endmacro %}
            {{- wrapper() }}
        {%- endfor %}
        {{- wrapper -}}
        ''')
        assert t.render(wrapper=23) == '[1][2][3][4]23'


@pytest.mark.regression
class TestBug():

    def test_keyword_folding(self, env):
        env = Environment()
        env.filters['testing'] = lambda value, some: value + some
        assert env.from_string("{{ 'test'|testing(some='stuff') }}") \
            .render() == 'teststuff'

    def test_extends_output_bugs(self, env):
        env = Environment(loader=DictLoader({
            'parent.html': '(({% block title %}{% endblock %}))'
        }))

        t = env.from_string(
            '{% if expr %}{% extends "parent.html" %}{% endif %}'
            '[[{% block title %}title{% endblock %}]]'
            '{% for item in [1, 2, 3] %}({{ item }}){% endfor %}'
        )
        assert t.render(expr=False) == '[[title]](1)(2)(3)'
        assert t.render(expr=True) == '((title))'

    def test_urlize_filter_escaping(self, env):
        tmpl = env.from_string('{{ "http://www.example.org/<foo"|urlize }}')
        assert tmpl.render() == '<a href="http://www.example.org/&lt;foo">'\
            'http://www.example.org/&lt;foo</a>'

    def test_loop_call_loop(self, env):
        tmpl = env.from_string('''

        {% macro test() %}
            {{ caller() }}
        {% endmacro %}

        {% for num1 in range(5) %}
            {% call test() %}
                {% for num2 in range(10) %}
                    {{ loop.index }}
                {% endfor %}
            {% endcall %}
        {% endfor %}

        ''')

        assert tmpl.render().split() \
            == [text_type(x) for x in range(1, 11)] * 5

    def test_weird_inline_comment(self, env):
        env = Environment(line_statement_prefix='%')
        pytest.raises(TemplateSyntaxError, env.from_string,
                      '% for item in seq {# missing #}\n...% endfor')

    def test_old_macro_loop_scoping_bug(self, env):
        tmpl = env.from_string('{% for i in (1, 2) %}{{ i }}{% endfor %}'
                               '{% macro i() %}3{% endmacro %}{{ i() }}')
        assert tmpl.render() == '123'

    def test_partial_conditional_assignments(self, env):
        tmpl = env.from_string('{% if b %}{% set a = 42 %}{% endif %}{{ a }}')
        assert tmpl.render(a=23) == '23'
        assert tmpl.render(b=True) == '42'

    def test_stacked_locals_scoping_bug(self, env):
        env = Environment(line_statement_prefix='#')
        t = env.from_string('''\
# for j in [1, 2]:
#   set x = 1
#   for i in [1, 2]:
#     print x
#     if i % 2 == 0:
#       set x = x + 1
#     endif
#   endfor
# endfor
# if a
#   print 'A'
# elif b
#   print 'B'
# elif c == d
#   print 'C'
# else
#   print 'D'
# endif
    ''')
        assert t.render(a=0, b=False, c=42, d=42.0) == '1111C'

    def test_stacked_locals_scoping_bug_twoframe(self, env):
        t = Template('''
            {% set x = 1 %}
            {% for item in foo %}
                {% if item == 1 %}
                    {% set x = 2 %}
                {% endif %}
            {% endfor %}
            {{ x }}
        ''')
        rv = t.render(foo=[1]).strip()
        assert rv == u'1'

    def test_call_with_args(self, env):
        t = Template("""{% macro dump_users(users) -%}
        <ul>
          {%- for user in users -%}
            <li><p>{{ user.username|e }}</p>{{ caller(user) }}</li>
          {%- endfor -%}
          </ul>
        {%- endmacro -%}

        {% call(user) dump_users(list_of_user) -%}
          <dl>
            <dl>Realname</dl>
            <dd>{{ user.realname|e }}</dd>
            <dl>Description</dl>
            <dd>{{ user.description }}</dd>
          </dl>
        {% endcall %}""")

        assert [x.strip() for x in t.render(list_of_user=[{
            'username': 'apo',
            'realname': 'something else',
            'description': 'test'
        }]).splitlines()] == [
            u'<ul><li><p>apo</p><dl>',
            u'<dl>Realname</dl>',
            u'<dd>something else</dd>',
            u'<dl>Description</dl>',
            u'<dd>test</dd>',
            u'</dl>',
            u'</li></ul>'
        ]

    def test_empty_if_condition_fails(self, env):
        pytest.raises(TemplateSyntaxError,
                      Template, '{% if %}....{% endif %}')
        pytest.raises(TemplateSyntaxError,
                      Template, '{% if foo %}...{% elif %}...{% endif %}')
        pytest.raises(TemplateSyntaxError,
                      Template, '{% for x in %}..{% endfor %}')

    def test_recursive_loop_bug(self, env):
        tpl1 = Template("""
        {% for p in foo recursive%}
            {{p.bar}}
            {% for f in p.fields recursive%}
                {{f.baz}}
                {{p.bar}}
                {% if f.rec %}
                    {{ loop(f.sub) }}
                {% endif %}
            {% endfor %}
        {% endfor %}
        """)

        tpl2 = Template("""
        {% for p in foo%}
            {{p.bar}}
            {% for f in p.fields recursive%}
                {{f.baz}}
                {{p.bar}}
                {% if f.rec %}
                    {{ loop(f.sub) }}
                {% endif %}
            {% endfor %}
        {% endfor %}
        """)

    def test_else_loop_bug(self, env):
        t = Template('''
            {% for x in y %}
                {{ loop.index0 }}
            {% else %}
                {% for i in range(3) %}{{ i }}{% endfor %}
            {% endfor %}
        ''')
        assert t.render(y=[]).strip() == '012'

    def test_correct_prefix_loader_name(self, env):
        env = Environment(loader=PrefixLoader({
            'foo':  DictLoader({})
        }))
        try:
            env.get_template('foo/bar.html')
        except TemplateNotFound as e:
            assert e.name == 'foo/bar.html'
        else:
            assert False, 'expected error here'

    def test_contextfunction_callable_classes(self, env):
        from jinja2.utils import contextfunction

        class CallableClass(object):
            @contextfunction
            def __call__(self, ctx):
                return ctx.resolve('hello')

        tpl = Template("""{{ callableclass() }}""")
        output = tpl.render(callableclass=CallableClass(), hello='TEST')
        expected = 'TEST'

        assert output == expected
