Sandbox
=======

The Jinja2 sandbox can be used to evaluate untrusted code.  Access to unsafe
attributes and methods is prohibited.

Assuming `env` is a :class:`SandboxedEnvironment` in the default configuration
the following piece of code shows how it works:

>>> env.from_string("{{ func.func_code }}").render(func=lambda:None)
u''
>>> env.from_string("{{ func.func_code.do_something }}").render(func=lambda:None)
Traceback (most recent call last):
  ...
SecurityError: access to attribute 'func_code' of 'function' object is unsafe.

API
---

.. module:: jinja2.sandbox

.. autoclass:: SandboxedEnvironment([options])
    :members: is_safe_attribute, is_safe_callable, default_binop_table,
              default_unop_table, intercepted_binops, intercepted_unops,
              call_binop, call_unop

.. autoclass:: ImmutableSandboxedEnvironment([options])

.. autoexception:: SecurityError

.. autofunction:: unsafe

.. autofunction:: is_internal_attribute

.. autofunction:: modifies_known_mutable

.. admonition:: Note

    The Jinja2 sandbox alone is no solution for perfect security.  Especially
    for web applications you have to keep in mind that users may create
    templates with arbitrary HTML in so it's crucial to ensure that (if you
    are running multiple users on the same server) they can't harm each other
    via JavaScript insertions and much more.

    Also the sandbox is only as good as the configuration.  We strongly
    recommend only passing non-shared resources to the template and use
    some sort of whitelisting for attributes.

    Also keep in mind that templates may raise runtime or compile time errors,
    so make sure to catch them.

Operator Intercepting
---------------------

.. versionadded:: 2.6

For maximum performance Jinja2 will let operators call directly the type
specific callback methods.  This means that it's not possible to have this
intercepted by overriding :meth:`Environment.call`.  Furthermore a
conversion from operator to special method is not always directly possible
due to how operators work.  For instance for divisions more than one
special method exist.

With Jinja 2.6 there is now support for explicit operator intercepting.
This can be used to customize specific operators as necessary.  In order
to intercept an operator one has to override the
:attr:`SandboxedEnvironment.intercepted_binops` attribute.  Once the
operator that needs to be intercepted is added to that set Jinja2 will
generate bytecode that calls the :meth:`SandboxedEnvironment.call_binop`
function.  For unary operators the `unary` attributes and methods have to
be used instead.

The default implementation of :attr:`SandboxedEnvironment.call_binop`
will use the :attr:`SandboxedEnvironment.binop_table` to translate
operator symbols into callbacks performing the default operator behavior.

This example shows how the power (``**``) operator can be disabled in
Jinja2::

    from jinja2.sandbox import SandboxedEnvironment


    class MyEnvironment(SandboxedEnvironment):
        intercepted_binops = frozenset(['**'])

        def call_binop(self, context, operator, left, right):
            if operator == '**':
                return self.undefined('the power operator is unavailable')
            return SandboxedEnvironment.call_binop(self, context,
                                                   operator, left, right)

Make sure to always call into the super method, even if you are not
intercepting the call.  Jinja2 might internally call the method to
evaluate expressions.
