from jinja2 import Environment


env = Environment(line_statement_prefix='%', variable_start_string="${", variable_end_string="}")
tmpl = env.from_string("""\
% macro foo()
    ${caller(42)}
% endmacro
<ul>
% for item in seq
    <li>${item}</li>
% endfor
</ul>
% call(var) foo()
    [${var}]
% endcall
% filter escape
    <hello world>
    % for item in [1, 2, 3]
      -  ${item}
    % endfor
% endfilter
""")

print(tmpl.render(seq=range(10)))
