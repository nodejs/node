from jinja2 import Environment

tmpl = Environment().from_string("""\
<ul>
{%- for item in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] if item % 2 == 0 %}
    <li>{{ loop.index }} / {{ loop.length }}: {{ item }}</li>
{%- endfor %}
</ul>
if condition: {{ 1 if foo else 0 }}
""")

print(tmpl.render(foo=True))
