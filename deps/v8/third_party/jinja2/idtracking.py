import typing as t

from . import nodes
from .visitor import NodeVisitor

VAR_LOAD_PARAMETER = "param"
VAR_LOAD_RESOLVE = "resolve"
VAR_LOAD_ALIAS = "alias"
VAR_LOAD_UNDEFINED = "undefined"


def find_symbols(
    nodes: t.Iterable[nodes.Node], parent_symbols: t.Optional["Symbols"] = None
) -> "Symbols":
    sym = Symbols(parent=parent_symbols)
    visitor = FrameSymbolVisitor(sym)
    for node in nodes:
        visitor.visit(node)
    return sym


def symbols_for_node(
    node: nodes.Node, parent_symbols: t.Optional["Symbols"] = None
) -> "Symbols":
    sym = Symbols(parent=parent_symbols)
    sym.analyze_node(node)
    return sym


class Symbols:
    def __init__(
        self, parent: t.Optional["Symbols"] = None, level: t.Optional[int] = None
    ) -> None:
        if level is None:
            if parent is None:
                level = 0
            else:
                level = parent.level + 1

        self.level: int = level
        self.parent = parent
        self.refs: t.Dict[str, str] = {}
        self.loads: t.Dict[str, t.Any] = {}
        self.stores: t.Set[str] = set()

    def analyze_node(self, node: nodes.Node, **kwargs: t.Any) -> None:
        visitor = RootVisitor(self)
        visitor.visit(node, **kwargs)

    def _define_ref(
        self, name: str, load: t.Optional[t.Tuple[str, t.Optional[str]]] = None
    ) -> str:
        ident = f"l_{self.level}_{name}"
        self.refs[name] = ident
        if load is not None:
            self.loads[ident] = load
        return ident

    def find_load(self, target: str) -> t.Optional[t.Any]:
        if target in self.loads:
            return self.loads[target]

        if self.parent is not None:
            return self.parent.find_load(target)

        return None

    def find_ref(self, name: str) -> t.Optional[str]:
        if name in self.refs:
            return self.refs[name]

        if self.parent is not None:
            return self.parent.find_ref(name)

        return None

    def ref(self, name: str) -> str:
        rv = self.find_ref(name)
        if rv is None:
            raise AssertionError(
                "Tried to resolve a name to a reference that was"
                f" unknown to the frame ({name!r})"
            )
        return rv

    def copy(self) -> "Symbols":
        rv = object.__new__(self.__class__)
        rv.__dict__.update(self.__dict__)
        rv.refs = self.refs.copy()
        rv.loads = self.loads.copy()
        rv.stores = self.stores.copy()
        return rv

    def store(self, name: str) -> None:
        self.stores.add(name)

        # If we have not see the name referenced yet, we need to figure
        # out what to set it to.
        if name not in self.refs:
            # If there is a parent scope we check if the name has a
            # reference there.  If it does it means we might have to alias
            # to a variable there.
            if self.parent is not None:
                outer_ref = self.parent.find_ref(name)
                if outer_ref is not None:
                    self._define_ref(name, load=(VAR_LOAD_ALIAS, outer_ref))
                    return

            # Otherwise we can just set it to undefined.
            self._define_ref(name, load=(VAR_LOAD_UNDEFINED, None))

    def declare_parameter(self, name: str) -> str:
        self.stores.add(name)
        return self._define_ref(name, load=(VAR_LOAD_PARAMETER, None))

    def load(self, name: str) -> None:
        if self.find_ref(name) is None:
            self._define_ref(name, load=(VAR_LOAD_RESOLVE, name))

    def branch_update(self, branch_symbols: t.Sequence["Symbols"]) -> None:
        stores: t.Dict[str, int] = {}
        for branch in branch_symbols:
            for target in branch.stores:
                if target in self.stores:
                    continue
                stores[target] = stores.get(target, 0) + 1

        for sym in branch_symbols:
            self.refs.update(sym.refs)
            self.loads.update(sym.loads)
            self.stores.update(sym.stores)

        for name, branch_count in stores.items():
            if branch_count == len(branch_symbols):
                continue

            target = self.find_ref(name)  # type: ignore
            assert target is not None, "should not happen"

            if self.parent is not None:
                outer_target = self.parent.find_ref(name)
                if outer_target is not None:
                    self.loads[target] = (VAR_LOAD_ALIAS, outer_target)
                    continue
            self.loads[target] = (VAR_LOAD_RESOLVE, name)

    def dump_stores(self) -> t.Dict[str, str]:
        rv: t.Dict[str, str] = {}
        node: t.Optional["Symbols"] = self

        while node is not None:
            for name in sorted(node.stores):
                if name not in rv:
                    rv[name] = self.find_ref(name)  # type: ignore

            node = node.parent

        return rv

    def dump_param_targets(self) -> t.Set[str]:
        rv = set()
        node: t.Optional["Symbols"] = self

        while node is not None:
            for target, (instr, _) in self.loads.items():
                if instr == VAR_LOAD_PARAMETER:
                    rv.add(target)

            node = node.parent

        return rv


