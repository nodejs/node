API
===

.. module:: jinja2
    :synopsis: public Jinja2 API

This document describes the API to Jinja2 and not the template language.  It
will be most useful as reference to those implementing the template interface
to the application and not those who are creating Jinja2 templates.

Basics
------

Jinja2 uses a central object called the template :class:`Environment`.
Instances of this class are used to store the configuration and global objects,
and are used to load templates from the file system or other locations.
Even if you are creating templates from strings by using the constructor of
:class:`Template` class, an environment is created automatically for you,
albeit a shared one.

Most applications will create one :class:`Environment` object on application
initialization and use that to load templates.  In some cases it's however
useful to have multiple environments side by side, if different configurations
are in use.

The simplest way to configure Jinja2 to load templates for your application
looks roughly like this::

    from jinja2 import Environment, PackageLoader
    env = Environment(loader=PackageLoader('yourapplication', 'templates'))

This will create a template environment with the default settings and a
loader that looks up the templates in the `templates` folder inside the
`yourapplication` python package.  Different loaders are available
and you can also write your own if you want to load templates from a
database or other resources.

To load a template from this environment you just have to call the
:meth:`get_template` method which then returns the loaded :class:`Template`::

    template = env.get_template('mytemplate.html')

To render it with some variables, just call the :meth:`render` method::

    print template.render(the='variables', go='here')

Using a template loader rather than passing strings to :class:`Template`
or :meth:`Environment.from_string` has multiple advantages.  Besides being
a lot easier to use it also enables template inheritance.


Unicode
-------

Jinja2 is using Unicode internally which means that you have to pass Unicode
objects to the render function or bytestrings that only consist of ASCII
characters.  Additionally newlines are normalized to one end of line
sequence which is per default UNIX style (``\n``).

Python 2.x supports two ways of representing string objects.  One is the
`str` type and the other is the `unicode` type, both of which extend a type
called `basestring`.  Unfortunately the default is `str` which should not
be used to store text based information unless only ASCII characters are
used.  With Python 2.6 it is possible to make `unicode` the default on a per
module level and with Python 3 it will be the default.

To explicitly use a Unicode string you have to prefix the string literal
with a `u`: ``u'Hänsel und Gretel sagen Hallo'``.  That way Python will
store the string as Unicode by decoding the string with the character
encoding from the current Python module.  If no encoding is specified this
defaults to 'ASCII' which means that you can't use any non ASCII identifier.

To set a better module encoding add the following comment to the first or
second line of the Python module using the Unicode literal::

    # -*- coding: utf-8 -*-

We recommend utf-8 as Encoding for Python modules and templates as it's
possible to represent every Unicode character in utf-8 and because it's
backwards compatible to ASCII.  For Jinja2 the default encoding of templates
is assumed to be utf-8.

It is not possible to use Jinja2 to process non-Unicode data.  The reason
for this is that Jinja2 uses Unicode already on the language level.  For
example Jinja2 treats the non-breaking space as valid whitespace inside
expressions which requires knowledge of the encoding or operating on an
Unicode string.

For more details about Unicode in Python have a look at the excellent
`Unicode documentation`_.

Another important thing is how Jinja2 is handling string literals in
templates.  A naive implementation would be using Unicode strings for
all string literals but it turned out in the past that this is problematic
as some libraries are typechecking against `str` explicitly.  For example
`datetime.strftime` does not accept Unicode arguments.  To not break it
completely Jinja2 is returning `str` for strings that fit into ASCII and
for everything else `unicode`:

>>> m = Template(u"{% set a, b = 'foo', 'föö' %}").module
>>> m.a
'foo'
>>> m.b
u'f\xf6\xf6'


.. _Unicode documentation: http://docs.python.org/dev/howto/unicode.html

High Level API
--------------

The high-level API is the API you will use in the application to load and
render Jinja2 templates.  The :ref:`low-level-api` on the other side is only
useful if you want to dig deeper into Jinja2 or :ref:`develop extensions
<jinja-extensions>`.

