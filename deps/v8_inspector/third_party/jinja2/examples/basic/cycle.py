from jinja2 import Environment


env = Environment(line_statement_prefix="#", variable_start_string="${", variable_end_string="}")


print env.from_string("""\
<ul>
# for item in range(10)
    <li class="${loop.cycle('odd', 'even')}">${item}</li>
# endfor
</ul>\
""").render()
