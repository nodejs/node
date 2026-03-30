"""Compiles nodes from the parser into Python code."""
import typing as t
from contextlib import contextmanager
from functools import update_wrapper
from io import StringIO
from itertools import chain
from keyword import iskeyword as is_python_keyword

from markupsafe import escape
from markupsafe import Markup

from . import nodes
from .exceptions import TemplateAssertionError
from .idtracking import Symbols
from .idtracking import VAR_LOAD_ALIAS
from .idtracking import VAR_LOAD_PARAMETER
from .idtracking import VAR_LOAD_RESOLVE
from .idtracking import VAR_LOAD_UNDEFINED
from .nodes import EvalContext
from .optimizer import Optimizer
from .utils import _PassArg
from .utils import concat
from .visitor import NodeVisitor

if t.TYPE_CHECKING:
    import typing_extensions as te
    from .environment import Environment

F = t.TypeVar("F", bound=t.Callable[..., t.Any])

operators = {
    "eq": "==",
    "ne": "!=",
    "gt": ">",
    "gteq": ">=",
    "lt": "<",
    "lteq": "<=",
    "in": "in",
    "notin": "not in",
}


def optimizeconst(f: F) -> F:
    def new_func(
        self: "CodeGenerator", node: nodes.Expr, frame: "Frame", **kwargs: t.Any
    ) -> t.Any:
        # Only optimize if the frame is not volatile
        if self.optimizer is not None and not frame.eval_ctx.volatile:
            new_node = self.optimizer.visit(node, frame.eval_ctx)

            if new_node != node:
                return self.visit(new_node, frame)

        return f(self, node, frame, **kwargs)

    return update_wrapper(t.cast(F, new_func), f)


def _make_binop(op: str) -> t.Callable[["CodeGenerator", nodes.BinExpr, "Frame"], None]:
    @optimizeconst
    def visitor(self: "CodeGenerator", node: nodes.BinExpr, frame: Frame) -> None:
        if (
            self.environment.sandboxed
            and op in self.environment.intercepted_binops  # type: ignore
        ):
            self.write(f"environment.call_binop(context, {op!r}, ")
            self.visit(node.left, frame)
            self.write(", ")
            self.visit(node.right, frame)
        else:
            self.write("(")
            self.visit(node.left, frame)
            self.write(f" {op} ")
            self.visit(node.right, frame)

        self.write(")")

    return visitor


def _make_unop(
    op: str,
) -> t.Callable[["CodeGenerator", nodes.UnaryExpr, "Frame"], None]:
    @optimizeconst
    def visitor(self: "CodeGenerator", node: nodes.UnaryExpr, frame: Frame) -> None:
        if (
            self.environment.sandboxed
            and op in self.environment.intercepted_unops  # type: ignore
        ):
            self.write(f"environment.call_unop(context, {op!r}, ")
            self.visit(node.node, frame)
        else:
            self.write("(" + op)
            self.visit(node.node, frame)

        self.write(")")

    return visitor


def generate(
    node: nodes.Template,
    environment: "Environment",
    name: t.Optional[str],
    filename: t.Optional[str],
    stream: t.Optional[t.TextIO] = None,
    defer_init: bool = False,
    optimized: bool = True,
) -> t.Optional[str]:
    """Generate the python source for a node tree."""
    if not isinstance(node, nodes.Template):
        raise TypeError("Can't compile non template nodes")

    generator = environment.code_generator_class(
        environment, name, filename, stream, defer_init, optimized
    )
    generator.visit(node)

    if stream is None:
        return generator.stream.getvalue()  # type: ignore

    return None


def has_safe_repr(value: t.Any) -> bool:
    """Does the node have a safe representation?"""
    if value is None or value is NotImplemented or value is Ellipsis:
        return True

    if type(value) in {bool, int, float, complex, range, str, Markup}:
        return True

    if type(value) in {tuple, list, set, frozenset}:
        return all(has_safe_repr(v) for v in value)

    if type(value) is dict:
        return all(has_safe_repr(k) and has_safe_repr(v) for k, v in value.items())

    return False


def find_undeclared(
    nodes: t.Iterable[nodes.Node], names: t.Iterable[str]
) -> t.Set[str]:
    """Check if the names passed are accessed undeclared.  The return value
    is a set of all the undeclared names from the sequence of names found.
    """
    visitor = UndeclaredNameVisitor(names)
    try:
        for node in nodes:
            visitor.visit(node)
    except VisitorExit:
        pass
    return visitor.undeclared


class MacroRef:
    def __init__(self, node: t.Union[nodes.Macro, nodes.CallBlock]) -> None:
        self.node = node
        self.accesses_caller = False
        self.accesses_kwargs = False
        self.accesses_varargs = False


class Frame:
    """Holds compile time information for us."""

    def __init__(
        self,
        eval_ctx: EvalContext,
        parent: t.Optional["Frame"] = None,
        level: t.Optional[int] = None,
    ) -> None:
        self.eval_ctx = eval_ctx

        # the parent of this frame
        self.parent = parent

        if parent is None:
            self.symbols = Symbols(level=level)

            # in some dynamic inheritance situations the compiler needs to add
            # write tests around output statements.
            self.require_output_check = False

            # inside some tags we are using a buffer rather than yield statements.
            # this for example affects {% filter %} or {% macro %}.  If a frame
            # is buffered this variable points to the name of the list used as
            # buffer.
            self.buffer: t.Optional[str] = None

            # the name of the block we're in, otherwise None.
            self.block: t.Optional[str] = None

        else:
            self.symbols = Symbols(parent.symbols, level=level)
            self.require_output_check = parent.require_output_check
            self.buffer = parent.buffer
            self.block = parent.block

        # a toplevel frame is the root + soft frames such as if conditions.
        self.toplevel = False

        # the root frame is basically just the outermost frame, so no if
        # conditions.  This information is used to optimize inheritance
        # situations.
        self.rootlevel = False

        # variables set inside of loops and blocks should not affect outer frames,
        # but they still needs to be kept track of as part of the active context.
        self.loop_frame = False
        self.block_frame = False

        # track whether the frame is being used in an if-statement or conditional
        # expression as it determines which errors should be raised during runtime
        # or compile time.
        self.soft_frame = False

    def copy(self) -> "Frame":
        """Create a copy of the current one."""
        rv = object.__new__(self.__class__)
        rv.__dict__.update(self.__dict__)
        rv.symbols = self.symbols.copy()
        return rv

    def inner(self, isolated: bool = False) -> "Frame":
        """Return an inner frame."""
        if isolated:
            return Frame(self.eval_ctx, level=self.symbols.level + 1)
        return Frame(self.eval_ctx, self)

    def soft(self) -> "Frame":
        """Return a soft frame.  A soft frame may not be modified as
        standalone thing as it shares the resources with the frame it
        was created of, but it's not a rootlevel frame any longer.

        This is only used to implement if-statements and conditional
        expressions.
        """
        rv = self.copy()
        rv.rootlevel = False
        rv.soft_frame = True
        return rv

    __copy__ = copy


class VisitorExit(RuntimeError):
    """Exception used by the `UndeclaredNameVisitor` to signal a stop."""


class DependencyFinderVisitor(NodeVisitor):
    """A visitor that collects filter and test calls."""

    def __init__(self) -> None:
        self.filters: t.Set[str] = set()
        self.tests: t.Set[str] = set()

    def visit_Filter(self, node: nodes.Filter) -> None:
        self.generic_visit(node)
        self.filters.add(node.name)

    def visit_Test(self, node: nodes.Test) -> None:
        self.generic_visit(node)
        self.tests.add(node.name)

    def visit_Block(self, node: nodes.Block) -> None:
        """Stop visiting at blocks."""


