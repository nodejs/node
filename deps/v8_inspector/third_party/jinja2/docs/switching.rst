Switching from other Template Engines
=====================================

.. highlight:: html+jinja

If you have used a different template engine in the past and want to switch
to Jinja2 here is a small guide that shows the basic syntactic and semantic
changes between some common, similar text template engines for Python.

Jinja1
------

Jinja2 is mostly compatible with Jinja1 in terms of API usage and template
syntax.  The differences between Jinja1 and 2 are explained in the following
list.

API
~~~

Loaders
    Jinja2 uses a different loader API.  Because the internal representation
    of templates changed there is no longer support for external caching
    systems such as memcached.  The memory consumed by templates is comparable
    with regular Python modules now and external caching doesn't give any
    advantage.  If you have used a custom loader in the past have a look at
    the new :ref:`loader API <loaders>`.

Loading templates from strings
    In the past it was possible to generate templates from a string with the
    default environment configuration by using `jinja.from_string`.  Jinja2
    provides a :class:`Template` class that can be used to do the same, but
    with optional additional configuration.

Automatic unicode conversion
    Jinja1 performed automatic conversion of bytestrings in a given encoding
    into unicode objects.  This conversion is no longer implemented as it
    was inconsistent as most libraries are using the regular Python ASCII
    bytestring to Unicode conversion.  An application powered by Jinja2
    *has to* use unicode internally everywhere or make sure that Jinja2 only
    gets unicode strings passed.

i18n
    Jinja1 used custom translators for internationalization.  i18n is now
    available as Jinja2 extension and uses a simpler, more gettext friendly
    interface and has support for babel.  For more details see
    :ref:`i18n-extension`.

Internal methods
    Jinja1 exposed a few internal methods on the environment object such
    as `call_function`, `get_attribute` and others.  While they were marked
    as being an internal method it was possible to override them.  Jinja2
    doesn't have equivalent methods.

Sandbox
    Jinja1 was running sandbox mode by default.  Few applications actually
    used that feature so it became optional in Jinja2.  For more details
    about the sandboxed execution see :class:`SandboxedEnvironment`.

Context
    Jinja1 had a stacked context as storage for variables passed to the
    environment.  In Jinja2 a similar object exists but it doesn't allow
    modifications nor is it a singleton.  As inheritance is dynamic now
    multiple context objects may exist during template evaluation.

Filters and Tests
    Filters and tests are regular functions now.  It's no longer necessary
    and allowed to use factory functions.


Templates
~~~~~~~~~

Jinja2 has mostly the same syntax as Jinja1.  What's different is that
macros require parentheses around the argument list now.

Additionally Jinja2 allows dynamic inheritance now and dynamic includes.
The old helper function `rendertemplate` is gone now, `include` can be used
instead.  Includes no longer import macros and variable assignments, for
that the new `import` tag is used.  This concept is explained in the
:ref:`import` documentation.

Another small change happened in the `for`-tag.  The special loop variable
doesn't have a `parent` attribute, instead you have to alias the loop
yourself.  See :ref:`accessing-the-parent-loop` for more details.


Django
------

If you have previously worked with Django templates, you should find
Jinja2 very familiar.  In fact, most of the syntax elements look and
work the same.

However, Jinja2 provides some more syntax elements covered in the
documentation and some work a bit different.

This section covers the template changes.  As the API is fundamentally
different we won't cover it here.

Method Calls
~~~~~~~~~~~~

In Django method calls work implicitly, while Jinja requires the explicit
Python syntax. Thus this Django code::

    {% for page in user.get_created_pages %}
        ...
    {% endfor %}

...looks like this in Jinja::

    {% for page in user.get_created_pages() %}
        ...
    {% endfor %}

This allows you to pass variables to the method, which is not possible in
Django. This syntax is also used for macros.

Filter Arguments
~~~~~~~~~~~~~~~~

Jinja2 provides more than one argument for filters.  Also the syntax for
argument passing is different.  A template that looks like this in Django::

    {{ items|join:", " }}

looks like this in Jinja2::

    {{ items|join(', ') }}

It is a bit more verbose, but it allows different types of arguments -
including variables - and more than one of them.

Tests
~~~~~

In addition to filters there also are tests you can perform using the is
operator.  Here are some examples::

    {% if user.user_id is odd %}
        {{ user.username|e }} is odd
    {% else %}
        hmm. {{ user.username|e }} looks pretty normal
    {% endif %}

Loops
~~~~~

For loops work very similarly to Django, but notably the Jinja2 special
variable for the loop context is called `loop`, not `forloop` as in Django.

In addition, the Django `empty` argument is called `else` in Jinja2. For
example, the Django template::

    {% for item in items %}
        {{ item }}
    {% empty %}
        No items!
    {% endfor %}

...looks like this in Jinja2::

    {% for item in items %}
        {{ item }}
    {% else %}
        No items!
    {% endfor %}

Cycle
~~~~~

The ``{% cycle %}`` tag does not exist in Jinja2; however, you can achieve the
same output by using the `cycle` method on the loop context special variable.

The following Django template::

    {% for user in users %}
        <li class="{% cycle 'odd' 'even' %}">{{ user }}</li>
    {% endfor %}

...looks like this in Jinja2::

    {% for user in users %}
        <li class="{{ loop.cycle('odd', 'even') }}">{{ user }}</li>
    {% endfor %}

There is no equivalent of ``{% cycle ... as variable %}``.


Mako
----

.. highlight:: html+mako

If you have used Mako so far and want to switch to Jinja2 you can configure
Jinja2 to look more like Mako:

.. sourcecode:: python

    env = Environment('<%', '%>', '${', '}', '<%doc>', '</%doc>', '%', '##')

With an environment configured like that, Jinja2 should be able to interpret
a small subset of Mako templates.  Jinja2 does not support embedded Python
code, so you would have to move that out of the template.  The syntax for defs
(which are called macros in Jinja2) and template inheritance is different too.
The following Mako template::

    <%inherit file="layout.html" />
    <%def name="title()">Page Title</%def>
    <ul>
    % for item in list:
        <li>${item}</li>
    % endfor
    </ul>

Looks like this in Jinja2 with the above configuration::

    <% extends "layout.html" %>
    <% block title %>Page Title<% endblock %>
    <% block body %>
    <ul>
    % for item in list:
        <li>${item}</li>
    % endfor
    </ul>
    <% endblock %>
