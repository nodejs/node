from django.conf import settings
settings.configure(TEMPLATE_DIRS=['templates'], TEMPLATE_DEBUG=True)

from django2jinja import convert_templates, Writer

writer = Writer(use_jinja_autoescape=True)
convert_templates('converted', writer=writer)
