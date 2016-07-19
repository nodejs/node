# -*- coding: utf-8 -*-
"""
    jinja2.testsuite.inheritance
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Tests the template inheritance feature.

    :copyright: (c) 2010 by the Jinja Team.
    :license: BSD, see LICENSE for more details.
"""
import pytest

from jinja2 import Environment, DictLoader, TemplateError


LAYOUTTEMPLATE = '''\
|{% block block1 %}block 1 from layout{% endblock %}
|{% block block2 %}block 2 from layout{% endblock %}
|{% block block3 %}
{% block block4 %}nested block 4 from layout{% endblock %}
{% endblock %}|'''

LEVEL1TEMPLATE = '''\
{% extends "layout" %}
{% block block1 %}block 1 from level1{% endblock %}'''

LEVEL2TEMPLATE = '''\
{% extends "level1" %}
{% block block2 %}{% block block5 %}nested block 5 from level2{%
endblock %}{% endblock %}'''

LEVEL3TEMPLATE = '''\
{% extends "level2" %}
{% block block5 %}block 5 from level3{% endblock %}
{% block block4 %}block 4 from level3{% endblock %}
'''

LEVEL4TEMPLATE = '''\
{% extends "level3" %}
{% block block3 %}block 3 from level4{% endblock %}
'''

WORKINGTEMPLATE = '''\
{% extends "layout" %}
{% block block1 %}
  {% if false %}
    {% block block2 %}
      this should workd
    {% endblock %}
  {% endif %}
{% endblock %}
'''

DOUBLEEXTENDS = '''\
{% extends "layout" %}
{% extends "layout" %}
{% block block1 %}
  {% if false %}
    {% block block2 %}
      this should workd
    {% endblock %}
  {% endif %}
{% endblock %}
'''


@pytest.fixture
def env():
    return Environment(loader=DictLoader({
        'layout':       LAYOUTTEMPLATE,
        'level1':       LEVEL1TEMPLATE,
        'level2':       LEVEL2TEMPLATE,
        'level3':       LEVEL3TEMPLATE,
        'level4':       LEVEL4TEMPLATE,
        'working':      WORKINGTEMPLATE,
        'doublee':      DOUBLEEXTENDS,
    }), trim_blocks=True)