.. autoclass:: Environment([options])
    :members: from_string, get_template, select_template,
              get_or_select_template, join_path, extend, compile_expression,
              compile_templates, list_templates, add_extension

    .. attribute:: shared

        If a template was created by using the :class:`Template` constructor
        an environment is created automatically.  These environments are
        created as shared environments which means that multiple templates
        may have the same anonymous environment.  For all shared environments
        this attribute is `True`, else `False`.

    .. attribute:: sandboxed

        If the environment is sandboxed this attribute is `True`.  For the
        sandbox mode have a look at the documentation for the
        :class:`~jinja2.sandbox.SandboxedEnvironment`.

    .. attribute:: filters

        A dict of filters for this environment.  As long as no template was
        loaded it's safe to add new filters or remove old.  For custom filters
        see :ref:`writing-filters`.  For valid filter names have a look at
        :ref:`identifier-naming`.

    .. attribute:: tests

        A dict of test functions for this environment.  As long as no
        template was loaded it's safe to modify this dict.  For custom tests
        see :ref:`writing-tests`.  For valid test names have a look at
        :ref:`identifier-naming`.

    .. attribute:: globals

        A dict of global variables.  These variables are always available
        in a template.  As long as no template was loaded it's safe
        to modify this dict.  For more details see :ref:`global-namespace`.
        For valid object names have a look at :ref:`identifier-naming`.

    .. attribute:: code_generator_class

       The class used for code generation.  This should not be changed
       in most cases, unless you need to modify the Python code a
       template compiles to.

    .. attribute:: context_class

       The context used for templates.  This should not be changed
       in most cases, unless you need to modify internals of how
       template variables are handled.  For details, see
       :class:`~jinja2.runtime.Context`.

    .. automethod:: overlay([options])

    .. method:: undefined([hint, obj, name, exc])

        Creates a new :class:`Undefined` object for `name`.  This is useful
        for filters or functions that may return undefined objects for
        some operations.  All parameters except of `hint` should be provided
        as keyword parameters for better readability.  The `hint` is used as
        error message for the exception if provided, otherwise the error
        message will be generated from `obj` and `name` automatically.  The exception
        provided as `exc` is raised if something with the generated undefined
        object is done that the undefined object does not allow.  The default
        exception is :exc:`UndefinedError`.  If a `hint` is provided the
        `name` may be omitted.

        The most common way to create an undefined object is by providing
        a name only::

            return environment.undefined(name='some_name')

        This means that the name `some_name` is not defined.  If the name
        was from an attribute of an object it makes sense to tell the
        undefined object the holder object to improve the error message::

            if not hasattr(obj, 'attr'):
                return environment.undefined(obj=obj, name='attr')

        For a more complex example you can provide a hint.  For example
        the :func:`first` filter creates an undefined object that way::

            return environment.undefined('no first item, sequence was empty')            

        If it the `name` or `obj` is known (for example because an attribute
        was accessed) it should be passed to the undefined object, even if
        a custom `hint` is provided.  This gives undefined objects the
        possibility to enhance the error message.

.. autoclass:: Template
    :members: module, make_module

    .. attribute:: globals

        The dict with the globals of that template.  It's unsafe to modify
        this dict as it may be shared with other templates or the environment
        that loaded the template.

    .. attribute:: name

        The loading name of the template.  If the template was loaded from a
        string this is `None`.

    .. attribute:: filename

        The filename of the template on the file system if it was loaded from
        there.  Otherwise this is `None`.

    .. automethod:: render([context])

    .. automethod:: generate([context])

    .. automethod:: stream([context])


.. autoclass:: jinja2.environment.TemplateStream()
    :members: disable_buffering, enable_buffering, dump


Autoescaping
------------

.. versionadded:: 2.4

As of Jinja 2.4 the preferred way to do autoescaping is to enable the
:ref:`autoescape-extension` and to configure a sensible default for
autoescaping.  This makes it possible to enable and disable autoescaping
on a per-template basis (HTML versus text for instance).

