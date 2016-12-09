Tips and Tricks
===============

.. highlight:: html+jinja

This part of the documentation shows some tips and tricks for Jinja2
templates.


.. _null-master-fallback:

Null-Master Fallback
--------------------

Jinja2 supports dynamic inheritance and does not distinguish between parent
and child template as long as no `extends` tag is visited.  While this leads
to the surprising behavior that everything before the first `extends` tag
including whitespace is printed out instead of being ignored, it can be used
for a neat trick.

Usually child templates extend from one template that adds a basic HTML
skeleton.  However it's possible to put the `extends` tag into an `if` tag to
only extend from the layout template if the `standalone` variable evaluates
to false which it does per default if it's not defined.  Additionally a very
basic skeleton is added to the file so that if it's indeed rendered with
`standalone` set to `True` a very basic HTML skeleton is added::

    {% if not standalone %}{% extends 'master.html' %}{% endif -%}
    <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
    <title>{% block title %}The Page Title{% endblock %}</title>
    <link rel="stylesheet" href="style.css" type="text/css">
    {% block body %}
      <p>This is the page body.</p>
    {% endblock %}


Alternating Rows
----------------

If you want to have different styles for each row of a table or
list you can use the `cycle` method on the `loop` object::

    <ul>
    {% for row in rows %}
      <li class="{{ loop.cycle('odd', 'even') }}">{{ row }}</li>
    {% endfor %}
    </ul>

`cycle` can take an unlimited amount of strings.  Each time this
tag is encountered the next item from the list is rendered.


Highlighting Active Menu Items
------------------------------

Often you want to have a navigation bar with an active navigation
item.  This is really simple to achieve.  Because assignments outside
of `block`\s in child templates are global and executed before the layout
template is evaluated it's possible to define the active menu item in the
child template::

    {% extends "layout.html" %}
    {% set active_page = "index" %}

The layout template can then access `active_page`.  Additionally it makes
sense to define a default for that variable::

    {% set navigation_bar = [
        ('/', 'index', 'Index'),
        ('/downloads/', 'downloads', 'Downloads'),
        ('/about/', 'about', 'About')
    ] -%}
    {% set active_page = active_page|default('index') -%}
    ...
    <ul id="navigation">
    {% for href, id, caption in navigation_bar %}
      <li{% if id == active_page %} class="active"{% endif
      %}><a href="{{ href|e }}">{{ caption|e }}</a></li>
    {% endfor %}
    </ul>
    ...

.. _accessing-the-parent-loop:

Accessing the parent Loop
-------------------------

The special `loop` variable always points to the innermost loop.  If it's
desired to have access to an outer loop it's possible to alias it::

    <table>
    {% for row in table %}
      <tr>
      {% set rowloop = loop %}
      {% for cell in row %}
        <td id="cell-{{ rowloop.index }}-{{ loop.index }}">{{ cell }}</td>
      {% endfor %}
      </tr>
    {% endfor %}
    </table>
