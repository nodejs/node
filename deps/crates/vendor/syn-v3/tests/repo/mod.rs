#![allow(clippy::manual_assert)]

mod progress;

use self::progress::Progress;
use anyhow::Result;
use flate2::read::GzDecoder;
use rayon::iter::{IntoParallelRefIterator, ParallelIterator};
use rayon::ThreadPoolBuilder;
use std::collections::BTreeSet;
use std::env;
use std::ffi::OsStr;
use std::fs;
use std::path::{Path, PathBuf};
use tar::Archive;
use walkdir::{DirEntry, WalkDir};

// nightly-2026-07-18
const REVISION: &str = "b6839f4d0e2bd63b960bbff8619c6fdea27d81e5";

#[rustfmt::skip]
static EXCLUDE_FILES: &[&str] = &[
    // TODO: unsafe fields: `struct S { unsafe field: T }`
    // https://github.com/dtolnay/syn/issues/1792
    "src/tools/clippy/tests/ui/derive.rs",
    "src/tools/clippy/tests/ui/doc_unsafe.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/record_field_list.rs",
    "src/tools/rustfmt/tests/source/unsafe-field.rs",
    "src/tools/rustfmt/tests/target/unsafe-field.rs",
    "tests/ui/unsafe-binders/unsafe-binders-debuginfo.rs",
    "tests/ui/unsafe-fields/auxiliary/unsafe-fields-crate-dep.rs",

    // TODO: guard patterns: `match expr { (A if f()) | (B if g()) => {} }`
    // https://github.com/dtolnay/syn/issues/1793
    "src/tools/rustfmt/tests/target/guard_patterns.rs",
    "tests/ui/lint/unused/unused_parens/parens-around-guard-patterns-not-unused.rs",
    "tests/ui/pattern/rfc-3637-guard-patterns/liveness-ice-issue-146445.rs",
    "tests/ui/pattern/rfc-3637-guard-patterns/only-gather-locals-once.rs",
    "tests/ui/reachable/guard_read_for_never.rs",

    // TODO: final associated functions: `final fn`
    // https://github.com/dtolnay/syn/issues/1981
    "library/core/src/clone.rs",
    "library/core/src/io/size_hint.rs",
    "src/tools/rustfmt/tests/target/final-kw.rs",
    "tests/rustdoc-html/final-trait-method.rs",
    "tests/ui/traits/final/dyn-compat.rs",
    "tests/ui/traits/final/works.rs",

    // TODO: return type notation: `where T: Trait<method(): Send>` and `where T::method(..): Send`
    // https://github.com/dtolnay/syn/issues/1434
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/return_type_syntax_in_path.rs",
    "src/tools/rustfmt/tests/target/return-type-notation.rs",
    "tests/rustdoc-html/jump-to-def/assoc-items-extra.rs",
    "tests/rustdoc-html/return-type-notation.rs",
    "tests/rustdoc-json/return-type-notation.rs",
    "tests/ui/associated-type-bounds/all-generics-lookup.rs",
    "tests/ui/associated-type-bounds/implied-from-self-where-clause.rs",
    "tests/ui/associated-type-bounds/return-type-notation/basic.rs",
    "tests/ui/associated-type-bounds/return-type-notation/higher-ranked-bound-works.rs",
    "tests/ui/associated-type-bounds/return-type-notation/namespace-conflict.rs",
    "tests/ui/associated-type-bounds/return-type-notation/path-constrained-in-method.rs",
    "tests/ui/associated-type-bounds/return-type-notation/path-self-qself.rs",
    "tests/ui/associated-type-bounds/return-type-notation/path-works.rs",
    "tests/ui/associated-type-bounds/return-type-notation/trait-alias.rs",
    "tests/ui/associated-type-bounds/return-type-notation/unpretty-parenthesized.rs",
    "tests/ui/async-await/return-type-notation/issue-110963-late.rs",
    "tests/ui/async-await/return-type-notation/normalizing-self-auto-trait-issue-109924.rs",
    "tests/ui/async-await/return-type-notation/rtn-implied-in-supertrait.rs",
    "tests/ui/async-await/return-type-notation/super-method-bound.rs",
    "tests/ui/async-await/return-type-notation/supertrait-bound.rs",
    "tests/ui/borrowck/alias-liveness/rtn-static.rs",
    "tests/ui/feature-gates/feature-gate-return_type_notation.rs",

    // TODO: gen blocks and functions
    // https://github.com/dtolnay/syn/issues/1526
    "compiler/rustc_codegen_cranelift/example/gen_block_iterate.rs",
    "compiler/rustc_hir_analysis/src/collect/resolve_bound_vars.rs",
    "compiler/rustc_metadata/src/rmeta/decoder.rs",
    "compiler/rustc_middle/src/ty/closure.rs",
    "compiler/rustc_middle/src/ty/context.rs",
    "src/tools/clippy/tests/ui/infinite_loops.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/gen_blocks.rs",
    "tests/ui/async-await/async-drop/assign-incompatible-types.rs",
    "tests/ui/coroutine/async-gen-deduce-yield.rs",
    "tests/ui/coroutine/async-gen-yield-ty-is-unit.rs",
    "tests/ui/coroutine/async_gen_fn_iter.rs",
    "tests/ui/coroutine/gen_block_is_fused_iter.rs",
    "tests/ui/coroutine/gen_block_is_iter.rs",
    "tests/ui/coroutine/gen_block_iterate.rs",
    "tests/ui/coroutine/gen_fn_iter.rs",
    "tests/ui/coroutine/gen_fn_lifetime_capture.rs",
    "tests/ui/coroutine/other-attribute-on-gen.rs",
    "tests/ui/coroutine/return-types-diverge.rs",
    "tests/ui/higher-ranked/builtin-closure-like-bounds.rs",
    "tests/ui/sanitizer/cfi/coroutine.rs",

    // TODO: coroutines (static closure)
    // https://github.com/dtolnay/syn/issues/2019
    "src/tools/clippy/tests/ui/redundant_locals.rs",
    "src/tools/miri/tests/fail/coroutine-pinned-moved.rs",
    "src/tools/miri/tests/pass/stacked_borrows/coroutine-self-referential.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/lambda_expr.rs",
    "src/tools/rustfmt/tests/source/async_block.rs",
    "src/tools/rustfmt/tests/source/immovable_coroutines.rs",
    "src/tools/rustfmt/tests/target/async_block.rs",
    "src/tools/rustfmt/tests/target/immovable_coroutines.rs",
    "tests/ui/coroutine/addassign-yield.rs",
    "tests/ui/coroutine/drop-track-addassign-yield.rs",
    "tests/ui/coroutine/drop-tracking-yielding-in-match-guards.rs",
    "tests/ui/coroutine/issue-53548.rs",
    "tests/ui/coroutine/issue-61442-stmt-expr-with-drop.rs",
    "tests/ui/coroutine/pin-box-coroutine.rs",
    "tests/ui/coroutine/size-moved-locals.rs",
    "tests/ui/coroutine/static-coroutine.rs",
    "tests/ui/coroutine/static-mut-reference-across-yield.rs",
    "tests/ui/coroutine/static-reference-across-yield.rs",
    "tests/ui/expr/weird-exprs.rs",

    // TODO: `!` as a pattern
    // https://github.com/dtolnay/syn/issues/1546
    "tests/mir-opt/building/match/never_patterns.rs",
    "tests/pretty/never-pattern.rs",
    "tests/ui/reachable/never-pattern-closure-param-array.rs",
    "tests/ui/rfcs/rfc-0000-never_patterns/always-read-in-closure-capture.rs",
    "tests/ui/rfcs/rfc-0000-never_patterns/diverges.rs",
    "tests/ui/rfcs/rfc-0000-never_patterns/use-bindings.rs",

    // TODO: async trait bounds: `impl async Fn()`
    // https://github.com/dtolnay/syn/issues/1628
    "src/tools/miri/tests/pass/async-closure-captures.rs",
    "src/tools/miri/tests/pass/async-closure-drop.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/async_trait_bound.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/for_binder_bound.rs",
    "src/tools/rustfmt/tests/target/asyncness.rs",
    "tests/coverage/async_closure.rs",
    "tests/ui/async-await/async-closures/async-fn-mut-for-async-fn.rs",
    "tests/ui/async-await/async-closures/async-fn-once-for-async-fn.rs",
    "tests/ui/async-await/async-closures/auxiliary/foreign.rs",
    "tests/ui/async-await/async-closures/body-check-on-non-fnmut.rs",
    "tests/ui/async-await/async-closures/box-deref-in-debuginfo.rs",
    "tests/ui/async-await/async-closures/brand.rs",
    "tests/ui/async-await/async-closures/captures.rs",
    "tests/ui/async-await/async-closures/clone-closure.rs",
    "tests/ui/async-await/async-closures/constrained-but-no-upvars-yet.rs",
    "tests/ui/async-await/async-closures/debuginfo-by-move-body.rs",
    "tests/ui/async-await/async-closures/drop.rs",
    "tests/ui/async-await/async-closures/force-move-due-to-inferred-kind.rs",
    "tests/ui/async-await/async-closures/foreign.rs",
    "tests/ui/async-await/async-closures/inline-body.rs",
    "tests/ui/async-await/async-closures/mangle.rs",
    "tests/ui/async-await/async-closures/moro-example.rs",
    "tests/ui/async-await/async-closures/move-is-async-fn.rs",
    "tests/ui/async-await/async-closures/mut-ref-reborrow.rs",
    "tests/ui/async-await/async-closures/no-borrow-from-env.rs",
    "tests/ui/async-await/async-closures/non-copy-arg-does-not-force-inner-move.rs",
    "tests/ui/async-await/async-closures/overlapping-projs.rs",
    "tests/ui/async-await/async-closures/precise-captures.rs",
    "tests/ui/async-await/async-closures/refd.rs",
    "tests/ui/async-await/async-closures/signature-deduction.rs",
    "tests/ui/async-await/async-fn/edition-2015-not-async-bound.rs",
    "tests/ui/async-await/async-fn/higher-ranked-async-fn.rs",
    "tests/ui/async-await/async-fn/impl-trait.rs",
    "tests/ui/async-await/async-fn/project.rs",
    "tests/ui/async-await/async-fn/sugar.rs",

    // TODO: mutable by-reference bindings (mut ref)
    // https://github.com/dtolnay/syn/issues/1629
    "src/tools/rustfmt/tests/source/mut_ref.rs",
    "src/tools/rustfmt/tests/target/mut_ref.rs",
    "tests/ui/mut/mut-ref.rs",

    // TODO: postfix match
    // https://github.com/dtolnay/syn/issues/1630
    "src/tools/clippy/tests/ui/unnecessary_semicolon.rs",
    "src/tools/rustfmt/tests/source/postfix-match/pf-match.rs",
    "src/tools/rustfmt/tests/target/postfix-match/pf-match.rs",
    "tests/pretty/postfix-match/simple-matches.rs",
    "tests/ui/match/postfix-match/no-unused-parens.rs",
    "tests/ui/match/postfix-match/pf-match-chain.rs",
    "tests/ui/match/postfix-match/postfix-match.rs",

    // TODO: postfix yield
    // https://github.com/dtolnay/syn/issues/1890
    "tests/pretty/postfix-yield.rs",
    "tests/ui/coroutine/postfix-yield.rs",

    // TODO: delegation: `reuse Trait::bar { Box::new(self.0) }`
    // https://github.com/dtolnay/syn/issues/1580
    "src/tools/rustfmt/tests/target/issue_6513.rs",
    "tests/incremental/delegation-ice-155729.rs",
    "tests/pretty/delegation/delegation.rs",
    "tests/pretty/delegation/generics.rs",
    "tests/pretty/delegation/impl-reuse.rs",
    "tests/pretty/delegation/inherit-attributes.rs",
    "tests/pretty/delegation/inline-attribute.rs",
    "tests/pretty/delegation/self-mapping-output.rs",
    "tests/pretty/delegation/self-rename.rs",
    "tests/pretty/hir-delegation.rs",
    "tests/rustdoc-html/async/async-fn-delegation.rs",
    "tests/ui/delegation/auxiliary/recursive-delegation-aux.rs",
    "tests/ui/delegation/body-identity-glob.rs",
    "tests/ui/delegation/body-identity-list.rs",
    "tests/ui/delegation/explicit-paths-in-traits-pass.rs",
    "tests/ui/delegation/explicit-paths-pass.rs",
    "tests/ui/delegation/explicit-paths-signature-pass.rs",
    "tests/ui/delegation/fn-header.rs",
    "tests/ui/delegation/generics/const-type-ice-154334.rs",
    "tests/ui/delegation/generics/free-fn-to-free-fn-pass.rs",
    "tests/ui/delegation/generics/free-fn-to-trait-method-pass.rs",
    "tests/ui/delegation/generics/generics-aux-pass.rs",
    "tests/ui/delegation/generics/impl-to-free-fn-pass.rs",
    "tests/ui/delegation/generics/impl-trait-to-trait-method-pass.rs",
    "tests/ui/delegation/generics/inherent-impl-to-trait-method-pass.rs",
    "tests/ui/delegation/generics/mapping/free-to-free-pass.rs",
    "tests/ui/delegation/generics/mapping/free-to-trait-pass.rs",
    "tests/ui/delegation/generics/mapping/impl-trait-to-free-pass.rs",
    "tests/ui/delegation/generics/mapping/impl-trait-to-trait-pass.rs",
    "tests/ui/delegation/generics/mapping/inherent-impl-to-free-pass.rs",
    "tests/ui/delegation/generics/mapping/inherent-impl-to-trait-pass.rs",
    "tests/ui/delegation/generics/mapping/trait-to-free-pass.rs",
    "tests/ui/delegation/generics/mapping/trait-to-trait-pass.rs",
    "tests/ui/delegation/generics/self-rename.rs",
    "tests/ui/delegation/generics/stability-implications-non-local-defid.rs",
    "tests/ui/delegation/generics/synth-params-ice-154780.rs",
    "tests/ui/delegation/generics/trait-method-to-other-pass.rs",
    "tests/ui/delegation/generics/unelided-lifetime-in-sig-ice-156848.rs",
    "tests/ui/delegation/glob-glob.rs",
    "tests/ui/delegation/glob-override.rs",
    "tests/ui/delegation/glob.rs",
    "tests/ui/delegation/impl-reuse-pass.rs",
    "tests/ui/delegation/impl-trait.rs",
    "tests/ui/delegation/list.rs",
    "tests/ui/delegation/macro-inside-glob.rs",
    "tests/ui/delegation/macro-inside-list.rs",
    "tests/ui/delegation/method-call-priority.rs",
    "tests/ui/delegation/parse.rs",
    "tests/ui/delegation/recursive-delegation-pass.rs",
    "tests/ui/delegation/rename.rs",
    "tests/ui/delegation/self-coercion.rs",
    "tests/ui/delegation/self-mapping-arguments.rs",
    "tests/ui/delegation/target-expression-removal-pass.rs",

    // TODO: for await
    // https://github.com/dtolnay/syn/issues/1631
    "tests/ui/async-await/for-await-2015.rs",
    "tests/ui/async-await/for-await-passthrough.rs",
    "tests/ui/async-await/for-await.rs",

    // TODO: unparenthesized half-open range pattern inside slice pattern: `[1..]`
    // https://github.com/dtolnay/syn/issues/1769
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/range_pat.rs",
    "tests/ui/consts/miri_unleashed/const_refers_to_static_cross_crate.rs",

    // TODO: pinned type sugar: `&pin const Self`
    // https://github.com/dtolnay/syn/issues/1770
    "src/tools/rustfmt/tests/source/pin_sugar.rs",
    "src/tools/rustfmt/tests/target/pin_sugar.rs",
    "tests/pretty/pin-ergonomics-hir.rs",
    "tests/pretty/pin-ergonomics.rs",
    "tests/ui/pin-ergonomics/borrow.rs",
    "tests/ui/pin-ergonomics/pin-coercion.rs",
    "tests/ui/pin-ergonomics/pinned-drop-sugar-no-core.rs",
    "tests/ui/pin-ergonomics/pinned-drop-sugar.rs",
    "tests/ui/pin-ergonomics/pinned-drop.rs",
    "tests/ui/pin-ergonomics/sugar-self.rs",
    "tests/ui/pin-ergonomics/sugar.rs",
    "tests/ui/pin-ergonomics/user-type-projection.rs",

    // TODO: super let
    // https://github.com/dtolnay/syn/issues/1889
    "library/std/src/macros/tests.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/let_stmt.rs",
    "tests/ui/drop/if-let-super-let.rs",
    "tests/ui/drop/super-let-tail-expr-drop-order.rs",

    // TODO: "ergonomic clones": `f(obj.use)`, `thread::spawn(use || f(obj))`, `async use`
    // https://github.com/dtolnay/syn/issues/1802
    "src/tools/clippy/tests/ui/redundant_closure_call_early.rs",
    "src/tools/clippy/tests/ui/redundant_closure_call_fixable.rs",
    "src/tools/rustfmt/tests/source/ergonomic_clones.rs",
    "src/tools/rustfmt/tests/target/ergonomic_clones.rs",
    "tests/codegen-llvm/ergonomic-clones/closure.rs",
    "tests/mir-opt/ergonomic-clones/closure.rs",
    "tests/ui/ergonomic-clones/async/basic.rs",
    "tests/ui/ergonomic-clones/closure/basic.rs",
    "tests/ui/ergonomic-clones/closure/const-closure.rs",
    "tests/ui/ergonomic-clones/closure/mutation.rs",
    "tests/ui/ergonomic-clones/closure/nested.rs",
    "tests/ui/ergonomic-clones/closure/once-move-out-on-heap.rs",
    "tests/ui/ergonomic-clones/closure/with-binders.rs",
    "tests/ui/ergonomic-clones/dotuse/basic.rs",
    "tests/ui/ergonomic-clones/dotuse/block.rs",

    // TODO: move expression
    // https://github.com/dtolnay/syn/issues/1983
    "tests/ui/move-expr/borrow-only.rs",
    "tests/ui/move-expr/copy-type.rs",
    "tests/ui/move-expr/name-resolution.rs",
    "tests/ui/move-expr/nested-closures.rs",
    "tests/ui/move-expr/nested-move-expr.rs",
    "tests/ui/move-expr/parse-ambiguity.rs",
    "tests/ui/move-expr/plain-closure.rs",

    // TODO: contracts
    // https://github.com/dtolnay/syn/issues/1892
    "tests/ui/contracts/internal_machinery/contract-ast-extensions-nest.rs",
    "tests/ui/contracts/internal_machinery/contract-ast-extensions-tail.rs",
    "tests/ui/contracts/internal_machinery/contracts-lowering-ensures-is-not-inherited-when-nesting.rs",
    "tests/ui/contracts/internal_machinery/contracts-lowering-requires-is-not-inherited-when-nesting.rs",

    // TODO: frontmatter
    // https://github.com/dtolnay/syn/issues/1893
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/frontmatter.rs",
    "src/tools/rustfmt/tests/source/frontmatter_compact.rs",
    "src/tools/rustfmt/tests/source/frontmatter_escaped.rs",
    "src/tools/rustfmt/tests/source/frontmatter_spaced.rs",
    "src/tools/rustfmt/tests/target/frontmatter_compact.rs",
    "src/tools/rustfmt/tests/target/frontmatter_escaped.rs",
    "src/tools/rustfmt/tests/target/frontmatter_spaced.rs",
    "tests/rustdoc-html/source-code-pages/frontmatter.rs",
    "tests/ui/frontmatter/auxiliary/expr.rs",
    "tests/ui/frontmatter/auxiliary/lib.rs",
    "tests/ui/frontmatter/content-contains-whitespace.rs",
    "tests/ui/frontmatter/content-non-lexible-tokens.rs",
    "tests/ui/frontmatter/escape-hyphens-leading.rs",
    "tests/ui/frontmatter/escape-hyphens-nonleading-1.rs",
    "tests/ui/frontmatter/escape-hyphens-nonleading-2.rs",
    "tests/ui/frontmatter/escape-hyphens-nonleading-3.rs",
    "tests/ui/frontmatter/fence-whitespace-trailing-1.rs",
    "tests/ui/frontmatter/fence-whitespace-trailing-2.rs",
    "tests/ui/frontmatter/frontmatter-crlf.rs",
    "tests/ui/frontmatter/infostring-dot-nonleading.rs",
    "tests/ui/frontmatter/infostring-hyphen-nonleading.rs",
    "tests/ui/frontmatter/location-after-shebang.rs",
    "tests/ui/include-macros/auxiliary/shebang-expr.rs",
    "tests/ui/unpretty/frontmatter.rs",

    // TODO: `type const` associated items
    // https://github.com/dtolnay/syn/issues/1984
    "src/tools/clippy/tests/ui/crashes/mgca-16691.rs",
    "src/tools/clippy/tests/ui/trait_duplication_in_bounds_assoc_const_eq.rs",
    "src/tools/rustfmt/tests/source/direct_const_arg.rs",
    "src/tools/rustfmt/tests/target/direct_const_arg.rs",
    "tests/debuginfo/associated-const-bindings.rs",
    "tests/rustdoc-html/constant/ice-associated-const-equality-105952.rs",
    "tests/rustdoc-html/inline_cross/auxiliary/assoc-const-equality.rs",
    "tests/rustdoc-html/type-const-associated-const-no-body.rs",
    "tests/rustdoc-html/type-const-free-in-array.rs",
    "tests/rustdoc-html/type-const-inherent-with-body.rs",
    "tests/ui/associated-consts/issue-110933.rs",
    "tests/ui/associated-consts/type-const-in-array-len.rs",
    "tests/ui/associated-type-bounds/duplicate-bound.rs",
    "tests/ui/const-generics/associated-const-bindings/assoc-const.rs",
    "tests/ui/const-generics/associated-const-bindings/bound-var-in-ty.rs",
    "tests/ui/const-generics/associated-const-bindings/coexisting-with-type-binding.rs",
    "tests/ui/const-generics/associated-const-bindings/const_evaluatable_unchecked.rs",
    "tests/ui/const-generics/associated-const-bindings/dyn-compat-basic.rs",
    "tests/ui/const-generics/associated-const-bindings/dyn-compat-self-const-projections-in-methods.rs",
    "tests/ui/const-generics/associated-const-bindings/dyn-const-projection-escaping-bound-vars.rs",
    "tests/ui/const-generics/associated-const-bindings/equality-unused-issue-126729.rs",
    "tests/ui/const-generics/associated-const-bindings/equality_bound_with_infer.rs",
    "tests/ui/const-generics/associated-const-bindings/normalization-via-param-env.rs",
    "tests/ui/const-generics/associated-const-bindings/supertraits.rs",
    "tests/ui/const-generics/gca/basic-different-definitions.rs",
    "tests/ui/const-generics/gca/basic.rs",
    "tests/ui/const-generics/generic_const_exprs/auxiliary/non_local_type_const.rs",
    "tests/ui/const-generics/mgca/adt_expr_infers_from_value.rs",
    "tests/ui/const-generics/mgca/array-expr-with-assoc-const.rs",
    "tests/ui/const-generics/mgca/assoc-const-projection-in-bound.rs",
    "tests/ui/const-generics/mgca/assoc-const.rs",
    "tests/ui/const-generics/mgca/concrete-expr-with-generics-in-env.rs",
    "tests/ui/const-generics/mgca/explicit_anon_consts_literals_hack.rs",
    "tests/ui/const-generics/mgca/generic-args-on-enum-variant-segments.rs",
    "tests/ui/const-generics/mgca/multi_braced_direct_const_args.rs",
    "tests/ui/const-generics/mgca/tuple_ctor_arg_simple.rs",
    "tests/ui/const-generics/mgca/tuple_expr_arg_simple.rs",
    "tests/ui/const-generics/mgca/type-const-ctor-148953.rs",
    "tests/ui/const-generics/mgca/type-const-used-in-trait.rs",
    "tests/ui/const-generics/mgca/type_const-array-return.rs",
    "tests/ui/const-generics/mgca/type_const-incemental-compile.rs",
    "tests/ui/const-generics/mgca/type_const-pub.rs",
    "tests/ui/const-generics/mgca/type_const-use.rs",
    "tests/ui/const-generics/mgca/type_const_in_pattern.rs",
    "tests/ui/generic-const-items/assoc-const-bindings.rs",
    "tests/ui/generic-const-items/type-const-nested-assoc-const.rs",
    "tests/ui/sanitizer/cfi/assoc-const-projection-issue-151878.rs",
    "tests/ui/supertrait-shadowing/assoc-const.rs",
    "tests/ui/supertrait-shadowing/out-of-scope.rs",
    "tests/ui/supertrait-shadowing/type-dependent.rs",

    // TODO: const block in path argument: `T: Trait<const { ... }>`
    // https://github.com/dtolnay/syn/issues/1985
    "tests/ui/const-generics/mgca/higher-ranked-lts-good.rs",
    "tests/ui/traits/associated_type_bound/generic-const-args-default.rs",

    // TODO: item-level const blocks
    // https://github.com/dtolnay/syn/issues/1986
    "src/tools/rustfmt/tests/source/const-block-items.rs",
    "src/tools/rustfmt/tests/target/const-block-items.rs",
    "tests/ui/consts/const-block-items/assert-pass.rs",
    "tests/ui/consts/const-block-items/hir.rs",
    "tests/ui/parser/const-block-items/attrs.rs",
    "tests/ui/parser/const-block-items/mod-in-fn.rs",
    "tests/ui/parser/const-block-items/pub.rs",

    // TODO: mut-restricted fields
    // https://github.com/dtolnay/syn/issues/1987
    "src/tools/rustfmt/tests/source/mut-restriction.rs",
    "src/tools/rustfmt/tests/target/mut-restriction.rs",
    "tests/pretty/hir-mut-restriction.rs",
    "tests/ui/mut-restriction/auxiliary/external-mut-restriction.rs",

    // TODO: heterogeneous try-blocks: `try bikeshed Option<_>`
    // https://github.com/dtolnay/syn/issues/1988
    "src/tools/rustfmt/tests/source/try_blocks_heterogeneous.rs",
    "src/tools/rustfmt/tests/target/try_blocks_heterogeneous.rs",
    "tests/pretty/try-blocks.rs",
    "tests/ui/try-block/try-block-heterogeneous.rs",

    // TODO: `|| .. .method()`
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/closure_range_method_call.rs",
    "src/tools/rustfmt/tests/source/issue-4808.rs",

    // Compile-fail expr parameter in const generic position: `f::<1 + 2>()`
    "tests/ui/const-generics/early/closing-args-token.rs",
    "tests/ui/const-generics/early/const-expression-parameter.rs",

    // Compile-fail variadics in not the last position of a function parameter list
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/fn_def_param.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/param_list_vararg.rs",
    "tests/ui/parser/variadic-ffi-syntactic-pass.rs",

    // Need at least one trait in impl Trait, no such type as impl 'static
    "tests/ui/type-alias-impl-trait/generic_type_does_not_live_long_enough.rs",

    // Negative polarity trait bound: `where T: !Copy`
    "src/tools/rustfmt/tests/target/negative-bounds.rs",
    "tests/ui/traits/negative-bounds/supertrait.rs",

    // Outdated const impl syntax: `impl const Trait for Type`
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/impl_item_const.rs",
    "src/tools/rust-analyzer/crates/test-utils/src/minicore.rs",

    // Const impl that is not a trait impl: `impl ~const T {}`
    "tests/ui/traits/const-traits/syntax.rs",

    // Lifetimes and types out of order in angle bracketed path arguments
    "tests/ui/parser/constraints-before-generic-args-syntactic-pass.rs",

    // Deprecated anonymous parameter syntax in traits
    "src/tools/rustfmt/tests/source/trait.rs",
    "src/tools/rustfmt/tests/target/trait.rs",
    "tests/pretty/hir-fn-params.rs",
    "tests/rustdoc-html/anon-fn-params.rs",
    "tests/rustdoc-html/auxiliary/ext-anon-fn-params.rs",
    "tests/ui/anon-params/anon-params-trait-method-multiple.rs",
    "tests/ui/fn/anonymous-parameters-trait-13105.rs",
    "tests/ui/proc-macro/trait-fn-args-2015.rs",
    "tests/ui/trait-bounds/anonymous-parameters-13775.rs",

    // Deprecated where-clause location
    "src/tools/rustfmt/tests/source/issue_4257.rs",
    "src/tools/rustfmt/tests/source/issue_4911.rs",
    "src/tools/rustfmt/tests/target/issue_4257.rs",
    "src/tools/rustfmt/tests/target/issue_4911.rs",
    "tests/pretty/gat-bounds.rs",
    "tests/ui/where-clauses/cfg-attr-issue-138010-1.rs",

    // Dual where-clauses on the same type alias.
    "src/tools/rustfmt/tests/source/type-alias-where-clauses-with-comments.rs",
    "src/tools/rustfmt/tests/source/type-alias-where-clauses.rs",
    "src/tools/rustfmt/tests/target/type-alias-where-clauses-with-comments.rs",
    "src/tools/rustfmt/tests/target/type-alias-where-clauses.rs",

    // Deprecated trait object syntax with parenthesized generic arguments and no dyn keyword
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/path_fn_trait_args.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/typepathfn_with_coloncolon.rs",
    "src/tools/rustfmt/tests/source/attrib.rs",
    "src/tools/rustfmt/tests/source/closure.rs",
    "src/tools/rustfmt/tests/source/existential_type.rs",
    "src/tools/rustfmt/tests/source/fn-simple.rs",
    "src/tools/rustfmt/tests/source/fn_args_layout-vertical.rs",
    "src/tools/rustfmt/tests/source/issue-4689/one.rs",
    "src/tools/rustfmt/tests/source/issue-4689/two.rs",
    "src/tools/rustfmt/tests/source/paths.rs",
    "src/tools/rustfmt/tests/source/structs.rs",
    "src/tools/rustfmt/tests/target/attrib.rs",
    "src/tools/rustfmt/tests/target/closure.rs",
    "src/tools/rustfmt/tests/target/existential_type.rs",
    "src/tools/rustfmt/tests/target/fn-simple.rs",
    "src/tools/rustfmt/tests/target/fn.rs",
    "src/tools/rustfmt/tests/target/fn_args_layout-vertical.rs",
    "src/tools/rustfmt/tests/target/issue-4689/one.rs",
    "src/tools/rustfmt/tests/target/issue-4689/two.rs",
    "src/tools/rustfmt/tests/target/paths.rs",
    "src/tools/rustfmt/tests/target/structs.rs",
    "tests/codegen-units/item-collection/non-generic-closures.rs",
    "tests/debuginfo/recursive-enum.rs",
    "tests/pretty/closure-reform-pretty.rs",
    "tests/run-make/reproducible-build-2/reproducible-build.rs",
    "tests/run-make/reproducible-build/reproducible-build.rs",
    "tests/ui/impl-trait/generic-with-implicit-hrtb-without-dyn.rs",
    "tests/ui/lifetimes/auxiliary/lifetime_bound_will_change_warning_lib.rs",
    "tests/ui/lifetimes/bare-trait-object-borrowck.rs",
    "tests/ui/lifetimes/bare-trait-object.rs",
    "tests/ui/parser/bounds-obj-parens.rs",

    // Various extensions to Rust syntax made up by rust-analyzer
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/asm_piece_attr.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/assoc_type_bound.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/const_param_default_path.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/deref_pat.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/field_expr.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/generic_arg_bounds.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/global_asm.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/include_bytes.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/not_null_pat.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/pattern_type.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/question_for_type_trait_bound.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/record_mut_restrictions_after.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/record_mut_restrictions_before.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/ref_expr.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/try_block_expr.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/tuple_mut_restrictions.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/type_const.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/use_tree_abs_star.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/ok/0015_use_tree.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/ok/0019_enums.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/ok/0029_range_forms.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/ok/0051_parameter_attrs.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/ok/0055_dot_dot_dot.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/ok/0068_item_modifiers.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/0031_block_inner_attrs.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/0038_endless_inclusive_range.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/0045_ambiguous_trait_object.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/0046_mutable_const_item.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/0224_dangling_dyn.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/0261_dangling_impl_undeclared_lifetime.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/dangling_impl.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/dangling_impl_reference.rs",
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/validation/impl_trait_lifetime_only.rs",

    // Placeholder syntax for "throw expressions"
    "compiler/rustc_expand/src/module.rs",
    "compiler/rustc_trait_selection/src/error_reporting/infer/need_type_info.rs",
    "src/tools/clippy/tests/ui/needless_return.rs",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/yeet_expr.rs",
    "tests/pretty/yeet-expr.rs",
    "tests/ui/contracts/contracts-ensures-early-fn-exit.rs",
    "tests/ui/try-trait/yeet-for-option.rs",
    "tests/ui/try-trait/yeet-for-result.rs",

    // Edition 2015 code using identifiers that are now keywords
    // TODO: some of these we should probably parse
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/ok/dyn_trait_type_weak.rs",
    "src/tools/rustfmt/tests/source/configs/indent_style/block_call.rs",
    "src/tools/rustfmt/tests/source/configs/use_try_shorthand/false.rs",
    "src/tools/rustfmt/tests/source/configs/use_try_shorthand/true.rs",
    "src/tools/rustfmt/tests/source/issue_1306.rs",
    "src/tools/rustfmt/tests/source/try-conversion.rs",
    "src/tools/rustfmt/tests/target/configs/indent_style/block_call.rs",
    "src/tools/rustfmt/tests/target/configs/use_try_shorthand/false.rs",
    "src/tools/rustfmt/tests/target/issue-1681.rs",
    "src/tools/rustfmt/tests/target/issue_1306.rs",
    "tests/ui/dyn-keyword/dyn-2015-no-warnings-without-lints.rs",
    "tests/ui/editions/edition-keywords-2015-2015.rs",
    "tests/ui/editions/edition-keywords-2015-2018.rs",
    "tests/ui/lint/keyword-idents/auxiliary/multi_file_submod.rs",
    "tests/ui/lint/lint_pre_expansion_extern_module_aux.rs",
    "tests/ui/macros/macro-comma-support-rpass.rs",
    "tests/ui/macros/try-macro.rs",
    "tests/ui/parser/extern-crate-async.rs",
    "tests/ui/try-block/try-is-identifier-edition2015.rs",

    // Excessive nesting
    "tests/ui/expr/if/expr-stack-overflow.rs",

    // Testing tools on invalid syntax
    "src/tools/clippy/tests/ui/non_expressive_names_error_recovery.rs",
    "src/tools/rustfmt/tests/coverage/target/comments.rs",
    "src/tools/rustfmt/tests/parser/issue-4126/invalid.rs",
    "src/tools/rustfmt/tests/parser/issue_4418.rs",
    "src/tools/rustfmt/tests/parser/stashed-diag.rs",
    "src/tools/rustfmt/tests/parser/stashed-diag2.rs",
    "src/tools/rustfmt/tests/parser/unclosed-delims/issue_4466.rs",
    "src/tools/rustfmt/tests/source/configs/disable_all_formatting/true.rs",
    "src/tools/rustfmt/tests/source/configs/spaces_around_ranges/false.rs",
    "src/tools/rustfmt/tests/source/configs/spaces_around_ranges/true.rs",
    "src/tools/rustfmt/tests/source/impls.rs",
    "src/tools/rustfmt/tests/source/type.rs",
    "src/tools/rustfmt/tests/target/configs/spaces_around_ranges/false.rs",
    "src/tools/rustfmt/tests/target/configs/spaces_around_ranges/true.rs",
    "src/tools/rustfmt/tests/target/impl.rs",
    "src/tools/rustfmt/tests/target/impls.rs",
    "src/tools/rustfmt/tests/target/type.rs",
    "src/tools/rustfmt/tests/target/unsafe_extern_blocks.rs",
    "tests/ui/generics/issue-94432-garbage-ice.rs",

    // Not actually test cases
    "tests/ui/lint/expansion-time-include.rs",
    "tests/ui/macros/auxiliary/macro-comma-support.rs",
    "tests/ui/macros/auxiliary/macro-include-items-expr.rs",
    "tests/ui/macros/include-single-expr-helper.rs",
    "tests/ui/macros/include-single-expr-helper-1.rs",
    "tests/ui/parser/issues/auxiliary/issue-21146-inc.rs",
];

