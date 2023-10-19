import sys
import typing as t
from types import CodeType
from types import TracebackType

from .exceptions import TemplateSyntaxError
from .utils import internal_code
from .utils import missing

if t.TYPE_CHECKING:
    from .runtime import Context


def rewrite_traceback_stack(source: t.Optional[str] = None) -> BaseException:
    """Rewrite the current exception to replace any tracebacks from
    within compiled template code with tracebacks that look like they
    came from the template source.

    This must be called within an ``except`` block.

    :param source: For ``TemplateSyntaxError``, the original source if
        known.
    :return: The original exception with the rewritten traceback.
    """
    _, exc_value, tb = sys.exc_info()
    exc_value = t.cast(BaseException, exc_value)
    tb = t.cast(TracebackType, tb)

    if isinstance(exc_value, TemplateSyntaxError) and not exc_value.translated:
        exc_value.translated = True
        exc_value.source = source
        # Remove the old traceback, otherwise the frames from the
        # compiler still show up.
        exc_value.with_traceback(None)
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
        tb.tb_next = tb_next
        tb_next = tb

    return exc_value.with_traceback(tb_next)


def fake_traceback(  # type: ignore
    exc_value: BaseException, tb: t.Optional[TracebackType], filename: str, lineno: int
) -> TracebackType:
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
    code: CodeType = compile(
        "\n" * (lineno - 1) + "raise __jinja_exception__", filename, "exec"
    )

    # Build a new code object that points to the template file and
    # replaces the location with a block name.
    location = "template"

    if tb is not None:
        function = tb.tb_frame.f_code.co_name

        if function == "root":
            location = "top-level template code"
        elif function.startswith("block_"):
            location = f"block {function[6:]!r}"

    if sys.version_info >= (3, 8):
        code = code.replace(co_name=location)
    else:
        code = CodeType(
            code.co_argcount,
            code.co_kwonlyargcount,
            code.co_nlocals,
            code.co_stacksize,
            code.co_flags,
            code.co_code,
            code.co_consts,
            code.co_names,
            code.co_varnames,
            code.co_filename,
            location,
            code.co_firstlineno,
            code.co_lnotab,
            code.co_freevars,
            code.co_cellvars,
        )

    # Execute the new code, which is guaranteed to raise, and return
    # the new traceback without this frame.
    try:
        exec(code, globals, locals)
    except BaseException:
        return sys.exc_info()[2].tb_next  # type: ignore


def get_template_locals(real_locals: t.Mapping[str, t.Any]) -> t.Dict[str, t.Any]:
    """Based on the runtime locals, get the context that would be
    available at that point in the template.
    """
    # Start with the current template context.
    ctx: "t.Optional[Context]" = real_locals.get("context")

    if ctx is not None:
        data: t.Dict[str, t.Any] = ctx.get_all().copy()
    else:
        data = {}

    # Might be in a derived context that only sets local variables
    # rather than pushing a context. Local variables follow the scheme
    # l_depth_name. Find the highest-depth local that has a value for
    # each name.
    local_overrides: t.Dict[str, t.Tuple[int, t.Any]] = {}

    for name, value in real_locals.items():
        if not name.startswith("l_") or value is missing:
            # Not a template variable, or no longer relevant.
            continue

        try:
            _, depth_str, name = name.split("_", 2)
            depth = int(depth_str)
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
