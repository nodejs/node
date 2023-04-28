Jinja
=====

Jinja is a fast, expressive, extensible templating engine. Special
placeholders in the template allow writing code similar to Python
syntax. Then the template is passed data to render the final document.

It includes:

-   Template inheritance and inclusion.
-   Define and import macros within templates.
-   HTML templates can use autoescaping to prevent XSS from untrusted
    user input.
-   A sandboxed environment can safely render untrusted templates.
-   AsyncIO support for generating templates and calling async
    functions.
-   I18N support with Babel.
-   Templates are compiled to optimized Python code just-in-time and
    cached, or can be compiled ahead-of-time.
-   Exceptions point to the correct line in templates to make debugging
    easier.
-   Extensible filters, tests, functions, and even syntax.

Jinja's philosophy is that while application logic belongs in Python if
possible, it shouldn't make the template designer's job difficult by
restricting functionality too much.


Installing
----------

Install and update using `pip`_:

.. code-block:: text

    $ pip install -U Jinja2

.. _pip: https://pip.pypa.io/en/stable/getting-started/


In A Nutshell
-------------

.. code-block:: jinja

    {% extends "base.html" %}
    {% block title %}Members{% endblock %}
    {% block content %}
      <ul>
      {% for user in users %}
        <li><a href="{{ user.url }}">{{ user.username }}</a></li>
      {% endfor %}
      </ul>
    {% endblock %}


Donate
------

The Pallets organization develops and supports Jinja and other popular
packages. In order to grow the community of contributors and users, and
allow the maintainers to devote more time to the projects, `please
donate today`_.

.. _please donate today: https://palletsprojects.com/donate


Links
-----

-   Documentation: https://jinja.palletsprojects.com/
-   Changes: https://jinja.palletsprojects.com/changes/
-   PyPI Releases: https://pypi.org/project/Jinja2/
-   Source Code: https://github.com/pallets/jinja/
-   Issue Tracker: https://github.com/pallets/jinja/issues/
-   Website: https://palletsprojects.com/p/jinja/
-   Twitter: https://twitter.com/PalletsTeam
-   Chat: https://discord.gg/pallets