#[rustfmt::skip]
static EXCLUDE_DIRS: &[&str] = &[
    // Inputs that intentionally do not parse
    "src/tools/rust-analyzer/crates/parser/test_data/parser/err",
    "src/tools/rust-analyzer/crates/parser/test_data/parser/inline/err",

    // Inputs that lex but do not necessarily parse
    "src/tools/rust-analyzer/crates/parser/test_data/lexer",

    // Inputs that used to crash rust-analyzer, but aren't necessarily supposed to parse
    "src/tools/rust-analyzer/crates/syntax/test_data/parser/fuzz-failures",
    "src/tools/rust-analyzer/crates/syntax/test_data/reparse/fuzz-failures",

    // Inputs that crash rustc, making no claim about whether they are valid Rust
    "tests/crashes",
];

// Directories in which a .stderr implies the corresponding .rs is not expected
// to work.
static UI_TEST_DIRS: &[&str] = &["tests/ui", "tests/rustdoc-ui"];

pub fn for_each_rust_file(for_each: impl Fn(&Path) + Sync + Send) {
    let mut rs_files = BTreeSet::new();

    let repo_dir = Path::new("tests/rust");
    for entry in WalkDir::new(repo_dir)
        .into_iter()
        .filter_entry(base_dir_filter)
    {
        let entry = entry.unwrap();
        if !entry.file_type().is_dir() {
            rs_files.insert(entry.into_path());
        }
    }

    for ui_test_dir in UI_TEST_DIRS {
        for entry in WalkDir::new(repo_dir.join(ui_test_dir)) {
            let mut path = entry.unwrap().into_path();
            if path.extension() == Some(OsStr::new("stderr")) {
                loop {
                    rs_files.remove(&path.with_extension("rs"));
                    path = path.with_extension("");
                    if path.extension().is_none() {
                        break;
                    }
                }
            }
        }
    }

    rs_files.par_iter().map(PathBuf::as_path).for_each(for_each);
}

