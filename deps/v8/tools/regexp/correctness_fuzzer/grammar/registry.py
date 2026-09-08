# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rule registry and the weighted derivation itself.

Each grammar alternative is a function registered with @rule; expand()
picks among the alternatives of a production that are eligible for the
current parameters and calls one.
"""

import collections
import contextlib
import re

Rule = collections.namedtuple("Rule", "prod name weight recursive guard fn")

# production name -> [Rule]; populated by the @rule decorator below.
GRAMMAR = {}


class GrammarError(Exception):
  """A malformed grammar: unknown production, duplicate rule name, or a
  production with no eligible alternative.

  Authoring mistakes, raised with the offending name -- otherwise only
  visible as a traceback from deep inside a derivation.
  """


def rule(prod, weight=1.0, recursive=False, guard=None):
  """Register a function as one alternative of production |prod|.

  weight     relative selection weight among the eligible alternatives.
  recursive  the alternative re-enters the grammar, so it is dropped once
             the depth budget is spent.
  guard      predicate on the context; a false guard makes the alternative
             ineligible (this is how the spec's [+UnicodeMode]-style
             parameters are expressed).
  """

  def deco(fn):
    alternatives = GRAMMAR.setdefault(prod, [])
    # Rules are addressed as "Production.rule" by --weight and by the coverage
    # report, so a duplicate name would make one of the two unreachable.
    if any(r.name == fn.__name__ for r in alternatives):
      raise GrammarError("duplicate rule %s.%s" % (prod, fn.__name__))
    alternatives.append(Rule(prod, fn.__name__, weight, recursive, guard, fn))
    return fn

  return deco


# Spec parameters, as predicates over the context.
UNICODE = lambda c: c.unicode_mode
NOT_UNICODE = lambda c: not c.unicode_mode
SETS = lambda c: c.unicode_sets_mode
NOT_SETS = lambda c: not c.unicode_sets_mode


class Context:
  """Derivation state: parameters, budget, and the bookkeeping that keeps
  generated references resolvable and subjects matchable (see gen_subject).
  """

  def __init__(self,
               rng,
               unicode_mode,
               unicode_sets_mode,
               max_depth,
               weights=None,
               coverage=None):
    self.rng = rng
    self.unicode_mode = unicode_mode
    self.unicode_sets_mode = unicode_sets_mode
    self.max_depth = max_depth
    self.depth = 0
    self.weights = weights or {}
    self.coverage = coverage
    self.capture_count = 0
    self.group_names = []
    self.literals = []
    # Nonzero inside a negated class, where a string-valued operand is a
    # static-semantics error rather than merely unusual.
    self.no_strings = 0
    # Nonzero inside a character class, where the `(?:)` separator
    # _concat_terms relies on is four more class members rather than a
    # zero-width group.
    self.in_class = 0
    # Set once the pattern uses `\k` as a legacy identity escape, which is
    # only legal while the pattern has no named group at all (see
    # legacy_k_escape).
    self.legacy_k_escape = False

  @contextlib.contextmanager
  def inside_class(self, no_strings=False):
    """Derive a class body: mark the subtree, optionally banning strings."""
    self.in_class += 1
    self.no_strings += no_strings
    try:
      yield
    finally:
      self.in_class -= 1
      self.no_strings -= no_strings

  def weight_of(self, r):
    return r.weight * self.weights.get("%s.%s" % (r.prod, r.name), 1.0)

  def note_literal(self, s):
    self.literals.append(s)
    return s

  def describe(self):
    """Parameter summary, for error messages."""
    return "unicode_mode=%r unicode_sets_mode=%r in_class=%d no_strings=%d" % (
        self.unicode_mode, self.unicode_sets_mode, self.in_class,
        self.no_strings)


def expand(ctx, prod):
  """Derive |prod|, honouring guards, the depth budget, and weights."""
  alternatives = GRAMMAR.get(prod)
  if alternatives is None:
    raise GrammarError("no such production %r (have: %s)" %
                       (prod, ", ".join(sorted(GRAMMAR))))
  rules = [r for r in alternatives if r.guard is None or r.guard(ctx)]
  if not rules:
    raise GrammarError("every alternative of %s is guarded off for %s" %
                       (prod, ctx.describe()))
  if ctx.depth >= ctx.max_depth:
    terminal = [r for r in rules if not r.recursive]
    # A production with no terminal alternative (Pattern, Disjunction) must
    # still expand; the budget is a bias, not a hard wall, and the recursive
    # rules below shrink their own fan-out once it is spent.
    rules = terminal or rules
  weights = [ctx.weight_of(r) for r in rules]
  total = sum(weights)
  if total <= 0:
    weights, total = [1.0] * len(rules), float(len(rules))
  pick = ctx.rng.random() * total
  for r, w in zip(rules, weights):
    pick -= w
    if pick <= 0:
      break
  if ctx.coverage is not None:
    ctx.coverage[(r.prod, r.name)] += 1
  if not r.recursive:
    return r.fn(ctx)
  ctx.depth += 1
  try:
    return r.fn(ctx)
  finally:
    ctx.depth -= 1


def concat_terms(parts):
  """Join Terms, keeping a digit-terminated escape away from a following digit.

  `\\1` and `\\0` are terminated by lookahead, not by their own syntax: put a
  digit after either and `\\1` `9` reads as backreference 11 and `\\0` `9` as a
  legacy octal escape.  The spec expresses this as [lookahead not in
  DecimalDigit]; concatenation is where the grammar has to honour it, since the
  two Terms are derived independently.  A non-capturing group is the neutral
  separator -- it cannot change what either side matches.
  """
  out = []
  for part in parts:
    if (out and part[:1].isdigit() and re.search(r"\\[0-9]+$", out[-1])):
      out.append("(?:)")
    out.append(part)
  return "".join(out)


def repeat(ctx, prod, lo, hi):
  """Concatenate |lo|..|hi| derivations of |prod|, shrinking past budget."""
  if ctx.depth >= ctx.max_depth:
    hi = lo
  return concat_terms(
      [expand(ctx, prod) for _ in range(ctx.rng.randint(lo, hi))])


def all_rules():
  return [(r.prod, r.name) for rs in GRAMMAR.values() for r in rs]
