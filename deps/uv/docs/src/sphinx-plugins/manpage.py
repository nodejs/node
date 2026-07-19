# encoding: utf-8

#
# Copyright (c) 2013 Dariusz Dwornikowski.  All rights reserved.
#
# Adapted from https://github.com/tdi/sphinxcontrib-manpage
# License: Apache 2
#


import re

from docutils import nodes, utils
from docutils.parsers.rst.roles import set_classes
from string import Template


def make_link_node(rawtext, app, name, manpage_num, options):
    ref = app.config.man_url_regex
    if not ref:
        ref = "https://man7.org/linux/man-pages/man%s/%s.%s.html" %(manpage_num, name, manpage_num)
    else:
        s = Template(ref)
        ref = s.substitute(num=manpage_num, topic=name)
    set_classes(options)
    node = nodes.reference(rawtext, "%s(%s)" % (name, manpage_num), refuri=ref, **options)
    return node


def man_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app
    p = re.compile("([a-zA-Z0-9_\.-_]+)\((\d)\)")
    m = p.match(text)

    manpage_num = m.group(2)
    name = m.group(1)
    node = make_link_node(rawtext, app, name, manpage_num, options)
    return [node], []


def setup(app):
    app.add_role('man', man_role)
    app.add_config_value('man_url_regex', None, 'env')
    return