pub fn base_dir_filter(entry: &DirEntry) -> bool {
    let path = entry.path();

    let mut path_string = path.to_string_lossy();
    if cfg!(windows) {
        path_string = path_string.replace('\\', "/").into();
    }
    let path_string = if path_string == "tests/rust" {
        return true;
    } else if let Some(path) = path_string.strip_prefix("tests/rust/") {
        path
    } else {
        panic!("unexpected path in Rust dist: {}", path_string);
    };

    if path.is_dir() {
        return !EXCLUDE_DIRS.contains(&path_string);
    }

    if path.extension() != Some(OsStr::new("rs")) {
        return false;
    }

    !EXCLUDE_FILES.contains(&path_string)
}

#[allow(dead_code)]
pub fn edition(path: &Path) -> &'static str {
    if path.ends_with("dyn-2015-no-warnings-without-lints.rs") {
        "2015"
    } else {
        "2021"
    }
}

#[allow(dead_code)]
pub fn abort_after() -> usize {
    match env::var("ABORT_AFTER_FAILURE") {
        Ok(s) => s.parse().expect("failed to parse ABORT_AFTER_FAILURE"),
        Err(_) => usize::MAX,
    }
}

pub fn rayon_init() {
    let stack_size = match env::var("RUST_MIN_STACK") {
        Ok(s) => s.parse().expect("failed to parse RUST_MIN_STACK"),
        Err(_) => 1024 * 1024 * if cfg!(debug_assertions) { 50 } else { 30 },
    };
    ThreadPoolBuilder::new()
        .stack_size(stack_size)
        .build_global()
        .unwrap();
}

