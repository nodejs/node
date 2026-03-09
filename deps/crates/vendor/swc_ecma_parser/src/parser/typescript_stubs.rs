//! Stub implementations for TypeScript parsing methods when the `typescript`
//! feature is disabled.
//!
//! These methods are never called at runtime (the calls are guarded by
//! `syntax().typescript()` which returns `false` when the feature is disabled),
//! but Rust needs them to exist for compilation to succeed.

use swc_common::BytePos;
use swc_ecma_ast::*;

use crate::{input::Tokens, lexer::Token, PResult, Parser};

impl<I: Tokens> Parser<I> {
    pub(crate) fn try_parse_ts<T, F>(&mut self, _op: F) -> Option<T>
    where
        F: FnOnce(&mut Self) -> PResult<Option<T>>,
    {
        None
    }

    pub(crate) fn eat_any_ts_modifier(&mut self) -> PResult<bool> {
        Ok(false)
    }

    pub(crate) fn parse_ts_modifier(
        &mut self,
        _allowed_modifiers: &[&'static str],
        _stop_on_start_of_class_static_blocks: bool,
    ) -> PResult<Option<&'static str>> {
        unreachable!("parse_ts_modifier should not be called without typescript feature")
    }

    pub(crate) fn ts_look_ahead<T, F>(&mut self, _op: F) -> T
    where
        F: FnOnce(&mut Self) -> T,
    {
        unreachable!("ts_look_ahead should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_type_args(&mut self) -> PResult<Box<TsTypeParamInstantiation>> {
        unreachable!("parse_ts_type_args should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_type_ann(
        &mut self,
        _eat_colon: bool,
        _start: BytePos,
    ) -> PResult<Box<TsTypeAnn>> {
        unreachable!("parse_ts_type_ann should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_type_params(
        &mut self,
        _permit_in_out: bool,
        _permit_const: bool,
    ) -> PResult<Box<TsTypeParamDecl>> {
        unreachable!("parse_ts_type_params should not be called without typescript feature")
    }

    pub(crate) fn try_parse_ts_type_params(
        &mut self,
        _permit_in_out: bool,
        _permit_const: bool,
    ) -> PResult<Option<Box<TsTypeParamDecl>>> {
        Ok(None)
    }

    pub(crate) fn parse_ts_type_or_type_predicate_ann(
        &mut self,
        _return_token: Token,
    ) -> PResult<Box<TsTypeAnn>> {
        unreachable!(
            "parse_ts_type_or_type_predicate_ann should not be called without typescript feature"
        )
    }

    pub(super) fn try_parse_ts_type_args(&mut self) -> Option<Box<TsTypeParamInstantiation>> {
        None
    }

    pub(crate) fn try_parse_ts_type_ann(&mut self) -> PResult<Option<Box<TsTypeAnn>>> {
        Ok(None)
    }

    pub(super) fn next_then_parse_ts_type(&mut self) -> PResult<Box<TsType>> {
        unreachable!("next_then_parse_ts_type should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_enum_decl(
        &mut self,
        _start: BytePos,
        _is_const: bool,
    ) -> PResult<Box<TsEnumDecl>> {
        unreachable!("parse_ts_enum_decl should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_heritage_clause(&mut self) -> PResult<Vec<TsExprWithTypeArgs>> {
        unreachable!("parse_ts_heritage_clause should not be called without typescript feature")
    }

    pub(crate) fn try_parse_ts_index_signature(
        &mut self,
        _index_signature_start: BytePos,
        _readonly: bool,
        _is_static: bool,
    ) -> PResult<Option<TsIndexSignature>> {
        Ok(None)
    }

    pub(crate) fn parse_ts_import_equals_decl(
        &mut self,
        _start: BytePos,
        _id: Ident,
        _is_export: bool,
        _is_type_only: bool,
    ) -> PResult<Box<TsImportEqualsDecl>> {
        unreachable!("parse_ts_import_equals_decl should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_type_alias_decl(
        &mut self,
        _start: BytePos,
    ) -> PResult<Box<TsTypeAliasDecl>> {
        unreachable!("parse_ts_type_alias_decl should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_type(&mut self) -> PResult<Box<TsType>> {
        unreachable!("parse_ts_type should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_interface_decl(
        &mut self,
        _start: BytePos,
    ) -> PResult<Box<TsInterfaceDecl>> {
        unreachable!("parse_ts_interface_decl should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_type_assertion(&mut self, _start: BytePos) -> PResult<TsTypeAssertion> {
        unreachable!("parse_ts_type_assertion should not be called without typescript feature")
    }

    pub(crate) fn parse_ts_expr_stmt(
        &mut self,
        _decorators: Vec<Decorator>,
        _expr: Ident,
    ) -> PResult<Option<Decl>> {
        Ok(None)
    }

    pub(crate) fn try_parse_ts_declare(
        &mut self,
        _start: BytePos,
        _decorators: Vec<Decorator>,
    ) -> PResult<Option<Decl>> {
        Ok(None)
    }

    pub(crate) fn try_parse_ts_export_decl(
        &mut self,
        _decorators: Vec<Decorator>,
        _value: swc_atoms::Atom,
    ) -> Option<Decl> {
        None
    }

    pub(crate) fn try_parse_ts_generic_async_arrow_fn(
        &mut self,
        _start: BytePos,
    ) -> PResult<Option<ArrowExpr>> {
        Ok(None)
    }
}
