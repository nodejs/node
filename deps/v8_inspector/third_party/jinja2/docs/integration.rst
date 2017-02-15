Integration
===========

Jinja2 provides some code for integration into other tools such as frameworks,
the `Babel`_ library or your favourite editor for fancy code highlighting.
This is a brief description of whats included.

Files to help integration are available
`here. <https://github.com/mitsuhiko/jinja2/tree/master/ext>`_

.. _babel-integration:

Babel Integration
-----------------

Jinja provides support for extracting gettext messages from templates via a
`Babel`_ extractor entry point called `jinja2.ext.babel_extract`.  The Babel
support is implemented as part of the :ref:`i18n-extension` extension.

Gettext messages extracted from both `trans` tags and code expressions.

To extract gettext messages from templates, the project needs a Jinja2 section
in its Babel extraction method `mapping file`_:

.. sourcecode:: ini

    [jinja2: **/templates/**.html]
    encoding = utf-8

The syntax related options of the :class:`Environment` are also available as
configuration values in the mapping file.  For example to tell the extraction
that templates use ``%`` as `line_statement_prefix` you can use this code:

.. sourcecode:: ini

    [jinja2: **/templates/**.html]
    encoding = utf-8
    line_statement_prefix = %

:ref:`jinja-extensions` may also be defined by passing a comma separated list
of import paths as `extensions` value.  The i18n extension is added
automatically.

.. versionchanged:: 2.7

   Until 2.7 template syntax errors were always ignored.  This was done
   since many people are dropping non template html files into the
   templates folder and it would randomly fail.  The assumption was that
   testsuites will catch syntax errors in templates anyways.  If you don't
   want that behavior you can add ``silent=false`` to the settings and
   exceptions are propagated.

.. _mapping file: http://babel.edgewall.org/wiki/Documentation/messages.html#extraction-method-mapping-and-configuration

Pylons
------

With `Pylons`_ 0.9.7 onwards it's incredible easy to integrate Jinja into a
Pylons powered application.

The template engine is configured in `config/environment.py`.  The configuration
for Jinja2 looks something like that::

    from jinja2 import Environment, PackageLoader
    config['pylons.app_globals'].jinja_env = Environment(
        loader=PackageLoader('yourapplication', 'templates')
    )

After that you can render Jinja templates by using the `render_jinja` function
from the `pylons.templating` module.

Additionally it's a good idea to set the Pylons' `c` object into strict mode.
Per default any attribute to not existing attributes on the `c` object return
an empty string and not an undefined object.  To change this just use this
snippet and add it into your `config/environment.py`::

    config['pylons.strict_c'] = True

.. _Pylons: http://www.pylonshq.com/

TextMate
--------

There is a bundle for TextMate that supports syntax highlighting for Jinja1 and Jinja2 for text based
templates as well as HTML.  It also contains a few often used snippets.

.. _TextMate Bundle: https://github.com/mitsuhiko/jinja2-tmbundle

Vim
---

A syntax plugin for `Vim`_ exists in the Vim-scripts directory as well as the
`ext` folder at the root of the Jinja2 project.  `The script
<http://www.vim.org/scripts/script.php?script_id=1856>`_ supports Jinja1 and
Jinja2.  Once installed two file types are available `jinja` and `htmljinja`.
The first one for text based templates, the latter for HTML templates.

Copy the files into your `syntax` folder.

.. _Babel: http://babel.edgewall.org/
.. _Vim: http://www.vim.org/