Here a recommended setup that enables autoescaping for templates ending
in ``'.html'``, ``'.htm'`` and ``'.xml'`` and disabling it by default
for all other extensions::

    def guess_autoescape(template_name):
        if template_name is None or '.' not in template_name:
            return False
        ext = template_name.rsplit('.', 1)[1]
        return ext in ('html', 'htm', 'xml')

    env = Environment(autoescape=guess_autoescape,
                      loader=PackageLoader('mypackage'),
                      extensions=['jinja2.ext.autoescape'])

When implementing a guessing autoescape function, make sure you also
accept `None` as valid template name.  This will be passed when generating
templates from strings.

Inside the templates the behaviour can be temporarily changed by using
the `autoescape` block (see :ref:`autoescape-overrides`).


.. _identifier-naming:

Notes on Identifiers
--------------------

Jinja2 uses the regular Python 2.x naming rules.  Valid identifiers have to
match ``[a-zA-Z_][a-zA-Z0-9_]*``.  As a matter of fact non ASCII characters
are currently not allowed.  This limitation will probably go away as soon as
unicode identifiers are fully specified for Python 3.

Filters and tests are looked up in separate namespaces and have slightly
modified identifier syntax.  Filters and tests may contain dots to group
filters and tests by topic.  For example it's perfectly valid to add a
function into the filter dict and call it `to.unicode`.  The regular
expression for filter and test identifiers is
``[a-zA-Z_][a-zA-Z0-9_]*(\.[a-zA-Z_][a-zA-Z0-9_]*)*```.


Undefined Types
---------------

These classes can be used as undefined types.  The :class:`Environment`
constructor takes an `undefined` parameter that can be one of those classes
or a custom subclass of :class:`Undefined`.  Whenever the template engine is
unable to look up a name or access an attribute one of those objects is
created and returned.  Some operations on undefined values are then allowed,
others fail.

The closest to regular Python behavior is the `StrictUndefined` which
disallows all operations beside testing if it's an undefined object.

.. autoclass:: jinja2.Undefined()

    .. attribute:: _undefined_hint

        Either `None` or an unicode string with the error message for
        the undefined object.

    .. attribute:: _undefined_obj

        Either `None` or the owner object that caused the undefined object
        to be created (for example because an attribute does not exist).

    .. attribute:: _undefined_name

        The name for the undefined variable / attribute or just `None`
        if no such information exists.

    .. attribute:: _undefined_exception

        The exception that the undefined object wants to raise.  This
        is usually one of :exc:`UndefinedError` or :exc:`SecurityError`.

    .. method:: _fail_with_undefined_error(\*args, \**kwargs)

        When called with any arguments this method raises
        :attr:`_undefined_exception` with an error message generated
        from the undefined hints stored on the undefined object.

.. autoclass:: jinja2.DebugUndefined()

.. autoclass:: jinja2.StrictUndefined()

There is also a factory function that can decorate undefined objects to
implement logging on failures:

.. autofunction:: jinja2.make_logging_undefined

Undefined objects are created by calling :attr:`undefined`.

.. admonition:: Implementation

    :class:`Undefined` objects are implemented by overriding the special
    `__underscore__` methods.  For example the default :class:`Undefined`
    class implements `__unicode__` in a way that it returns an empty
    string, however `__int__` and others still fail with an exception.  To
    allow conversion to int by returning ``0`` you can implement your own::

        class NullUndefined(Undefined):
            def __int__(self):
                return 0
            def __float__(self):
                return 0.0

    To disallow a method, just override it and raise
    :attr:`~Undefined._undefined_exception`.  Because this is a very common
    idom in undefined objects there is the helper method
    :meth:`~Undefined._fail_with_undefined_error` that does the error raising
    automatically.  Here a class that works like the regular :class:`Undefined`
    but chokes on iteration::

        class NonIterableUndefined(Undefined):
            __iter__ = Undefined._fail_with_undefined_error


The Context
-----------

.. autoclass:: jinja2.runtime.Context()
    :members: resolve, get_exported, get_all

    .. attribute:: parent

        A dict of read only, global variables the template looks up.  These
        can either come from another :class:`Context`, from the
        :attr:`Environment.globals` or :attr:`Template.globals` or points
        to a dict created by combining the globals with the variables
        passed to the render function.  It must not be altered.

    .. attribute:: vars

        The template local variables.  This list contains environment and
        context functions from the :attr:`parent` scope as well as local
        modifications and exported variables from the template.  The template
        will modify this dict during template evaluation but filters and
        context functions are not allowed to modify it.

    .. attribute:: environment

        The environment that loaded the template.

    .. attribute:: exported_vars

        This set contains all the names the template exports.  The values for
        the names are in the :attr:`vars` dict.  In order to get a copy of the
        exported variables as dict, :meth:`get_exported` can be used.

    .. attribute:: name

        The load name of the template owning this context.

    .. attribute:: blocks

        A dict with the current mapping of blocks in the template.  The keys
        in this dict are the names of the blocks, and the values a list of
        blocks registered.  The last item in each list is the current active
        block (latest in the inheritance chain).

    .. attribute:: eval_ctx

        The current :ref:`eval-context`.

    .. automethod:: jinja2.runtime.Context.call(callable, \*args, \**kwargs)


.. admonition:: Implementation

    Context is immutable for the same reason Python's frame locals are
    immutable inside functions.  Both Jinja2 and Python are not using the
    context / frame locals as data storage for variables but only as primary
    data source.

    When a template accesses a variable the template does not define, Jinja2
    looks up the variable in the context, after that the variable is treated
    as if it was defined in the template.


.. _loaders:

Loaders
-------

Loaders are responsible for loading templates from a resource such as the
file system.  The environment will keep the compiled modules in memory like
Python's `sys.modules`.  Unlike `sys.modules` however this cache is limited in
size by default and templates are automatically reloaded.
All loaders are subclasses of :class:`BaseLoader`.  If you want to create your
own loader, subclass :class:`BaseLoader` and override `get_source`.

.. autoclass:: jinja2.BaseLoader
    :members: get_source, load

Here a list of the builtin loaders Jinja2 provides:

.. autoclass:: jinja2.FileSystemLoader

.. autoclass:: jinja2.PackageLoader

.. autoclass:: jinja2.DictLoader

.. autoclass:: jinja2.FunctionLoader

.. autoclass:: jinja2.PrefixLoader

.. autoclass:: jinja2.ChoiceLoader

.. autoclass:: jinja2.ModuleLoader


.. _bytecode-cache:

Bytecode Cache
--------------

Jinja 2.1 and higher support external bytecode caching.  Bytecode caches make
it possible to store the generated bytecode on the file system or a different
location to avoid parsing the templates on first use.

This is especially useful if you have a web application that is initialized on
the first request and Jinja compiles many templates at once which slows down
the application.

To use a bytecode cache, instantiate it and pass it to the :class:`Environment`.

.. autoclass:: jinja2.BytecodeCache
    :members: load_bytecode, dump_bytecode, clear

.. autoclass:: jinja2.bccache.Bucket
    :members: write_bytecode, load_bytecode, bytecode_from_string,
              bytecode_to_string, reset

    .. attribute:: environment

        The :class:`Environment` that created the bucket.

    .. attribute:: key

        The unique cache key for this bucket

    .. attribute:: code

        The bytecode if it's loaded, otherwise `None`.


Builtin bytecode caches:

.. autoclass:: jinja2.FileSystemBytecodeCache

.. autoclass:: jinja2.MemcachedBytecodeCache


Utilities
---------

These helper functions and classes are useful if you add custom filters or
functions to a Jinja2 environment.

.. autofunction:: jinja2.environmentfilter

.. autofunction:: jinja2.contextfilter

.. autofunction:: jinja2.evalcontextfilter

.. autofunction:: jinja2.environmentfunction

.. autofunction:: jinja2.contextfunction

.. autofunction:: jinja2.evalcontextfunction

.. function:: escape(s)

    Convert the characters ``&``, ``<``, ``>``, ``'``, and ``"`` in string `s`
    to HTML-safe sequences.  Use this if you need to display text that might
    contain such characters in HTML.  This function will not escaped objects
    that do have an HTML representation such as already escaped data.

    The return value is a :class:`Markup` string.

.. autofunction:: jinja2.clear_caches

.. autofunction:: jinja2.is_undefined

.. autoclass:: jinja2.Markup([string])
    :members: escape, unescape, striptags

.. admonition:: Note

    The Jinja2 :class:`Markup` class is compatible with at least Pylons and
    Genshi.  It's expected that more template engines and framework will pick
    up the `__html__` concept soon.


Exceptions
----------

.. autoexception:: jinja2.TemplateError

.. autoexception:: jinja2.UndefinedError

.. autoexception:: jinja2.TemplateNotFound

.. autoexception:: jinja2.TemplatesNotFound

.. autoexception:: jinja2.TemplateSyntaxError

    .. attribute:: message

        The error message as utf-8 bytestring.

    .. attribute:: lineno

        The line number where the error occurred

    .. attribute:: name

        The load name for the template as unicode string.

    .. attribute:: filename

        The filename that loaded the template as bytestring in the encoding
        of the file system (most likely utf-8 or mbcs on Windows systems).

    The reason why the filename and error message are bytestrings and not
    unicode strings is that Python 2.x is not using unicode for exceptions
    and tracebacks as well as the compiler.  This will change with Python 3.

.. autoexception:: jinja2.TemplateAssertionError


.. _writing-filters:

Custom Filters
--------------

Custom filters are just regular Python functions that take the left side of
the filter as first argument and the the arguments passed to the filter as
extra arguments or keyword arguments.

For example in the filter ``{{ 42|myfilter(23) }}`` the function would be
called with ``myfilter(42, 23)``.  Here for example a simple filter that can
be applied to datetime objects to format them::

    def datetimeformat(value, format='%H:%M / %d-%m-%Y'):
        return value.strftime(format)

You can register it on the template environment by updating the
:attr:`~Environment.filters` dict on the environment::

    environment.filters['datetimeformat'] = datetimeformat

Inside the template it can then be used as follows:

.. sourcecode:: jinja

    written on: {{ article.pub_date|datetimeformat }}
    publication date: {{ article.pub_date|datetimeformat('%d-%m-%Y') }}

Filters can also be passed the current template context or environment.  This
is useful if a filter wants to return an undefined value or check the current
:attr:`~Environment.autoescape` setting.  For this purpose three decorators
exist: :func:`environmentfilter`, :func:`contextfilter` and
:func:`evalcontextfilter`.

Here a small example filter that breaks a text into HTML line breaks and
paragraphs and marks the return value as safe HTML string if autoescaping is
enabled::

    import re
    from jinja2 import evalcontextfilter, Markup, escape

    _paragraph_re = re.compile(r'(?:\r\n|\r|\n){2,}')

    @evalcontextfilter
    def nl2br(eval_ctx, value):
        result = u'\n\n'.join(u'<p>%s</p>' % p.replace('\n', Markup('<br>\n'))
                              for p in _paragraph_re.split(escape(value)))
        if eval_ctx.autoescape:
            result = Markup(result)
        return result

Context filters work the same just that the first argument is the current
active :class:`Context` rather then the environment.


.. _eval-context:

Evaluation Context
------------------

The evaluation context (short eval context or eval ctx) is a new object
introduced in Jinja 2.4 that makes it possible to activate and deactivate
compiled features at runtime.

Currently it is only used to enable and disable the automatic escaping but
can be used for extensions as well.

In previous Jinja versions filters and functions were marked as
environment callables in order to check for the autoescape status from the
environment.  In new versions it's encouraged to check the setting from the
evaluation context instead.

Previous versions::

    @environmentfilter
    def filter(env, value):
        result = do_something(value)
        if env.autoescape:
            result = Markup(result)
        return result

In new versions you can either use a :func:`contextfilter` and access the
evaluation context from the actual context, or use a
:func:`evalcontextfilter` which directly passes the evaluation context to
the function::

    @contextfilter
    def filter(context, value):
        result = do_something(value)
        if context.eval_ctx.autoescape:
            result = Markup(result)
        return result

    @evalcontextfilter
    def filter(eval_ctx, value):
        result = do_something(value)
        if eval_ctx.autoescape:
            result = Markup(result)
        return result

The evaluation context must not be modified at runtime.  Modifications
must only happen with a :class:`nodes.EvalContextModifier` and
:class:`nodes.ScopedEvalContextModifier` from an extension, not on the
eval context object itself.

.. autoclass:: jinja2.nodes.EvalContext

   .. attribute:: autoescape

      `True` or `False` depending on if autoescaping is active or not.

   .. attribute:: volatile

      `True` if the compiler cannot evaluate some expressions at compile
      time.  At runtime this should always be `False`.


.. _writing-tests:

Custom Tests
------------

Tests work like filters just that there is no way for a test to get access
to the environment or context and that they can't be chained.  The return
value of a test should be `True` or `False`.  The purpose of a test is to
give the template designers the possibility to perform type and conformability
checks.

Here a simple test that checks if a variable is a prime number::

    import math

    def is_prime(n):
        if n == 2:
            return True
        for i in xrange(2, int(math.ceil(math.sqrt(n))) + 1):
            if n % i == 0:
                return False
        return True
        

You can register it on the template environment by updating the
:attr:`~Environment.tests` dict on the environment::

    environment.tests['prime'] = is_prime

A template designer can then use the test like this:

.. sourcecode:: jinja

    {% if 42 is prime %}
        42 is a prime number
    {% else %}
        42 is not a prime number
    {% endif %}


.. _global-namespace:

The Global Namespace
--------------------

Variables stored in the :attr:`Environment.globals` dict are special as they
are available for imported templates too, even if they are imported without
context.  This is the place where you can put variables and functions
that should be available all the time.  Additionally :attr:`Template.globals`
exist that are variables available to a specific template that are available
to all :meth:`~Template.render` calls.


.. _low-level-api:

Low Level API
-------------

The low level API exposes functionality that can be useful to understand some
implementation details, debugging purposes or advanced :ref:`extension
<jinja-extensions>` techniques.  Unless you know exactly what you are doing we
don't recommend using any of those.

.. automethod:: Environment.lex

.. automethod:: Environment.parse

.. automethod:: Environment.preprocess

.. automethod:: Template.new_context

.. method:: Template.root_render_func(context)

    This is the low level render function.  It's passed a :class:`Context`
    that has to be created by :meth:`new_context` of the same template or
    a compatible template.  This render function is generated by the
    compiler from the template code and returns a generator that yields
    unicode strings.

    If an exception in the template code happens the template engine will
    not rewrite the exception but pass through the original one.  As a
    matter of fact this function should only be called from within a
    :meth:`render` / :meth:`generate` / :meth:`stream` call.

.. attribute:: Template.blocks

    A dict of block render functions.  Each of these functions works exactly
    like the :meth:`root_render_func` with the same limitations.

.. attribute:: Template.is_up_to_date

    This attribute is `False` if there is a newer version of the template
    available, otherwise `True`.

.. admonition:: Note

    The low-level API is fragile.  Future Jinja2 versions will try not to
    change it in a backwards incompatible way but modifications in the Jinja2
    core may shine through.  For example if Jinja2 introduces a new AST node
    in later versions that may be returned by :meth:`~Environment.parse`.

The Meta API
------------

.. versionadded:: 2.2

The meta API returns some information about abstract syntax trees that
could help applications to implement more advanced template concepts.  All
the functions of the meta API operate on an abstract syntax tree as
returned by the :meth:`Environment.parse` method.

.. autofunction:: jinja2.meta.find_undeclared_variables

.. autofunction:: jinja2.meta.find_referenced_templates