@pytest.mark.inheritance
class TestInheritance():

    def test_layout(self, env):
        tmpl = env.get_template('layout')
        assert tmpl.render() == ('|block 1 from layout|block 2 from '
                                 'layout|nested block 4 from layout|')

    def test_level1(self, env):
        tmpl = env.get_template('level1')
        assert tmpl.render() == ('|block 1 from level1|block 2 from '
                                 'layout|nested block 4 from layout|')

    def test_level2(self, env):
        tmpl = env.get_template('level2')
        assert tmpl.render() == ('|block 1 from level1|nested block 5 from '
                                 'level2|nested block 4 from layout|')

    def test_level3(self, env):
        tmpl = env.get_template('level3')
        assert tmpl.render() == ('|block 1 from level1|block 5 from level3|'
                                 'block 4 from level3|')

    def test_level4(self, env):
        tmpl = env.get_template('level4')
        assert tmpl.render() == ('|block 1 from level1|block 5 from '
                                 'level3|block 3 from level4|')

    def test_super(self, env):
        env = Environment(loader=DictLoader({
            'a': '{% block intro %}INTRO{% endblock %}|'
                 'BEFORE|{% block data %}INNER{% endblock %}|AFTER',
            'b': '{% extends "a" %}{% block data %}({{ '
                 'super() }}){% endblock %}',
            'c': '{% extends "b" %}{% block intro %}--{{ '
                 'super() }}--{% endblock %}\n{% block data '
                 '%}[{{ super() }}]{% endblock %}'
        }))
        tmpl = env.get_template('c')
        assert tmpl.render() == '--INTRO--|BEFORE|[(INNER)]|AFTER'

    def test_working(self, env):
        tmpl = env.get_template('working')

    def test_reuse_blocks(self, env):
        tmpl = env.from_string('{{ self.foo() }}|{% block foo %}42'
                               '{% endblock %}|{{ self.foo() }}')
        assert tmpl.render() == '42|42|42'

    def test_preserve_blocks(self, env):
        env = Environment(loader=DictLoader({
            'a': '{% if false %}{% block x %}A{% endblock %}'
            '{% endif %}{{ self.x() }}',
            'b': '{% extends "a" %}{% block x %}B{{ super() }}{% endblock %}'
        }))
        tmpl = env.get_template('b')
        assert tmpl.render() == 'BA'

    def test_dynamic_inheritance(self, env):
        env = Environment(loader=DictLoader({
            'master1': 'MASTER1{% block x %}{% endblock %}',
            'master2': 'MASTER2{% block x %}{% endblock %}',
            'child': '{% extends master %}{% block x %}CHILD{% endblock %}'
        }))
        tmpl = env.get_template('child')
        for m in range(1, 3):
            assert tmpl.render(master='master%d' % m) == 'MASTER%dCHILD' % m

    def test_multi_inheritance(self, env):
        env = Environment(loader=DictLoader({
            'master1': 'MASTER1{% block x %}{% endblock %}',
            'master2': 'MASTER2{% block x %}{% endblock %}',
            'child':
                '''{% if master %}{% extends master %}{% else %}{% extends
                'master1' %}{% endif %}{% block x %}CHILD{% endblock %}'''
        }))
        tmpl = env.get_template('child')
        assert tmpl.render(master='master2') == 'MASTER2CHILD'
        assert tmpl.render(master='master1') == 'MASTER1CHILD'
        assert tmpl.render() == 'MASTER1CHILD'

    def test_scoped_block(self, env):
        env = Environment(loader=DictLoader({
            'master.html': '{% for item in seq %}[{% block item scoped %}'
                           '{% endblock %}]{% endfor %}'
        }))
        t = env.from_string('{% extends "master.html" %}{% block item %}'
                            '{{ item }}{% endblock %}')
        assert t.render(seq=list(range(5))) == '[0][1][2][3][4]'

    def test_super_in_scoped_block(self, env):
        env = Environment(loader=DictLoader({
            'master.html': '{% for item in seq %}[{% block item scoped %}'
                           '{{ item }}{% endblock %}]{% endfor %}'
        }))
        t = env.from_string('{% extends "master.html" %}{% block item %}'
                            '{{ super() }}|{{ item * 2 }}{% endblock %}')
        assert t.render(seq=list(range(5))) == '[0|0][1|2][2|4][3|6][4|8]'

    def test_scoped_block_after_inheritance(self, env):
        env = Environment(loader=DictLoader({
            'layout.html': '''
            {% block useless %}{% endblock %}
            ''',
            'index.html': '''
            {%- extends 'layout.html' %}
            {% from 'helpers.html' import foo with context %}
            {% block useless %}
                {% for x in [1, 2, 3] %}
                    {% block testing scoped %}
                        {{ foo(x) }}
                    {% endblock %}
                {% endfor %}
            {% endblock %}
            ''',
            'helpers.html': '''
            {% macro foo(x) %}{{ the_foo + x }}{% endmacro %}
            '''
        }))
        rv = env.get_template('index.html').render(the_foo=42).split()
        assert rv == ['43', '44', '45']


@pytest.mark.inheritance
class TestBugFix():

    def test_fixed_macro_scoping_bug(self, env):
        assert Environment(loader=DictLoader({
            'test.html': '''\
        {% extends 'details.html' %}

        {% macro my_macro() %}
        my_macro
        {% endmacro %}

        {% block inner_box %}
            {{ my_macro() }}
        {% endblock %}
            ''',
            'details.html': '''\
        {% extends 'standard.html' %}

        {% macro my_macro() %}
        my_macro
        {% endmacro %}

        {% block content %}
            {% block outer_box %}
                outer_box
                {% block inner_box %}
                    inner_box
                {% endblock %}
            {% endblock %}
        {% endblock %}
        ''',
            'standard.html': '''
        {% block content %}&nbsp;{% endblock %}
        '''
        })).get_template("test.html").render().split() \
            == [u'outer_box', u'my_macro']

    def test_double_extends(self, env):
        """Ensures that a template with more than 1 {% extends ... %} usage
        raises a ``TemplateError``.
        """
        try:
            tmpl = env.get_template('doublee')
        except Exception as e:
            assert isinstance(e, TemplateError)
