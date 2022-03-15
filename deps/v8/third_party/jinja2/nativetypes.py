from ast import literal_eval
from itertools import chain
from itertools import islice

from . import nodes
from ._compat import text_type
from .compiler import CodeGenerator
from .compiler import has_safe_repr
from .environment import Environment
from .environment import Template


def native_concat(nodes):
    """Return a native Python type from the list of compiled nodes. If
    the result is a single node, its value is returned. Otherwise, the
    nodes are concatenated as strings. If the result can be parsed with
    :func:`ast.literal_eval`, the parsed value is returned. Otherwise,
    the string is returned.

    :param nodes: Iterable of nodes to concatenate.
    """
    head = list(islice(nodes, 2))

    if not head:
        return None

    if len(head) == 1:
        raw = head[0]
    else:
        raw = u"".join([text_type(v) for v in chain(head, nodes)])

    try:
        return literal_eval(raw)
    except (ValueError, SyntaxError, MemoryError):
        return raw


class NativeCodeGenerator(CodeGenerator):
    """A code generator which renders Python types by not adding
    ``to_string()`` around output nodes.
    """

    @staticmethod
    def _default_finalize(value):
        return value

    def _output_const_repr(self, group):
        return repr(u"".join([text_type(v) for v in group]))

    def _output_child_to_const(self, node, frame, finalize):
        const = node.as_const(frame.eval_ctx)

        if not has_safe_repr(const):
            raise nodes.Impossible()

        if isinstance(node, nodes.TemplateData):
            return const

        return finalize.const(const)

    def _output_child_pre(self, node, frame, finalize):
        if finalize.src is not None:
            self.write(finalize.src)

    def _output_child_post(self, node, frame, finalize):
        if finalize.src is not None:
            self.write(")")


class NativeEnvironment(Environment):
    """An environment that renders templates to native Python types."""

    code_generator_class = NativeCodeGenerator


class NativeTemplate(Template):
    environment_class = NativeEnvironment

    def render(self, *args, **kwargs):
        """Render the template to produce a native Python type. If the
        result is a single node, its value is returned. Otherwise, the
        nodes are concatenated as strings. If the result can be parsed
        with :func:`ast.literal_eval`, the parsed value is returned.
        Otherwise, the string is returned.
        """
        vars = dict(*args, **kwargs)

        try:
            return native_concat(self.root_render_func(self.new_context(vars)))
        except Exception:
            return self.environment.handle_exception()


NativeEnvironment.template_class = NativeTemplate