class RootVisitor(NodeVisitor):
    def __init__(self, symbols: "Symbols") -> None:
        self.sym_visitor = FrameSymbolVisitor(symbols)

    def _simple_visit(self, node: nodes.Node, **kwargs: t.Any) -> None:
        for child in node.iter_child_nodes():
            self.sym_visitor.visit(child)

    visit_Template = _simple_visit
    visit_Block = _simple_visit
    visit_Macro = _simple_visit
    visit_FilterBlock = _simple_visit
    visit_Scope = _simple_visit
    visit_If = _simple_visit
    visit_ScopedEvalContextModifier = _simple_visit

    def visit_AssignBlock(self, node: nodes.AssignBlock, **kwargs: t.Any) -> None:
        for child in node.body:
            self.sym_visitor.visit(child)

    def visit_CallBlock(self, node: nodes.CallBlock, **kwargs: t.Any) -> None:
        for child in node.iter_child_nodes(exclude=("call",)):
            self.sym_visitor.visit(child)

    def visit_OverlayScope(self, node: nodes.OverlayScope, **kwargs: t.Any) -> None:
        for child in node.body:
            self.sym_visitor.visit(child)

    def visit_For(
        self, node: nodes.For, for_branch: str = "body", **kwargs: t.Any
    ) -> None:
        if for_branch == "body":
            self.sym_visitor.visit(node.target, store_as_param=True)
            branch = node.body
        elif for_branch == "else":
            branch = node.else_
        elif for_branch == "test":
            self.sym_visitor.visit(node.target, store_as_param=True)
            if node.test is not None:
                self.sym_visitor.visit(node.test)
            return
        else:
            raise RuntimeError("Unknown for branch")

        if branch:
            for item in branch:
                self.sym_visitor.visit(item)

    def visit_With(self, node: nodes.With, **kwargs: t.Any) -> None:
        for target in node.targets:
            self.sym_visitor.visit(target)
        for child in node.body:
            self.sym_visitor.visit(child)

    def generic_visit(self, node: nodes.Node, *args: t.Any, **kwargs: t.Any) -> None:
        raise NotImplementedError(f"Cannot find symbols for {type(node).__name__!r}")


class FrameSymbolVisitor(NodeVisitor):
    """A visitor for `Frame.inspect`."""

    def __init__(self, symbols: "Symbols") -> None:
        self.symbols = symbols

    def visit_Name(
        self, node: nodes.Name, store_as_param: bool = False, **kwargs: t.Any
    ) -> None:
        """All assignments to names go through this function."""
        if store_as_param or node.ctx == "param":
            self.symbols.declare_parameter(node.name)
        elif node.ctx == "store":
            self.symbols.store(node.name)
        elif node.ctx == "load":
            self.symbols.load(node.name)

    def visit_NSRef(self, node: nodes.NSRef, **kwargs: t.Any) -> None:
        self.symbols.load(node.name)

    def visit_If(self, node: nodes.If, **kwargs: t.Any) -> None:
        self.visit(node.test, **kwargs)
        original_symbols = self.symbols

        def inner_visit(nodes: t.Iterable[nodes.Node]) -> "Symbols":
            self.symbols = rv = original_symbols.copy()

            for subnode in nodes:
                self.visit(subnode, **kwargs)

            self.symbols = original_symbols
            return rv

        body_symbols = inner_visit(node.body)
        elif_symbols = inner_visit(node.elif_)
        else_symbols = inner_visit(node.else_ or ())
        self.symbols.branch_update([body_symbols, elif_symbols, else_symbols])

    def visit_Macro(self, node: nodes.Macro, **kwargs: t.Any) -> None:
        self.symbols.store(node.name)

    def visit_Import(self, node: nodes.Import, **kwargs: t.Any) -> None:
        self.generic_visit(node, **kwargs)
        self.symbols.store(node.target)

    def visit_FromImport(self, node: nodes.FromImport, **kwargs: t.Any) -> None:
        self.generic_visit(node, **kwargs)

        for name in node.names:
            if isinstance(name, tuple):
                self.symbols.store(name[1])
            else:
                self.symbols.store(name)

    def visit_Assign(self, node: nodes.Assign, **kwargs: t.Any) -> None:
        """Visit assignments in the correct order."""
        self.visit(node.node, **kwargs)
        self.visit(node.target, **kwargs)

    def visit_For(self, node: nodes.For, **kwargs: t.Any) -> None:
        """Visiting stops at for blocks.  However the block sequence
        is visited as part of the outer scope.
        """
        self.visit(node.iter, **kwargs)

    def visit_CallBlock(self, node: nodes.CallBlock, **kwargs: t.Any) -> None:
        self.visit(node.call, **kwargs)

    def visit_FilterBlock(self, node: nodes.FilterBlock, **kwargs: t.Any) -> None:
        self.visit(node.filter, **kwargs)

    def visit_With(self, node: nodes.With, **kwargs: t.Any) -> None:
        for target in node.values:
            self.visit(target)

    def visit_AssignBlock(self, node: nodes.AssignBlock, **kwargs: t.Any) -> None:
        """Stop visiting at block assigns."""
        self.visit(node.target, **kwargs)

    def visit_Scope(self, node: nodes.Scope, **kwargs: t.Any) -> None:
        """Stop visiting at scopes."""

    def visit_Block(self, node: nodes.Block, **kwargs: t.Any) -> None:
        """Stop visiting at blocks."""

    def visit_OverlayScope(self, node: nodes.OverlayScope, **kwargs: t.Any) -> None:
        """Do not visit into overlay scopes."""
