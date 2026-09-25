"""API for traversing the AST nodes. Implemented by the compiler and
meta introspection.
"""
import typing as t

from .nodes import Node

if t.TYPE_CHECKING:
    import typing_extensions as te

    class VisitCallable(te.Protocol):
        def __call__(self, node: Node, *args: t.Any, **kwargs: t.Any) -> t.Any:
            ...


class NodeVisitor:
    """Walks the abstract syntax tree and call visitor functions for every
    node found.  The visitor functions may return values which will be
    forwarded by the `visit` method.

    Per default the visitor functions for the nodes are ``'visit_'`` +
    class name of the node.  So a `TryFinally` node visit function would
    be `visit_TryFinally`.  This behavior can be changed by overriding
    the `get_visitor` function.  If no visitor function exists for a node
    (return value `None`) the `generic_visit` visitor is used instead.
    """

    def get_visitor(self, node: Node) -> "t.Optional[VisitCallable]":
        """Return the visitor function for this node or `None` if no visitor
        exists for this node.  In that case the generic visit function is
        used instead.
        """
        return getattr(self, f"visit_{type(node).__name__}", None)

    def visit(self, node: Node, *args: t.Any, **kwargs: t.Any) -> t.Any:
        """Visit a node."""
        f = self.get_visitor(node)

        if f is not None:
            return f(node, *args, **kwargs)

        return self.generic_visit(node, *args, **kwargs)

    def generic_visit(self, node: Node, *args: t.Any, **kwargs: t.Any) -> t.Any:
        """Called if no explicit visitor function exists for a node."""
        for child_node in node.iter_child_nodes():
            self.visit(child_node, *args, **kwargs)


class NodeTransformer(NodeVisitor):
    """Walks the abstract syntax tree and allows modifications of nodes.

    The `NodeTransformer` will walk the AST and use the return value of the
    visitor functions to replace or remove the old node.  If the return
    value of the visitor function is `None` the node will be removed
    from the previous location otherwise it's replaced with the return
    value.  The return value may be the original node in which case no
    replacement takes place.
    """

    def generic_visit(self, node: Node, *args: t.Any, **kwargs: t.Any) -> Node:
        for field, old_value in node.iter_fields():
            if isinstance(old_value, list):
                new_values = []
                for value in old_value:
                    if isinstance(value, Node):
                        value = self.visit(value, *args, **kwargs)
                        if value is None:
                            continue
                        elif not isinstance(value, Node):
                            new_values.extend(value)
                            continue
                    new_values.append(value)
                old_value[:] = new_values
            elif isinstance(old_value, Node):
                new_node = self.visit(old_value, *args, **kwargs)
                if new_node is None:
                    delattr(node, field)
                else:
                    setattr(node, field, new_node)
        return node

    def visit_list(self, node: Node, *args: t.Any, **kwargs: t.Any) -> t.List[Node]:
        """As transformers may return lists in some places this method
        can be used to enforce a list as return value.
        """
        rv = self.visit(node, *args, **kwargs)

        if not isinstance(rv, list):
            return [rv]

        return rv
