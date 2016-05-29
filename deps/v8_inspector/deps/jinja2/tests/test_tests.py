# -*- coding: utf-8 -*-
"""
    jinja2.testsuite.tests
    ~~~~~~~~~~~~~~~~~~~~~~

    Who tests the tests?

    :copyright: (c) 2010 by the Jinja Team.
    :license: BSD, see LICENSE for more details.
"""
import pytest

from jinja2 import Markup, Environment


@pytest.mark.test_tests
class TestTestsCase():

    def test_defined(self, env):
        tmpl = env.from_string('{{ missing is defined }}|'
                               '{{ true is defined }}')
        assert tmpl.render() == 'False|True'

    def test_even(self, env):
        tmpl = env.from_string('''{{ 1 is even }}|{{ 2 is even }}''')
        assert tmpl.render() == 'False|True'

    def test_odd(self, env):
        tmpl = env.from_string('''{{ 1 is odd }}|{{ 2 is odd }}''')
        assert tmpl.render() == 'True|False'

    def test_lower(self, env):
        tmpl = env.from_string('''{{ "foo" is lower }}|{{ "FOO" is lower }}''')
        assert tmpl.render() == 'True|False'

    def test_typechecks(self, env):
        tmpl = env.from_string('''
            {{ 42 is undefined }}
            {{ 42 is defined }}
            {{ 42 is none }}
            {{ none is none }}
            {{ 42 is number }}
            {{ 42 is string }}
            {{ "foo" is string }}
            {{ "foo" is sequence }}
            {{ [1] is sequence }}
            {{ range is callable }}
            {{ 42 is callable }}
            {{ range(5) is iterable }}
            {{ {} is mapping }}
            {{ mydict is mapping }}
            {{ [] is mapping }}
            {{ 10 is number }}
            {{ (10 ** 100) is number }}
            {{ 3.14159 is number }}
            {{ complex is number }}
        ''')

        class MyDict(dict):
            pass

        assert tmpl.render(mydict=MyDict(), complex=complex(1, 2)).split() == [
            'False', 'True', 'False', 'True', 'True', 'False',
            'True', 'True', 'True', 'True', 'False', 'True',
            'True', 'True', 'False', 'True', 'True', 'True', 'True'
        ]

    def test_sequence(self, env):
        tmpl = env.from_string(
            '{{ [1, 2, 3] is sequence }}|'
            '{{ "foo" is sequence }}|'
            '{{ 42 is sequence }}'
        )
        assert tmpl.render() == 'True|True|False'

    def test_upper(self, env):
        tmpl = env.from_string('{{ "FOO" is upper }}|{{ "foo" is upper }}')
        assert tmpl.render() == 'True|False'

    def test_equalto(self, env):
        tmpl = env.from_string('{{ foo is equalto 12 }}|'
                               '{{ foo is equalto 0 }}|'
                               '{{ foo is equalto (3 * 4) }}|'
                               '{{ bar is equalto "baz" }}|'
                               '{{ bar is equalto "zab" }}|'
                               '{{ bar is equalto ("ba" + "z") }}|'
                               '{{ bar is equalto bar }}|'
                               '{{ bar is equalto foo }}')
        assert tmpl.render(foo=12, bar="baz") \
            == 'True|False|True|True|False|True|True|False'

    def test_sameas(self, env):
        tmpl = env.from_string('{{ foo is sameas false }}|'
                               '{{ 0 is sameas false }}')
        assert tmpl.render(foo=False) == 'True|False'

    def test_no_paren_for_arg1(self, env):
        tmpl = env.from_string('{{ foo is sameas none }}')
        assert tmpl.render(foo=None) == 'True'

    def test_escaped(self, env):
        env = Environment(autoescape=True)
        tmpl = env.from_string('{{ x is escaped }}|{{ y is escaped }}')
        assert tmpl.render(x='foo', y=Markup('foo')) == 'False|True'
