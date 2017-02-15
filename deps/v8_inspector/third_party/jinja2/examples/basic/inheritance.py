from jinja2 import Environment
from jinja2.loaders import DictLoader


env = Environment(loader=DictLoader({
'a': '''[A[{% block body %}{% endblock %}]]''',
'b': '''{% extends 'a' %}{% block body %}[B]{% endblock %}''',
'c': '''{% extends 'b' %}{% block body %}###{{ super() }}###{% endblock %}'''
}))


print env.get_template('c').render()