class UndeclaredNameVisitor(NodeVisitor):
    """A visitor that checks if a name is accessed without being
    declared.  This is different from the frame visitor as it will
    not stop at closure frames.
    """

    def __init__(self, names: t.Iterable[str]) -> None:
        self.names = set(names)
        self.undeclared: t.Set[str] = set()

    def visit_Name(self, node: nodes.Name) -> None:
        if node.ctx == "load" and node.name in self.names:
            self.undeclared.add(node.name)
            if self.undeclared == self.names:
                raise VisitorExit()
        else:
            self.names.discard(node.name)

    def visit_Block(self, node: nodes.Block) -> None:
        """Stop visiting a blocks."""


class CompilerExit(Exception):
    """Raised if the compiler encountered a situation where it just
    doesn't make sense to further process the code.  Any block that
    raises such an exception is not further processed.
    """


class CodeGenerator(NodeVisitor):
    def __init__(
        self,
        environment: "Environment",
        name: t.Optional[str],
        filename: t.Optional[str],
        stream: t.Optional[t.TextIO] = None,
        defer_init: bool = False,
        optimized: bool = True,
    ) -> None:
        if stream is None:
            stream = StringIO()
        self.environment = environment
        self.name = name
        self.filename = filename
        self.stream = stream
        self.created_block_context = False
        self.defer_init = defer_init
        self.optimizer: t.Optional[Optimizer] = None

        if optimized:
            self.optimizer = Optimizer(environment)

        # aliases for imports
        self.import_aliases: t.Dict[str, str] = {}

        # a registry for all blocks.  Because blocks are moved out
        # into the global python scope they are registered here
        self.blocks: t.Dict[str, nodes.Block] = {}

        # the number of extends statements so far
        self.extends_so_far = 0

        # some templates have a rootlevel extends.  In this case we
        # can safely assume that we're a child template and do some
        # more optimizations.
        self.has_known_extends = False

        # the current line number
        self.code_lineno = 1

        # registry of all filters and tests (global, not block local)
        self.tests: t.Dict[str, str] = {}
        self.filters: t.Dict[str, str] = {}

        # the debug information
        self.debug_info: t.List[t.Tuple[int, int]] = []
        self._write_debug_info: t.Optional[int] = None

        # the number of new lines before the next write()
        self._new_lines = 0

        # the line number of the last written statement
        self._last_line = 0

        # true if nothing was written so far.
        self._first_write = True

        # used by the `temporary_identifier` method to get new
        # unique, temporary identifier
        self._last_identifier = 0

        # the current indentation
        self._indentation = 0

        # Tracks toplevel assignments
        self._assign_stack: t.List[t.Set[str]] = []

        # Tracks parameter definition blocks
        self._param_def_block: t.List[t.Set[str]] = []

        # Tracks the current context.
        self._context_reference_stack = ["context"]

    @property
    def optimized(self) -> bool:
        return self.optimizer is not None

    # -- Various compilation helpers

    def fail(self, msg: str, lineno: int) -> "te.NoReturn":
        """Fail with a :exc:`TemplateAssertionError`."""
        raise TemplateAssertionError(msg, lineno, self.name, self.filename)

    def temporary_identifier(self) -> str:
        """Get a new unique identifier."""
        self._last_identifier += 1
        return f"t_{self._last_identifier}"

    def buffer(self, frame: Frame) -> None:
        """Enable buffering for the frame from that point onwards."""
        frame.buffer = self.temporary_identifier()
        self.writeline(f"{frame.buffer} = []")

    def return_buffer_contents(
        self, frame: Frame, force_unescaped: bool = False
    ) -> None:
        """Return the buffer contents of the frame."""
        if not force_unescaped:
            if frame.eval_ctx.volatile:
                self.writeline("if context.eval_ctx.autoescape:")
                self.indent()
                self.writeline(f"return Markup(concat({frame.buffer}))")
                self.outdent()
                self.writeline("else:")
                self.indent()
                self.writeline(f"return concat({frame.buffer})")
                self.outdent()
                return
            elif frame.eval_ctx.autoescape:
                self.writeline(f"return Markup(concat({frame.buffer}))")
                return
        self.writeline(f"return concat({frame.buffer})")

    def indent(self) -> None:
        """Indent by one."""
        self._indentation += 1

    def outdent(self, step: int = 1) -> None:
        """Outdent by step."""
        self._indentation -= step

    def start_write(self, frame: Frame, node: t.Optional[nodes.Node] = None) -> None:
        """Yield or write into the frame buffer."""
        if frame.buffer is None:
            self.writeline("yield ", node)
        else:
            self.writeline(f"{frame.buffer}.append(", node)

    def end_write(self, frame: Frame) -> None:
        """End the writing process started by `start_write`."""
        if frame.buffer is not None:
            self.write(")")

    def simple_write(
        self, s: str, frame: Frame, node: t.Optional[nodes.Node] = None
    ) -> None:
        """Simple shortcut for start_write + write + end_write."""
        self.start_write(frame, node)
        self.write(s)
        self.end_write(frame)

    def blockvisit(self, nodes: t.Iterable[nodes.Node], frame: Frame) -> None:
        """Visit a list of nodes as block in a frame.  If the current frame
        is no buffer a dummy ``if 0: yield None`` is written automatically.
        """
        try:
            self.writeline("pass")
            for node in nodes:
                self.visit(node, frame)
        except CompilerExit:
            pass

    def write(self, x: str) -> None:
        """Write a string into the output stream."""
        if self._new_lines:
            if not self._first_write:
                self.stream.write("\n" * self._new_lines)
                self.code_lineno += self._new_lines
                if self._write_debug_info is not None:
                    self.debug_info.append((self._write_debug_info, self.code_lineno))
                    self._write_debug_info = None
            self._first_write = False
            self.stream.write("    " * self._indentation)
            self._new_lines = 0
        self.stream.write(x)

    def writeline(
        self, x: str, node: t.Optional[nodes.Node] = None, extra: int = 0
    ) -> None:
        """Combination of newline and write."""
        self.newline(node, extra)
        self.write(x)

    def newline(self, node: t.Optional[nodes.Node] = None, extra: int = 0) -> None:
        """Add one or more newlines before the next write."""
        self._new_lines = max(self._new_lines, 1 + extra)
        if node is not None and node.lineno != self._last_line:
            self._write_debug_info = node.lineno
            self._last_line = node.lineno

    def signature(
        self,
        node: t.Union[nodes.Call, nodes.Filter, nodes.Test],
        frame: Frame,
        extra_kwargs: t.Optional[t.Mapping[str, t.Any]] = None,
    ) -> None:
        """Writes a function call to the stream for the current node.
        A leading comma is added automatically.  The extra keyword
        arguments may not include python keywords otherwise a syntax
        error could occur.  The extra keyword arguments should be given
        as python dict.
        """
        # if any of the given keyword arguments is a python keyword
        # we have to make sure that no invalid call is created.
        kwarg_workaround = any(
            is_python_keyword(t.cast(str, k))
            for k in chain((x.key for x in node.kwargs), extra_kwargs or ())
        )

        for arg in node.args:
            self.write(", ")
            self.visit(arg, frame)

        if not kwarg_workaround:
            for kwarg in node.kwargs:
                self.write(", ")
                self.visit(kwarg, frame)
            if extra_kwargs is not None:
                for key, value in extra_kwargs.items():
                    self.write(f", {key}={value}")
        if node.dyn_args:
            self.write(", *")
            self.visit(node.dyn_args, frame)

        if kwarg_workaround:
            if node.dyn_kwargs is not None:
                self.write(", **dict({")
            else:
                self.write(", **{")
            for kwarg in node.kwargs:
                self.write(f"{kwarg.key!r}: ")
                self.visit(kwarg.value, frame)
                self.write(", ")
            if extra_kwargs is not None:
                for key, value in extra_kwargs.items():
                    self.write(f"{key!r}: {value}, ")
            if node.dyn_kwargs is not None:
                self.write("}, **")
                self.visit(node.dyn_kwargs, frame)
                self.write(")")
            else:
                self.write("}")

        elif node.dyn_kwargs is not None:
            self.write(", **")
            self.visit(node.dyn_kwargs, frame)

    def pull_dependencies(self, nodes: t.Iterable[nodes.Node]) -> None:
        """Find all filter and test names used in the template and
        assign them to variables in the compiled namespace. Checking
        that the names are registered with the environment is done when
        compiling the Filter and Test nodes. If the node is in an If or
        CondExpr node, the check is done at runtime instead.

        .. versionchanged:: 3.0
            Filters and tests in If and CondExpr nodes are checked at
            runtime instead of compile time.
        """
        visitor = DependencyFinderVisitor()

        for node in nodes:
            visitor.visit(node)

        for id_map, names, dependency in (self.filters, visitor.filters, "filters"), (
            self.tests,
            visitor.tests,
            "tests",
        ):
            for name in sorted(names):
                if name not in id_map:
                    id_map[name] = self.temporary_identifier()

                # add check during runtime that dependencies used inside of executed
                # blocks are defined, as this step may be skipped during compile time
                self.writeline("try:")
                self.indent()
                self.writeline(f"{id_map[name]} = environment.{dependency}[{name!r}]")
                self.outdent()
                self.writeline("except KeyError:")
                self.indent()
                self.writeline("@internalcode")
                self.writeline(f"def {id_map[name]}(*unused):")
                self.indent()
                self.writeline(
                    f'raise TemplateRuntimeError("No {dependency[:-1]}'
                    f' named {name!r} found.")'
                )
                self.outdent()
                self.outdent()

    def enter_frame(self, frame: Frame) -> None:
        undefs = []
        for target, (action, param) in frame.symbols.loads.items():
            if action == VAR_LOAD_PARAMETER:
                pass
            elif action == VAR_LOAD_RESOLVE:
                self.writeline(f"{target} = {self.get_resolve_func()}({param!r})")
            elif action == VAR_LOAD_ALIAS:
                self.writeline(f"{target} = {param}")
            elif action == VAR_LOAD_UNDEFINED:
                undefs.append(target)
            else:
                raise NotImplementedError("unknown load instruction")
        if undefs:
            self.writeline(f"{' = '.join(undefs)} = missing")

    def leave_frame(self, frame: Frame, with_python_scope: bool = False) -> None:
        if not with_python_scope:
            undefs = []
            for target in frame.symbols.loads:
                undefs.append(target)
            if undefs:
                self.writeline(f"{' = '.join(undefs)} = missing")

    def choose_async(self, async_value: str = "async ", sync_value: str = "") -> str:
        return async_value if self.environment.is_async else sync_value

    def func(self, name: str) -> str:
        return f"{self.choose_async()}def {name}"

    def macro_body(
        self, node: t.Union[nodes.Macro, nodes.CallBlock], frame: Frame
    ) -> t.Tuple[Frame, MacroRef]:
        """Dump the function def of a macro or call block."""
        frame = frame.inner()
        frame.symbols.analyze_node(node)
        macro_ref = MacroRef(node)

        explicit_caller = None
        skip_special_params = set()
        args = []

        for idx, arg in enumerate(node.args):
            if arg.name == "caller":
                explicit_caller = idx
            if arg.name in ("kwargs", "varargs"):
                skip_special_params.add(arg.name)
            args.append(frame.symbols.ref(arg.name))

        undeclared = find_undeclared(node.body, ("caller", "kwargs", "varargs"))

        if "caller" in undeclared:
            # In older Jinja versions there was a bug that allowed caller
            # to retain the special behavior even if it was mentioned in
            # the argument list.  However thankfully this was only really
            # working if it was the last argument.  So we are explicitly
            # checking this now and error out if it is anywhere else in
            # the argument list.
            if explicit_caller is not None:
                try:
                    node.defaults[explicit_caller - len(node.args)]
                except IndexError:
                    self.fail(
                        "When defining macros or call blocks the "
                        'special "caller" argument must be omitted '
                        "or be given a default.",
                        node.lineno,
                    )
            else:
                args.append(frame.symbols.declare_parameter("caller"))
            macro_ref.accesses_caller = True
        if "kwargs" in undeclared and "kwargs" not in skip_special_params:
            args.append(frame.symbols.declare_parameter("kwargs"))
            macro_ref.accesses_kwargs = True
        if "varargs" in undeclared and "varargs" not in skip_special_params:
            args.append(frame.symbols.declare_parameter("varargs"))
            macro_ref.accesses_varargs = True

        # macros are delayed, they never require output checks
        frame.require_output_check = False
        frame.symbols.analyze_node(node)
        self.writeline(f"{self.func('macro')}({', '.join(args)}):", node)
        self.indent()

        self.buffer(frame)
        self.enter_frame(frame)

        self.push_parameter_definitions(frame)
        for idx, arg in enumerate(node.args):
            ref = frame.symbols.ref(arg.name)
            self.writeline(f"if {ref} is missing:")
            self.indent()
            try:
                default = node.defaults[idx - len(node.args)]
            except IndexError:
                self.writeline(
                    f'{ref} = undefined("parameter {arg.name!r} was not provided",'
                    f" name={arg.name!r})"
                )
            else:
                self.writeline(f"{ref} = ")
                self.visit(default, frame)
            self.mark_parameter_stored(ref)
            self.outdent()
        self.pop_parameter_definitions()

        self.blockvisit(node.body, frame)
        self.return_buffer_contents(frame, force_unescaped=True)
        self.leave_frame(frame, with_python_scope=True)
        self.outdent()

        return frame, macro_ref

    def macro_def(self, macro_ref: MacroRef, frame: Frame) -> None:
        """Dump the macro definition for the def created by macro_body."""
        arg_tuple = ", ".join(repr(x.name) for x in macro_ref.node.args)
        name = getattr(macro_ref.node, "name", None)
        if len(macro_ref.node.args) == 1:
            arg_tuple += ","
        self.write(
            f"Macro(environment, macro, {name!r}, ({arg_tuple}),"
            f" {macro_ref.accesses_kwargs!r}, {macro_ref.accesses_varargs!r},"
            f" {macro_ref.accesses_caller!r}, context.eval_ctx.autoescape)"
        )

    def position(self, node: nodes.Node) -> str:
        """Return a human readable position for the node."""
        rv = f"line {node.lineno}"
        if self.name is not None:
            rv = f"{rv} in {self.name!r}"
        return rv

    def dump_local_context(self, frame: Frame) -> str:
        items_kv = ", ".join(
            f"{name!r}: {target}"
            for name, target in frame.symbols.dump_stores().items()
        )
        return f"{{{items_kv}}}"

    def write_commons(self) -> None:
        """Writes a common preamble that is used by root and block functions.
        Primarily this sets up common local helpers and enforces a generator
        through a dead branch.
        """
        self.writeline("resolve = context.resolve_or_missing")
        self.writeline("undefined = environment.undefined")
        self.writeline("concat = environment.concat")
        # always use the standard Undefined class for the implicit else of
        # conditional expressions
        self.writeline("cond_expr_undefined = Undefined")
        self.writeline("if 0: yield None")

    def push_parameter_definitions(self, frame: Frame) -> None:
        """Pushes all parameter targets from the given frame into a local
        stack that permits tracking of yet to be assigned parameters.  In
        particular this enables the optimization from `visit_Name` to skip
        undefined expressions for parameters in macros as macros can reference
        otherwise unbound parameters.
        """
        self._param_def_block.append(frame.symbols.dump_param_targets())

    def pop_parameter_definitions(self) -> None:
        """Pops the current parameter definitions set."""
        self._param_def_block.pop()

    def mark_parameter_stored(self, target: str) -> None:
        """Marks a parameter in the current parameter definitions as stored.
        This will skip the enforced undefined checks.
        """
        if self._param_def_block:
            self._param_def_block[-1].discard(target)

    def push_context_reference(self, target: str) -> None:
        self._context_reference_stack.append(target)

    def pop_context_reference(self) -> None:
        self._context_reference_stack.pop()

    def get_context_ref(self) -> str:
        return self._context_reference_stack[-1]

    def get_resolve_func(self) -> str:
        target = self._context_reference_stack[-1]
        if target == "context":
            return "resolve"
        return f"{target}.resolve"

    def derive_context(self, frame: Frame) -> str:
        return f"{self.get_context_ref()}.derived({self.dump_local_context(frame)})"

    def parameter_is_undeclared(self, target: str) -> bool:
        """Checks if a given target is an undeclared parameter."""
        if not self._param_def_block:
            return False
        return target in self._param_def_block[-1]

    def push_assign_tracking(self) -> None:
        """Pushes a new layer for assignment tracking."""
        self._assign_stack.append(set())

    def pop_assign_tracking(self, frame: Frame) -> None:
        """Pops the topmost level for assignment tracking and updates the
        context variables if necessary.
        """
        vars = self._assign_stack.pop()
        if (
            not frame.block_frame
            and not frame.loop_frame
            and not frame.toplevel
            or not vars
        ):
            return
        public_names = [x for x in vars if x[:1] != "_"]
        if len(vars) == 1:
            name = next(iter(vars))
            ref = frame.symbols.ref(name)
            if frame.loop_frame:
                self.writeline(f"_loop_vars[{name!r}] = {ref}")
                return
            if frame.block_frame:
                self.writeline(f"_block_vars[{name!r}] = {ref}")
                return
            self.writeline(f"context.vars[{name!r}] = {ref}")
        else:
            if frame.loop_frame:
                self.writeline("_loop_vars.update({")
            elif frame.block_frame:
                self.writeline("_block_vars.update({")
            else:
                self.writeline("context.vars.update({")
            for idx, name in enumerate(vars):
                if idx:
                    self.write(", ")
                ref = frame.symbols.ref(name)
                self.write(f"{name!r}: {ref}")
            self.write("})")
        if not frame.block_frame and not frame.loop_frame and public_names:
            if len(public_names) == 1:
                self.writeline(f"context.exported_vars.add({public_names[0]!r})")
            else:
                names_str = ", ".join(map(repr, public_names))
                self.writeline(f"context.exported_vars.update(({names_str}))")

    # -- Statement Visitors

    def visit_Template(
        self, node: nodes.Template, frame: t.Optional[Frame] = None
    ) -> None:
        assert frame is None, "no root frame allowed"
        eval_ctx = EvalContext(self.environment, self.name)

        from .runtime import exported, async_exported

        if self.environment.is_async:
            exported_names = sorted(exported + async_exported)
        else:
            exported_names = sorted(exported)

        self.writeline("from jinja2.runtime import " + ", ".join(exported_names))

        # if we want a deferred initialization we cannot move the
        # environment into a local name
        envenv = "" if self.defer_init else ", environment=environment"

        # do we have an extends tag at all?  If not, we can save some
        # overhead by just not processing any inheritance code.
        have_extends = node.find(nodes.Extends) is not None

        # find all blocks
        for block in node.find_all(nodes.Block):
            if block.name in self.blocks:
                self.fail(f"block {block.name!r} defined twice", block.lineno)
            self.blocks[block.name] = block

        # find all imports and import them
        for import_ in node.find_all(nodes.ImportedName):
            if import_.importname not in self.import_aliases:
                imp = import_.importname
                self.import_aliases[imp] = alias = self.temporary_identifier()
                if "." in imp:
                    module, obj = imp.rsplit(".", 1)
                    self.writeline(f"from {module} import {obj} as {alias}")
                else:
                    self.writeline(f"import {imp} as {alias}")

        # add the load name
        self.writeline(f"name = {self.name!r}")

        # generate the root render function.
        self.writeline(
            f"{self.func('root')}(context, missing=missing{envenv}):", extra=1
        )
        self.indent()
        self.write_commons()

        # process the root
        frame = Frame(eval_ctx)
        if "self" in find_undeclared(node.body, ("self",)):
            ref = frame.symbols.declare_parameter("self")
            self.writeline(f"{ref} = TemplateReference(context)")
        frame.symbols.analyze_node(node)
        frame.toplevel = frame.rootlevel = True
        frame.require_output_check = have_extends and not self.has_known_extends
        if have_extends:
            self.writeline("parent_template = None")
        self.enter_frame(frame)
        self.pull_dependencies(node.body)
        self.blockvisit(node.body, frame)
        self.leave_frame(frame, with_python_scope=True)
        self.outdent()

        # make sure that the parent root is called.
        if have_extends:
            if not self.has_known_extends:
                self.indent()
                self.writeline("if parent_template is not None:")
            self.indent()
            if not self.environment.is_async:
                self.writeline("yield from parent_template.root_render_func(context)")
            else:
                self.writeline(
                    "async for event in parent_template.root_render_func(context):"
                )
                self.indent()
                self.writeline("yield event")
                self.outdent()
            self.outdent(1 + (not self.has_known_extends))

        # at this point we now have the blocks collected and can visit them too.
        for name, block in self.blocks.items():
            self.writeline(
                f"{self.func('block_' + name)}(context, missing=missing{envenv}):",
                block,
                1,
            )
            self.indent()
            self.write_commons()
            # It's important that we do not make this frame a child of the
            # toplevel template.  This would cause a variety of
            # interesting issues with identifier tracking.
            block_frame = Frame(eval_ctx)
            block_frame.block_frame = True
            undeclared = find_undeclared(block.body, ("self", "super"))
            if "self" in undeclared:
                ref = block_frame.symbols.declare_parameter("self")
                self.writeline(f"{ref} = TemplateReference(context)")
            if "super" in undeclared:
                ref = block_frame.symbols.declare_parameter("super")
                self.writeline(f"{ref} = context.super({name!r}, block_{name})")
            block_frame.symbols.analyze_node(block)
            block_frame.block = name
            self.writeline("_block_vars = {}")
            self.enter_frame(block_frame)
            self.pull_dependencies(block.body)
            self.blockvisit(block.body, block_frame)
            self.leave_frame(block_frame, with_python_scope=True)
            self.outdent()

        blocks_kv_str = ", ".join(f"{x!r}: block_{x}" for x in self.blocks)
        self.writeline(f"blocks = {{{blocks_kv_str}}}", extra=1)
        debug_kv_str = "&".join(f"{k}={v}" for k, v in self.debug_info)
        self.writeline(f"debug_info = {debug_kv_str!r}")

    def visit_Block(self, node: nodes.Block, frame: Frame) -> None:
        """Call a block and register it for the template."""
        level = 0
        if frame.toplevel:
            # if we know that we are a child template, there is no need to
            # check if we are one
            if self.has_known_extends:
                return
            if self.extends_so_far > 0:
                self.writeline("if parent_template is None:")
                self.indent()
                level += 1

        if node.scoped:
            context = self.derive_context(frame)
        else:
            context = self.get_context_ref()

        if node.required:
            self.writeline(f"if len(context.blocks[{node.name!r}]) <= 1:", node)
            self.indent()
            self.writeline(
                f'raise TemplateRuntimeError("Required block {node.name!r} not found")',
                node,
            )
            self.outdent()

        if not self.environment.is_async and frame.buffer is None:
            self.writeline(
                f"yield from context.blocks[{node.name!r}][0]({context})", node
            )
        else:
            self.writeline(
                f"{self.choose_async()}for event in"
                f" context.blocks[{node.name!r}][0]({context}):",
                node,
            )
            self.indent()
            self.simple_write("event", frame)
            self.outdent()

        self.outdent(level)

    def visit_Extends(self, node: nodes.Extends, frame: Frame) -> None:
        """Calls the extender."""
        if not frame.toplevel:
            self.fail("cannot use extend from a non top-level scope", node.lineno)

        # if the number of extends statements in general is zero so
        # far, we don't have to add a check if something extended
        # the template before this one.
        if self.extends_so_far > 0:

            # if we have a known extends we just add a template runtime
            # error into the generated code.  We could catch that at compile
            # time too, but i welcome it not to confuse users by throwing the
            # same error at different times just "because we can".
            if not self.has_known_extends:
                self.writeline("if parent_template is not None:")
                self.indent()
            self.writeline('raise TemplateRuntimeError("extended multiple times")')

            # if we have a known extends already we don't need that code here
            # as we know that the template execution will end here.
            if self.has_known_extends:
                raise CompilerExit()
            else:
                self.outdent()

        self.writeline("parent_template = environment.get_template(", node)
        self.visit(node.template, frame)
        self.write(f", {self.name!r})")
        self.writeline("for name, parent_block in parent_template.blocks.items():")
        self.indent()
        self.writeline("context.blocks.setdefault(name, []).append(parent_block)")
        self.outdent()

        # if this extends statement was in the root level we can take
        # advantage of that information and simplify the generated code
        # in the top level from this point onwards
        if frame.rootlevel:
            self.has_known_extends = True

        # and now we have one more
        self.extends_so_far += 1

    def visit_Include(self, node: nodes.Include, frame: Frame) -> None:
        """Handles includes."""
        if node.ignore_missing:
            self.writeline("try:")
            self.indent()

        func_name = "get_or_select_template"
        if isinstance(node.template, nodes.Const):
            if isinstance(node.template.value, str):
                func_name = "get_template"
            elif isinstance(node.template.value, (tuple, list)):
                func_name = "select_template"
        elif isinstance(node.template, (nodes.Tuple, nodes.List)):
            func_name = "select_template"

        self.writeline(f"template = environment.{func_name}(", node)
        self.visit(node.template, frame)
        self.write(f", {self.name!r})")
        if node.ignore_missing:
            self.outdent()
            self.writeline("except TemplateNotFound:")
            self.indent()
            self.writeline("pass")
            self.outdent()
            self.writeline("else:")
            self.indent()

        skip_event_yield = False
        if node.with_context:
            self.writeline(
                f"{self.choose_async()}for event in template.root_render_func("
                "template.new_context(context.get_all(), True,"
                f" {self.dump_local_context(frame)})):"
            )
        elif self.environment.is_async:
            self.writeline(
                "for event in (await template._get_default_module_async())"
                "._body_stream:"
            )
        else:
            self.writeline("yield from template._get_default_module()._body_stream")
            skip_event_yield = True

        if not skip_event_yield:
            self.indent()
            self.simple_write("event", frame)
            self.outdent()

        if node.ignore_missing:
            self.outdent()

    def _import_common(
        self, node: t.Union[nodes.Import, nodes.FromImport], frame: Frame
    ) -> None:
        self.write(f"{self.choose_async('await ')}environment.get_template(")
        self.visit(node.template, frame)
        self.write(f", {self.name!r}).")

        if node.with_context:
            f_name = f"make_module{self.choose_async('_async')}"
            self.write(
                f"{f_name}(context.get_all(), True, {self.dump_local_context(frame)})"
            )
        else:
            self.write(f"_get_default_module{self.choose_async('_async')}(context)")

    def visit_Import(self, node: nodes.Import, frame: Frame) -> None:
        """Visit regular imports."""
        self.writeline(f"{frame.symbols.ref(node.target)} = ", node)
        if frame.toplevel:
            self.write(f"context.vars[{node.target!r}] = ")

        self._import_common(node, frame)

        if frame.toplevel and not node.target.startswith("_"):
            self.writeline(f"context.exported_vars.discard({node.target!r})")

    def visit_FromImport(self, node: nodes.FromImport, frame: Frame) -> None:
        """Visit named imports."""
        self.newline(node)
        self.write("included_template = ")
        self._import_common(node, frame)
        var_names = []
        discarded_names = []
        for name in node.names:
            if isinstance(name, tuple):
                name, alias = name
            else:
                alias = name
            self.writeline(
                f"{frame.symbols.ref(alias)} ="
                f" getattr(included_template, {name!r}, missing)"
            )
            self.writeline(f"if {frame.symbols.ref(alias)} is missing:")
            self.indent()
            message = (
                "the template {included_template.__name__!r}"
                f" (imported on {self.position(node)})"
                f" does not export the requested name {name!r}"
            )
            self.writeline(
                f"{frame.symbols.ref(alias)} = undefined(f{message!r}, name={name!r})"
            )
            self.outdent()
            if frame.toplevel:
                var_names.append(alias)
                if not alias.startswith("_"):
                    discarded_names.append(alias)

        if var_names:
            if len(var_names) == 1:
                name = var_names[0]
                self.writeline(f"context.vars[{name!r}] = {frame.symbols.ref(name)}")
            else:
                names_kv = ", ".join(
                    f"{name!r}: {frame.symbols.ref(name)}" for name in var_names
                )
                self.writeline(f"context.vars.update({{{names_kv}}})")
        if discarded_names:
            if len(discarded_names) == 1:
                self.writeline(f"context.exported_vars.discard({discarded_names[0]!r})")
            else:
                names_str = ", ".join(map(repr, discarded_names))
                self.writeline(
                    f"context.exported_vars.difference_update(({names_str}))"
                )

    def visit_For(self, node: nodes.For, frame: Frame) -> None:
        loop_frame = frame.inner()
        loop_frame.loop_frame = True
        test_frame = frame.inner()
        else_frame = frame.inner()

        # try to figure out if we have an extended loop.  An extended loop
        # is necessary if the loop is in recursive mode if the special loop
        # variable is accessed in the body if the body is a scoped block.
        extended_loop = (
            node.recursive
            or "loop"
            in find_undeclared(node.iter_child_nodes(only=("body",)), ("loop",))
            or any(block.scoped for block in node.find_all(nodes.Block))
        )

        loop_ref = None
        if extended_loop:
            loop_ref = loop_frame.symbols.declare_parameter("loop")

        loop_frame.symbols.analyze_node(node, for_branch="body")
        if node.else_:
            else_frame.symbols.analyze_node(node, for_branch="else")

        if node.test:
            loop_filter_func = self.temporary_identifier()
            test_frame.symbols.analyze_node(node, for_branch="test")
            self.writeline(f"{self.func(loop_filter_func)}(fiter):", node.test)
            self.indent()
            self.enter_frame(test_frame)
            self.writeline(self.choose_async("async for ", "for "))
            self.visit(node.target, loop_frame)
            self.write(" in ")
            self.write(self.choose_async("auto_aiter(fiter)", "fiter"))
            self.write(":")
            self.indent()
            self.writeline("if ", node.test)
            self.visit(node.test, test_frame)
            self.write(":")
            self.indent()
            self.writeline("yield ")
            self.visit(node.target, loop_frame)
            self.outdent(3)
            self.leave_frame(test_frame, with_python_scope=True)

        # if we don't have an recursive loop we have to find the shadowed
        # variables at that point.  Because loops can be nested but the loop
        # variable is a special one we have to enforce aliasing for it.
        if node.recursive:
            self.writeline(
                f"{self.func('loop')}(reciter, loop_render_func, depth=0):", node
            )
            self.indent()
            self.buffer(loop_frame)

            # Use the same buffer for the else frame
            else_frame.buffer = loop_frame.buffer

        # make sure the loop variable is a special one and raise a template
        # assertion error if a loop tries to write to loop
        if extended_loop:
            self.writeline(f"{loop_ref} = missing")

        for name in node.find_all(nodes.Name):
            if name.ctx == "store" and name.name == "loop":
                self.fail(
                    "Can't assign to special loop variable in for-loop target",
                    name.lineno,
                )

        if node.else_:
            iteration_indicator = self.temporary_identifier()
            self.writeline(f"{iteration_indicator} = 1")

        self.writeline(self.choose_async("async for ", "for "), node)
        self.visit(node.target, loop_frame)
        if extended_loop:
            self.write(f", {loop_ref} in {self.choose_async('Async')}LoopContext(")
        else:
            self.write(" in ")

        if node.test:
            self.write(f"{loop_filter_func}(")
        if node.recursive:
            self.write("reciter")
        else:
            if self.environment.is_async and not extended_loop:
                self.write("auto_aiter(")
            self.visit(node.iter, frame)
            if self.environment.is_async and not extended_loop:
                self.write(")")
        if node.test:
            self.write(")")

        if node.recursive:
            self.write(", undefined, loop_render_func, depth):")
        else:
            self.write(", undefined):" if extended_loop else ":")

        self.indent()
        self.enter_frame(loop_frame)

        self.writeline("_loop_vars = {}")
        self.blockvisit(node.body, loop_frame)
        if node.else_:
            self.writeline(f"{iteration_indicator} = 0")
        self.outdent()
        self.leave_frame(
            loop_frame, with_python_scope=node.recursive and not node.else_
        )

        if node.else_:
            self.writeline(f"if {iteration_indicator}:")
            self.indent()
            self.enter_frame(else_frame)
            self.blockvisit(node.else_, else_frame)
            self.leave_frame(else_frame)
            self.outdent()

        # if the node was recursive we have to return the buffer contents
        # and start the iteration code
        if node.recursive:
            self.return_buffer_contents(loop_frame)
            self.outdent()
            self.start_write(frame, node)
            self.write(f"{self.choose_async('await ')}loop(")
            if self.environment.is_async:
                self.write("auto_aiter(")
            self.visit(node.iter, frame)
            if self.environment.is_async:
                self.write(")")
            self.write(", loop)")
            self.end_write(frame)

        # at the end of the iteration, clear any assignments made in the
        # loop from the top level
        if self._assign_stack:
            self._assign_stack[-1].difference_update(loop_frame.symbols.stores)

    def visit_If(self, node: nodes.If, frame: Frame) -> None:
        if_frame = frame.soft()
        self.writeline("if ", node)
        self.visit(node.test, if_frame)
        self.write(":")
        self.indent()
        self.blockvisit(node.body, if_frame)
        self.outdent()
        for elif_ in node.elif_:
            self.writeline("elif ", elif_)
            self.visit(elif_.test, if_frame)
            self.write(":")
            self.indent()
            self.blockvisit(elif_.body, if_frame)
            self.outdent()
        if node.else_:
            self.writeline("else:")
            self.indent()
            self.blockvisit(node.else_, if_frame)
            self.outdent()

    def visit_Macro(self, node: nodes.Macro, frame: Frame) -> None:
        macro_frame, macro_ref = self.macro_body(node, frame)
        self.newline()
        if frame.toplevel:
            if not node.name.startswith("_"):
                self.write(f"context.exported_vars.add({node.name!r})")
            self.writeline(f"context.vars[{node.name!r}] = ")
        self.write(f"{frame.symbols.ref(node.name)} = ")
        self.macro_def(macro_ref, macro_frame)

    def visit_CallBlock(self, node: nodes.CallBlock, frame: Frame) -> None:
        call_frame, macro_ref = self.macro_body(node, frame)
        self.writeline("caller = ")
        self.macro_def(macro_ref, call_frame)
        self.start_write(frame, node)
        self.visit_Call(node.call, frame, forward_caller=True)
        self.end_write(frame)

    def visit_FilterBlock(self, node: nodes.FilterBlock, frame: Frame) -> None:
        filter_frame = frame.inner()
        filter_frame.symbols.analyze_node(node)
        self.enter_frame(filter_frame)
        self.buffer(filter_frame)
        self.blockvisit(node.body, filter_frame)
        self.start_write(frame, node)
        self.visit_Filter(node.filter, filter_frame)
        self.end_write(frame)
        self.leave_frame(filter_frame)

    def visit_With(self, node: nodes.With, frame: Frame) -> None:
        with_frame = frame.inner()
        with_frame.symbols.analyze_node(node)
        self.enter_frame(with_frame)
        for target, expr in zip(node.targets, node.values):
            self.newline()
            self.visit(target, with_frame)
            self.write(" = ")
            self.visit(expr, frame)
        self.blockvisit(node.body, with_frame)
        self.leave_frame(with_frame)

    def visit_ExprStmt(self, node: nodes.ExprStmt, frame: Frame) -> None:
        self.newline(node)
        self.visit(node.node, frame)

    class _FinalizeInfo(t.NamedTuple):
        const: t.Optional[t.Callable[..., str]]
        src: t.Optional[str]

    @staticmethod
    def _default_finalize(value: t.Any) -> t.Any:
        """The default finalize function if the environment isn't
        configured with one. Or, if the environment has one, this is
        called on that function's output for constants.
        """
        return str(value)

    _finalize: t.Optional[_FinalizeInfo] = None

    def _make_finalize(self) -> _FinalizeInfo:
        """Build the finalize function to be used on constants and at
        runtime. Cached so it's only created once for all output nodes.

        Returns a ``namedtuple`` with the following attributes:

        ``const``
            A function to finalize constant data at compile time.

        ``src``
            Source code to output around nodes to be evaluated at
            runtime.
        """
        if self._finalize is not None:
            return self._finalize

        finalize: t.Optional[t.Callable[..., t.Any]]
        finalize = default = self._default_finalize
        src = None

        if self.environment.finalize:
            src = "environment.finalize("
            env_finalize = self.environment.finalize
            pass_arg = {
                _PassArg.context: "context",
                _PassArg.eval_context: "context.eval_ctx",
                _PassArg.environment: "environment",
            }.get(
                _PassArg.from_obj(env_finalize)  # type: ignore
            )
            finalize = None

            if pass_arg is None:

                def finalize(value: t.Any) -> t.Any:
                    return default(env_finalize(value))

            else:
                src = f"{src}{pass_arg}, "

                if pass_arg == "environment":

                    def finalize(value: t.Any) -> t.Any:
                        return default(env_finalize(self.environment, value))

        self._finalize = self._FinalizeInfo(finalize, src)
        return self._finalize

    def _output_const_repr(self, group: t.Iterable[t.Any]) -> str:
        """Given a group of constant values converted from ``Output``
        child nodes, produce a string to write to the template module
        source.
        """
        return repr(concat(group))

    def _output_child_to_const(
        self, node: nodes.Expr, frame: Frame, finalize: _FinalizeInfo
    ) -> str:
        """Try to optimize a child of an ``Output`` node by trying to
        convert it to constant, finalized data at compile time.

        If :exc:`Impossible` is raised, the node is not constant and
        will be evaluated at runtime. Any other exception will also be
        evaluated at runtime for easier debugging.
        """
        const = node.as_const(frame.eval_ctx)

        if frame.eval_ctx.autoescape:
            const = escape(const)

        # Template data doesn't go through finalize.
        if isinstance(node, nodes.TemplateData):
            return str(const)

        return finalize.const(const)  # type: ignore

    def _output_child_pre(
        self, node: nodes.Expr, frame: Frame, finalize: _FinalizeInfo
    ) -> None:
        """Output extra source code before visiting a child of an
        ``Output`` node.
        """
        if frame.eval_ctx.volatile:
            self.write("(escape if context.eval_ctx.autoescape else str)(")
        elif frame.eval_ctx.autoescape:
            self.write("escape(")
        else:
            self.write("str(")

        if finalize.src is not None:
            self.write(finalize.src)

    def _output_child_post(
        self, node: nodes.Expr, frame: Frame, finalize: _FinalizeInfo
    ) -> None:
        """Output extra source code after visiting a child of an
        ``Output`` node.
        """
        self.write(")")

        if finalize.src is not None:
            self.write(")")

    def visit_Output(self, node: nodes.Output, frame: Frame) -> None:
        # If an extends is active, don't render outside a block.
        if frame.require_output_check:
            # A top-level extends is known to exist at compile time.
            if self.has_known_extends:
                return

            self.writeline("if parent_template is None:")
            self.indent()

        finalize = self._make_finalize()
        body: t.List[t.Union[t.List[t.Any], nodes.Expr]] = []

        # Evaluate constants at compile time if possible. Each item in
        # body will be either a list of static data or a node to be
        # evaluated at runtime.
        for child in node.nodes:
            try:
                if not (
                    # If the finalize function requires runtime context,
                    # constants can't be evaluated at compile time.
                    finalize.const
                    # Unless it's basic template data that won't be
                    # finalized anyway.
                    or isinstance(child, nodes.TemplateData)
                ):
                    raise nodes.Impossible()

                const = self._output_child_to_const(child, frame, finalize)
            except (nodes.Impossible, Exception):
                # The node was not constant and needs to be evaluated at
                # runtime. Or another error was raised, which is easier
                # to debug at runtime.
                body.append(child)
                continue

            if body and isinstance(body[-1], list):
                body[-1].append(const)
            else:
                body.append([const])

        if frame.buffer is not None:
            if len(body) == 1:
                self.writeline(f"{frame.buffer}.append(")
            else:
                self.writeline(f"{frame.buffer}.extend((")

            self.indent()

        for item in body:
            if isinstance(item, list):
                # A group of constant data to join and output.
                val = self._output_const_repr(item)

                if frame.buffer is None:
                    self.writeline("yield " + val)
                else:
                    self.writeline(val + ",")
            else:
                if frame.buffer is None:
                    self.writeline("yield ", item)
                else:
                    self.newline(item)

                # A node to be evaluated at runtime.
                self._output_child_pre(item, frame, finalize)
                self.visit(item, frame)
                self._output_child_post(item, frame, finalize)

                if frame.buffer is not None:
                    self.write(",")

        if frame.buffer is not None:
            self.outdent()
            self.writeline(")" if len(body) == 1 else "))")

        if frame.require_output_check:
            self.outdent()

    def visit_Assign(self, node: nodes.Assign, frame: Frame) -> None:
        self.push_assign_tracking()
        self.newline(node)
        self.visit(node.target, frame)
        self.write(" = ")
        self.visit(node.node, frame)
        self.pop_assign_tracking(frame)

    def visit_AssignBlock(self, node: nodes.AssignBlock, frame: Frame) -> None:
        self.push_assign_tracking()
        block_frame = frame.inner()
        # This is a special case.  Since a set block always captures we
        # will disable output checks.  This way one can use set blocks
        # toplevel even in extended templates.
        block_frame.require_output_check = False
        block_frame.symbols.analyze_node(node)
        self.enter_frame(block_frame)
        self.buffer(block_frame)
        self.blockvisit(node.body, block_frame)
        self.newline(node)
        self.visit(node.target, frame)
        self.write(" = (Markup if context.eval_ctx.autoescape else identity)(")
        if node.filter is not None:
            self.visit_Filter(node.filter, block_frame)
        else:
            self.write(f"concat({block_frame.buffer})")
        self.write(")")
        self.pop_assign_tracking(frame)
        self.leave_frame(block_frame)

    # -- Expression Visitors

    def visit_Name(self, node: nodes.Name, frame: Frame) -> None:
        if node.ctx == "store" and (
            frame.toplevel or frame.loop_frame or frame.block_frame
        ):
            if self._assign_stack:
                self._assign_stack[-1].add(node.name)
        ref = frame.symbols.ref(node.name)

        # If we are looking up a variable we might have to deal with the
        # case where it's undefined.  We can skip that case if the load
        # instruction indicates a parameter which are always defined.
        if node.ctx == "load":
            load = frame.symbols.find_load(ref)
            if not (
                load is not None
                and load[0] == VAR_LOAD_PARAMETER
                and not self.parameter_is_undeclared(ref)
            ):
                self.write(
                    f"(undefined(name={node.name!r}) if {ref} is missing else {ref})"
                )
                return

        self.write(ref)

    def visit_NSRef(self, node: nodes.NSRef, frame: Frame) -> None:
        # NSRefs can only be used to store values; since they use the normal
        # `foo.bar` notation they will be parsed as a normal attribute access
        # when used anywhere but in a `set` context
        ref = frame.symbols.ref(node.name)
        self.writeline(f"if not isinstance({ref}, Namespace):")
        self.indent()
        self.writeline(
            "raise TemplateRuntimeError"
            '("cannot assign attribute on non-namespace object")'
        )
        self.outdent()
        self.writeline(f"{ref}[{node.attr!r}]")

    def visit_Const(self, node: nodes.Const, frame: Frame) -> None:
        val = node.as_const(frame.eval_ctx)
        if isinstance(val, float):
            self.write(str(val))
        else:
            self.write(repr(val))

    def visit_TemplateData(self, node: nodes.TemplateData, frame: Frame) -> None:
        try:
            self.write(repr(node.as_const(frame.eval_ctx)))
        except nodes.Impossible:
            self.write(
                f"(Markup if context.eval_ctx.autoescape else identity)({node.data!r})"
            )

    def visit_Tuple(self, node: nodes.Tuple, frame: Frame) -> None:
        self.write("(")
        idx = -1
        for idx, item in enumerate(node.items):
            if idx:
                self.write(", ")
            self.visit(item, frame)
        self.write(",)" if idx == 0 else ")")

    def visit_List(self, node: nodes.List, frame: Frame) -> None:
        self.write("[")
        for idx, item in enumerate(node.items):
            if idx:
                self.write(", ")
            self.visit(item, frame)
        self.write("]")

    def visit_Dict(self, node: nodes.Dict, frame: Frame) -> None:
        self.write("{")
        for idx, item in enumerate(node.items):
            if idx:
                self.write(", ")
            self.visit(item.key, frame)
            self.write(": ")
            self.visit(item.value, frame)
        self.write("}")

    visit_Add = _make_binop("+")
    visit_Sub = _make_binop("-")
    visit_Mul = _make_binop("*")
    visit_Div = _make_binop("/")
    visit_FloorDiv = _make_binop("//")
    visit_Pow = _make_binop("**")
    visit_Mod = _make_binop("%")
    visit_And = _make_binop("and")
    visit_Or = _make_binop("or")
    visit_Pos = _make_unop("+")
    visit_Neg = _make_unop("-")
    visit_Not = _make_unop("not ")

    @optimizeconst
    def visit_Concat(self, node: nodes.Concat, frame: Frame) -> None:
        if frame.eval_ctx.volatile:
            func_name = "(markup_join if context.eval_ctx.volatile else str_join)"
        elif frame.eval_ctx.autoescape:
            func_name = "markup_join"
        else:
            func_name = "str_join"
        self.write(f"{func_name}((")
        for arg in node.nodes:
            self.visit(arg, frame)
            self.write(", ")
        self.write("))")

    @optimizeconst
    def visit_Compare(self, node: nodes.Compare, frame: Frame) -> None:
        self.write("(")
        self.visit(node.expr, frame)
        for op in node.ops:
            self.visit(op, frame)
        self.write(")")

    def visit_Operand(self, node: nodes.Operand, frame: Frame) -> None:
        self.write(f" {operators[node.op]} ")
        self.visit(node.expr, frame)

    @optimizeconst
    def visit_Getattr(self, node: nodes.Getattr, frame: Frame) -> None:
        if self.environment.is_async:
            self.write("(await auto_await(")

        self.write("environment.getattr(")
        self.visit(node.node, frame)
        self.write(f", {node.attr!r})")

        if self.environment.is_async:
            self.write("))")

    @optimizeconst
    def visit_Getitem(self, node: nodes.Getitem, frame: Frame) -> None:
        # slices bypass the environment getitem method.
        if isinstance(node.arg, nodes.Slice):
            self.visit(node.node, frame)
            self.write("[")
            self.visit(node.arg, frame)
            self.write("]")
        else:
            if self.environment.is_async:
                self.write("(await auto_await(")

            self.write("environment.getitem(")
            self.visit(node.node, frame)
            self.write(", ")
            self.visit(node.arg, frame)
            self.write(")")

            if self.environment.is_async:
                self.write("))")

    def visit_Slice(self, node: nodes.Slice, frame: Frame) -> None:
        if node.start is not None:
            self.visit(node.start, frame)
        self.write(":")
        if node.stop is not None:
            self.visit(node.stop, frame)
        if node.step is not None:
            self.write(":")
            self.visit(node.step, frame)

    @contextmanager
    def _filter_test_common(
        self, node: t.Union[nodes.Filter, nodes.Test], frame: Frame, is_filter: bool
    ) -> t.Iterator[None]:
        if self.environment.is_async:
            self.write("(await auto_await(")

        if is_filter:
            self.write(f"{self.filters[node.name]}(")
            func = self.environment.filters.get(node.name)
        else:
            self.write(f"{self.tests[node.name]}(")
            func = self.environment.tests.get(node.name)

        # When inside an If or CondExpr frame, allow the filter to be
        # undefined at compile time and only raise an error if it's
        # actually called at runtime. See pull_dependencies.
        if func is None and not frame.soft_frame:
            type_name = "filter" if is_filter else "test"
            self.fail(f"No {type_name} named {node.name!r}.", node.lineno)

        pass_arg = {
            _PassArg.context: "context",
            _PassArg.eval_context: "context.eval_ctx",
            _PassArg.environment: "environment",
        }.get(
            _PassArg.from_obj(func)  # type: ignore
        )

        if pass_arg is not None:
            self.write(f"{pass_arg}, ")

        # Back to the visitor function to handle visiting the target of
        # the filter or test.
        yield

        self.signature(node, frame)
        self.write(")")

        if self.environment.is_async:
            self.write("))")

    @optimizeconst
    def visit_Filter(self, node: nodes.Filter, frame: Frame) -> None:
        with self._filter_test_common(node, frame, True):
            # if the filter node is None we are inside a filter block
            # and want to write to the current buffer
            if node.node is not None:
                self.visit(node.node, frame)
            elif frame.eval_ctx.volatile:
                self.write(
                    f"(Markup(concat({frame.buffer}))"
                    f" if context.eval_ctx.autoescape else concat({frame.buffer}))"
                )
            elif frame.eval_ctx.autoescape:
                self.write(f"Markup(concat({frame.buffer}))")
            else:
                self.write(f"concat({frame.buffer})")

    @optimizeconst
    def visit_Test(self, node: nodes.Test, frame: Frame) -> None:
        with self._filter_test_common(node, frame, False):
            self.visit(node.node, frame)

    @optimizeconst
    def visit_CondExpr(self, node: nodes.CondExpr, frame: Frame) -> None:
        frame = frame.soft()

        def write_expr2() -> None:
            if node.expr2 is not None:
                self.visit(node.expr2, frame)
                return

            self.write(
                f'cond_expr_undefined("the inline if-expression on'
                f" {self.position(node)} evaluated to false and no else"
                f' section was defined.")'
            )

        self.write("(")
        self.visit(node.expr1, frame)
        self.write(" if ")
        self.visit(node.test, frame)
        self.write(" else ")
        write_expr2()
        self.write(")")

    @optimizeconst
    def visit_Call(
        self, node: nodes.Call, frame: Frame, forward_caller: bool = False
    ) -> None:
        if self.environment.is_async:
            self.write("(await auto_await(")
        if self.environment.sandboxed:
            self.write("environment.call(context, ")
        else:
            self.write("context.call(")
        self.visit(node.node, frame)
        extra_kwargs = {"caller": "caller"} if forward_caller else None
        loop_kwargs = {"_loop_vars": "_loop_vars"} if frame.loop_frame else {}
        block_kwargs = {"_block_vars": "_block_vars"} if frame.block_frame else {}
        if extra_kwargs:
            extra_kwargs.update(loop_kwargs, **block_kwargs)
        elif loop_kwargs or block_kwargs:
            extra_kwargs = dict(loop_kwargs, **block_kwargs)
        self.signature(node, frame, extra_kwargs)
        self.write(")")
        if self.environment.is_async:
            self.write("))")

    def visit_Keyword(self, node: nodes.Keyword, frame: Frame) -> None:
        self.write(node.key + "=")
        self.visit(node.value, frame)

    # -- Unused nodes for extensions

    def visit_MarkSafe(self, node: nodes.MarkSafe, frame: Frame) -> None:
        self.write("Markup(")
        self.visit(node.expr, frame)
        self.write(")")

    def visit_MarkSafeIfAutoescape(
        self, node: nodes.MarkSafeIfAutoescape, frame: Frame
    ) -> None:
        self.write("(Markup if context.eval_ctx.autoescape else identity)(")
        self.visit(node.expr, frame)
        self.write(")")

    def visit_EnvironmentAttribute(
        self, node: nodes.EnvironmentAttribute, frame: Frame
    ) -> None:
        self.write("environment." + node.name)

    def visit_ExtensionAttribute(
        self, node: nodes.ExtensionAttribute, frame: Frame
    ) -> None:
        self.write(f"environment.extensions[{node.identifier!r}].{node.name}")

    def visit_ImportedName(self, node: nodes.ImportedName, frame: Frame) -> None:
        self.write(self.import_aliases[node.importname])

    def visit_InternalName(self, node: nodes.InternalName, frame: Frame) -> None:
        self.write(node.name)

    def visit_ContextReference(
        self, node: nodes.ContextReference, frame: Frame
    ) -> None:
        self.write("context")

    def visit_DerivedContextReference(
        self, node: nodes.DerivedContextReference, frame: Frame
    ) -> None:
        self.write(self.derive_context(frame))

    def visit_Continue(self, node: nodes.Continue, frame: Frame) -> None:
        self.writeline("continue", node)

    def visit_Break(self, node: nodes.Break, frame: Frame) -> None:
        self.writeline("break", node)

    def visit_Scope(self, node: nodes.Scope, frame: Frame) -> None:
        scope_frame = frame.inner()
        scope_frame.symbols.analyze_node(node)
        self.enter_frame(scope_frame)
        self.blockvisit(node.body, scope_frame)
        self.leave_frame(scope_frame)

    def visit_OverlayScope(self, node: nodes.OverlayScope, frame: Frame) -> None:
        ctx = self.temporary_identifier()
        self.writeline(f"{ctx} = {self.derive_context(frame)}")
        self.writeline(f"{ctx}.vars = ")
        self.visit(node.context, frame)
        self.push_context_reference(ctx)

        scope_frame = frame.inner(isolated=True)
        scope_frame.symbols.analyze_node(node)
        self.enter_frame(scope_frame)
        self.blockvisit(node.body, scope_frame)
        self.leave_frame(scope_frame)
        self.pop_context_reference()

    def visit_EvalContextModifier(
        self, node: nodes.EvalContextModifier, frame: Frame
    ) -> None:
        for keyword in node.options:
            self.writeline(f"context.eval_ctx.{keyword.key} = ")
            self.visit(keyword.value, frame)
            try:
                val = keyword.value.as_const(frame.eval_ctx)
            except nodes.Impossible:
                frame.eval_ctx.volatile = True
            else:
                setattr(frame.eval_ctx, keyword.key, val)

    def visit_ScopedEvalContextModifier(
        self, node: nodes.ScopedEvalContextModifier, frame: Frame
    ) -> None:
        old_ctx_name = self.temporary_identifier()
        saved_ctx = frame.eval_ctx.save()
        self.writeline(f"{old_ctx_name} = context.eval_ctx.save()")
        self.visit_EvalContextModifier(node, frame)
        for child in node.body:
            self.visit(child, frame)
        frame.eval_ctx.revert(saved_ctx)
        self.writeline(f"context.eval_ctx.revert({old_ctx_name})")
