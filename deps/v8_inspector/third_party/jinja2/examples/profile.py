try:
    from cProfile import Profile
except ImportError:
    from profile import Profile
from pstats import Stats
from jinja2 import Environment as JinjaEnvironment

context = {
    'page_title': 'mitsuhiko\'s benchmark',
    'table': [dict(a=1,b=2,c=3,d=4,e=5,f=6,g=7,h=8,i=9,j=10) for x in range(1000)]
}

source = """\
% macro testmacro(x)
  <span>${x}</span>
% endmacro
<!doctype html>
<html>
  <head>
    <title>${page_title|e}</title>
  </head>
  <body>
    <div class="header">
      <h1>${page_title|e}</h1>
    </div>
    <div class="table">
      <table>
      % for row in table
        <tr>
        % for cell in row
          <td>${testmacro(cell)}</td>
        % endfor
        </tr>
      % endfor
      </table>
    </div>
  </body>
</html>\
"""
jinja_template = JinjaEnvironment(
    line_statement_prefix='%',
    variable_start_string="${",
    variable_end_string="}"
).from_string(source)
print jinja_template.environment.compile(source, raw=True)


p = Profile()
p.runcall(lambda: jinja_template.render(context))
stats = Stats(p)
stats.sort_stats('time', 'calls')
stats.print_stats()