pub fn clone_rust() {
    let needs_clone = match fs::read_to_string("tests/rust/COMMIT") {
        Err(_) => true,
        Ok(contents) => contents.trim() != REVISION,
    };
    if needs_clone {
        download_and_unpack().unwrap();
    }

    let mut missing = String::new();
    let test_src = Path::new("tests/rust");

    let mut exclude_files_set = BTreeSet::new();
    for exclude in EXCLUDE_FILES {
        if !exclude_files_set.insert(exclude) {
            panic!("duplicate path in EXCLUDE_FILES: {}", exclude);
        }
        for dir in EXCLUDE_DIRS {
            if Path::new(exclude).starts_with(dir) {
                panic!("excluded file {} is inside an excluded dir", exclude);
            }
        }
        if !test_src.join(exclude).is_file() {
            missing += "\ntests/rust/";
            missing += exclude;
        }
    }

    let mut exclude_dirs_set = BTreeSet::new();
    for exclude in EXCLUDE_DIRS {
        if !exclude_dirs_set.insert(exclude) {
            panic!("duplicate path in EXCLUDE_DIRS: {}", exclude);
        }
        if !test_src.join(exclude).is_dir() {
            missing += "\ntests/rust/";
            missing += exclude;
            missing += "/";
        }
    }

    if !missing.is_empty() {
        panic!("excluded test file does not exist:{}\n", missing);
    }
}

fn download_and_unpack() -> Result<()> {
    let url = format!("https://github.com/rust-lang/rust/archive/{REVISION}.tar.gz");
    errorf!("downloading {url}\n");

    let response = reqwest::blocking::get(url)?.error_for_status()?;
    let progress = Progress::new(response);
    let decoder = GzDecoder::new(progress);
    let mut archive = Archive::new(decoder);
    let prefix = format!("rust-{}", REVISION);

    let tests_rust = Path::new("tests/rust");
    if tests_rust.exists() {
        fs::remove_dir_all(tests_rust)?;
    }

    for entry in archive.entries()? {
        let mut entry = entry?;
        let path = entry.path()?;
        if path == Path::new("pax_global_header") {
            continue;
        }
        let relative = path.strip_prefix(&prefix)?;
        let out = tests_rust.join(relative);
        entry.unpack(&out)?;
    }

    fs::write("tests/rust/COMMIT", REVISION)?;
    Ok(())
}
