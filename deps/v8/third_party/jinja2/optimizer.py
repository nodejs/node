# -*- coding: utf-8 -*-
"""The optimizer tries to constant fold expressions and modify the AST
in place so that it should be faster to evaluate.

Because the AST does not contain all the scoping information and the
compiler has to find that out, we cannot do all the optimizations we
want. For example, loop unrolling doesn't work because unrolled loops
would have a different scope. The solution would be a second syntax tree
that stored the scoping rules.
"""
from . import nodes
from .visitor import NodeTransformer


def optimize(node, environment):
    """The context hint can be used to perform an static optimization
    based on the context given."""
    optimizer = Optimizer(environment)
    return optimizer.visit(node)


class Optimizer(NodeTransformer):
    def __init__(self, environment):
        self.environment = environment

    def generic_visit(self, node, *args, **kwargs):
        node = super(Optimizer, self).generic_visit(node, *args, **kwargs)

        # Do constant folding. Some other nodes besides Expr have
        # as_const, but folding them causes errors later on.
        if isinstance(node, nodes.Expr):
            try:
                return nodes.Const.from_untrusted(
                    node.as_const(args[0] if args else None),
                    lineno=node.lineno,
                    environment=self.environment,
                )
            except nodes.Impossible:
                pass

        return node
