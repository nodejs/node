# -*- coding: utf-8 -*-
from ._compat import imap
from ._compat import implements_to_string
from ._compat import PY2
from ._compat import text_type


class TemplateError(Exception):
    """Baseclass for all template errors."""

    if PY2:

        def __init__(self, message=None):
            if message is not None:
                message = text_type(message).encode("utf-8")
            Exception.__init__(self, message)

        @property
        def message(self):
            if self.args:
                message = self.args[0]
                if message is not None:
                    return message.decode("utf-8", "replace")

        def __unicode__(self):
            return self.message or u""

    else:

        def __init__(self, message=None):
            Exception.__init__(self, message)

        @property
        def message(self):
            if self.args:
                message = self.args[0]
                if message is not None:
                    return message


@implements_to_string
class TemplateNotFound(IOError, LookupError, TemplateError):
    """Raised if a template does not exist.

    .. versionchanged:: 2.11
        If the given name is :class:`Undefined` and no message was
        provided, an :exc:`UndefinedError` is raised.
    """

    # looks weird, but removes the warning descriptor that just
    # bogusly warns us about message being deprecated
    message = None

    def __init__(self, name, message=None):
        IOError.__init__(self, name)

        if message is None:
            from .runtime import Undefined

            if isinstance(name, Undefined):
                name._fail_with_undefined_error()

            message = name

        self.message = message
        self.name = name
        self.templates = [name]

    def __str__(self):
        return self.message


class TemplatesNotFound(TemplateNotFound):
    """Like :class:`TemplateNotFound` but raised if multiple templates
    are selected.  This is a subclass of :class:`TemplateNotFound`
    exception, so just catching the base exception will catch both.

    .. versionchanged:: 2.11
        If a name in the list of names is :class:`Undefined`, a message
        about it being undefined is shown rather than the empty string.

    .. versionadded:: 2.2
    """

    def __init__(self, names=(), message=None):
        if message is None:
            from .runtime import Undefined

            parts = []

            for name in names:
                if isinstance(name, Undefined):
                    parts.append(name._undefined_message)
                else:
                    parts.append(name)

            message = u"none of the templates given were found: " + u", ".join(
                imap(text_type, parts)
            )
        TemplateNotFound.__init__(self, names and names[-1] or None, message)
        self.templates = list(names)


@implements_to_string
class TemplateSyntaxError(TemplateError):
    """Raised to tell the user that there is a problem with the template."""

    def __init__(self, message, lineno, name=None, filename=None):
        TemplateError.__init__(self, message)
        self.lineno = lineno
        self.name = name
        self.filename = filename
        self.source = None

        # this is set to True if the debug.translate_syntax_error
        # function translated the syntax error into a new traceback
        self.translated = False

    def __str__(self):
        # for translated errors we only return the message
        if self.translated:
            return self.message

        # otherwise attach some stuff
        location = "line %d" % self.lineno
        name = self.filename or self.name
        if name:
            location = 'File "%s", %s' % (name, location)
        lines = [self.message, "  " + location]

        # if the source is set, add the line to the output
        if self.source is not None:
            try:
                line = self.source.splitlines()[self.lineno - 1]
            except IndexError:
                line = None
            if line:
                lines.append("    " + line.strip())

        return u"\n".join(lines)

    def __reduce__(self):
        # https://bugs.python.org/issue1692335 Exceptions that take
        # multiple required arguments have problems with pickling.
        # Without this, raises TypeError: __init__() missing 1 required
        # positional argument: 'lineno'
        return self.__class__, (self.message, self.lineno, self.name, self.filename)


class TemplateAssertionError(TemplateSyntaxError):
    """Like a template syntax error, but covers cases where something in the
    template caused an error at compile time that wasn't necessarily caused
    by a syntax error.  However it's a direct subclass of
    :exc:`TemplateSyntaxError` and has the same attributes.
    """


class TemplateRuntimeError(TemplateError):
    """A generic runtime error in the template engine.  Under some situations
    Jinja may raise this exception.
    """


class UndefinedError(TemplateRuntimeError):
    """Raised if a template tries to operate on :class:`Undefined`."""


class SecurityError(TemplateRuntimeError):
    """Raised if a template tries to do something insecure if the
    sandbox is enabled.
    """


class FilterArgumentError(TemplateRuntimeError):
    """This error is raised if a filter was called with inappropriate
    arguments
    """
