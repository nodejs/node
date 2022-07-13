import sys
from types import CodeType

from . import TemplateSyntaxError
from ._compat import PYPY
from .utils import internal_code
from .utils import missing


def rewrite_traceback_stack(source=None):
    """Rewrite the current exception to replace any tracebacks from
    within compiled template code with tracebacks that look like they
    came from the template source.

    This must be called within an ``except`` block.

    :param exc_info: A :meth:`sys.exc_info` tuple. If not provided,
        the current ``exc_info`` is used.
    :param source: For ``TemplateSyntaxError``, the original source if
        known.
    :return: A :meth:`sys.exc_info` tuple that can be re-raised.
    """
    exc_type, exc_value, tb = sys.exc_info()

    if isinstance(exc_value, TemplateSyntaxError) and not exc_value.translated:
        exc_value.translated = True
        exc_value.source = source

        try:
            # Remove the old traceback on Python 3, otherwise the frames
            # from the compiler still show up.
            exc_value.with_traceback(None)
        except AttributeError:
            pass

        # Outside of runtime, so the frame isn't executing template
        # code, but it still needs to point at the template.
        tb = fake_traceback(
            exc_value, None, exc_value.filename or "<unknown>", exc_value.lineno
        )
    else:
        # Skip the frame for the render function.
        tb = tb.tb_next

    stack = []

    # Build the stack of traceback object, replacing any in template
    # code with the source file and line information.
    while tb is not None:
        # Skip frames decorated with @internalcode. These are internal
        # calls that aren't useful in template debugging output.
        if tb.tb_frame.f_code in internal_code:
            tb = tb.tb_next
            continue

        template = tb.tb_frame.f_globals.get("__jinja_template__")

        if template is not None:
            lineno = template.get_corresponding_lineno(tb.tb_lineno)
            fake_tb = fake_traceback(exc_value, tb, template.filename, lineno)
            stack.append(fake_tb)
        else:
            stack.append(tb)

        tb = tb.tb_next

    tb_next = None

    # Assign tb_next in reverse to avoid circular references.
    for tb in reversed(stack):
        tb_next = tb_set_next(tb, tb_next)

    return exc_type, exc_value, tb_next


def fake_traceback(exc_value, tb, filename, lineno):
    """Produce a new traceback object that looks like it came from the
    template source instead of the compiled code. The filename, line
    number, and location name will point to the template, and the local
    variables will be the current template context.

    :param exc_value: The original exception to be re-raised to create
        the new traceback.
    :param tb: The original traceback to get the local variables and
        code info from.
    :param filename: The template filename.
    :param lineno: The line number in the template source.
    """
    if tb is not None:
        # Replace the real locals with the context that would be
        # available at that point in the template.
        locals = get_template_locals(tb.tb_frame.f_locals)
        locals.pop("__jinja_exception__", None)
    else:
        locals = {}

    globals = {
        "__name__": filename,
        "__file__": filename,
        "__jinja_exception__": exc_value,
    }
    # Raise an exception at the correct line number.
    code = compile("\n" * (lineno - 1) + "raise __jinja_exception__", filename, "exec")

    # Build a new code object that points to the template file and
    # replaces the location with a block name.
    try:
        location = "template"

        if tb is not None:
            function = tb.tb_frame.f_code.co_name

            if function == "root":
                location = "top-level template code"
            elif function.startswith("block_"):
                location = 'block "%s"' % function[6:]

        # Collect arguments for the new code object. CodeType only
        # accepts positional arguments, and arguments were inserted in
        # new Python versions.
        code_args = []

        for attr in (
            "argcount",
            "posonlyargcount",  # Python 3.8
            "kwonlyargcount",  # Python 3
            "nlocals",
            "stacksize",
            "flags",
            "code",  # codestring
            "consts",  # constants
            "names",
            "varnames",
            ("filename", filename),
            ("name", location),
            "firstlineno",
            "lnotab",
            "freevars",
            "cellvars",
        ):
            if isinstance(attr, tuple):
                # Replace with given value.
                code_args.append(attr[1])
                continue

            try:
                # Copy original value if it exists.
                code_args.append(getattr(code, "co_" + attr))
            except AttributeError:
                # Some arguments were added later.
                continue

        code = CodeType(*code_args)
    except Exception:
        # Some environments such as Google App Engine don't support
        # modifying code objects.
        pass

    # Execute the new code, which is guaranteed to raise, and return
    # the new traceback without this frame.
    try:
        exec(code, globals, locals)
    except BaseException:
        return sys.exc_info()[2].tb_next


def get_template_locals(real_locals):
    """Based on the runtime locals, get the context that would be
    available at that point in the template.
    """
    # Start with the current template context.
    ctx = real_locals.get("context")

    if ctx:
        data = ctx.get_all().copy()
    else:
        data = {}

    # Might be in a derived context that only sets local variables
    # rather than pushing a context. Local variables follow the scheme
    # l_depth_name. Find the highest-depth local that has a value for
    # each name.
    local_overrides = {}

    for name, value in real_locals.items():
        if not name.startswith("l_") or value is missing:
            # Not a template variable, or no longer relevant.
            continue

        try:
            _, depth, name = name.split("_", 2)
            depth = int(depth)
        except ValueError:
            continue

        cur_depth = local_overrides.get(name, (-1,))[0]

        if cur_depth < depth:
            local_overrides[name] = (depth, value)

    # Modify the context with any derived context.
    for name, (_, value) in local_overrides.items():
        if value is missing:
            data.pop(name, None)
        else:
            data[name] = value

    return data


if sys.version_info >= (3, 7):
    # tb_next is directly assignable as of Python 3.7
    def tb_set_next(tb, tb_next):
        tb.tb_next = tb_next
        return tb


elif PYPY:
    # PyPy might have special support, and won't work with ctypes.
    try:
        import tputil
    except ImportError:
        # Without tproxy support, use the original traceback.
        def tb_set_next(tb, tb_next):
            return tb

    else:
        # With tproxy support, create a proxy around the traceback that
        # returns the new tb_next.
        def tb_set_next(tb, tb_next):
            def controller(op):
                if op.opname == "__getattribute__" and op.args[0] == "tb_next":
                    return tb_next

                return op.delegate()

            return tputil.make_proxy(controller, obj=tb)


else:
    # Use ctypes to assign tb_next at the C level since it's read-only
    # from Python.
    import ctypes

    class _CTraceback(ctypes.Structure):
        _fields_ = [
            # Extra PyObject slots when compiled with Py_TRACE_REFS.
            ("PyObject_HEAD", ctypes.c_byte * object().__sizeof__()),
            # Only care about tb_next as an object, not a traceback.
            ("tb_next", ctypes.py_object),
        ]

    def tb_set_next(tb, tb_next):
        c_tb = _CTraceback.from_address(id(tb))

        # Clear out the old tb_next.
        if tb.tb_next is not None:
            c_tb_next = ctypes.py_object(tb.tb_next)
            c_tb.tb_next = ctypes.py_object()
            ctypes.pythonapi.Py_DecRef(c_tb_next)

        # Assign the new tb_next.
        if tb_next is not None:
            c_tb_next = ctypes.py_object(tb_next)
            ctypes.pythonapi.Py_IncRef(c_tb_next)
            c_tb.tb_next = c_tb_next

        return tb
