use swc_common::Spanned;
use swc_ecma_ast::*;
use swc_ecma_codegen_macros::node_impl;

#[cfg(swc_ast_unknown)]
use crate::unknown_error;

#[node_impl]
impl MacroNode for ParamOrTsParamProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            ParamOrTsParamProp::Param(n) => emit!(n),
            ParamOrTsParamProp::TsParamProp(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }

        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsArrayType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.elem_type);
        punct!(emitter, "[");
        punct!(emitter, "]");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsAsExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.expr);

        space!(emitter);
        keyword!(emitter, "as");
        space!(emitter);

        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsSatisfiesExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.expr);

        space!(emitter);
        keyword!(emitter, "satisfies");
        space!(emitter);

        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsCallSignatureDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.type_params);

        punct!(emitter, "(");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::Parameters)?;
        punct!(emitter, ")");

        if let Some(type_ann) = &self.type_ann {
            space!(emitter);
            punct!(emitter, ":");
            space!(emitter);

            emit!(type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsConditionalType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.check_type);
        space!(emitter);

        keyword!(emitter, "extends");
        space!(emitter);

        emit!(self.extends_type);
        space!(emitter);
        punct!(emitter, "?");

        space!(emitter);
        emit!(self.true_type);
        space!(emitter);

        punct!(emitter, ":");

        space!(emitter);
        emit!(self.false_type);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsConstructSignatureDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "new");
        if let Some(type_params) = &self.type_params {
            space!(emitter);
            emit!(type_params);
        }

        punct!(emitter, "(");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::Parameters)?;
        punct!(emitter, ")");

        if let Some(type_ann) = &self.type_ann {
            punct!(emitter, ":");
            space!(emitter);
            emit!(type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsConstructorType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.is_abstract {
            keyword!(emitter, "abstract");
            space!(emitter);
        }

        keyword!(emitter, "new");
        if let Some(type_params) = &self.type_params {
            space!(emitter);
            emit!(type_params);
        }

        punct!(emitter, "(");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::Parameters)?;
        punct!(emitter, ")");

        formatting_space!(emitter);
        punct!(emitter, "=>");
        formatting_space!(emitter);

        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsEntityName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self {
            TsEntityName::TsQualifiedName(n) => {
                emit!(n);
            }
            TsEntityName::Ident(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsEnumDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.declare {
            keyword!(emitter, "declare");
            space!(emitter);
        }

        if self.is_const {
            keyword!(emitter, "const");
            space!(emitter);
        }

        keyword!(emitter, "enum");
        space!(emitter);

        emit!(self.id);
        formatting_space!(emitter);

        punct!(emitter, "{");

        emitter.emit_list(self.span, Some(&self.members), ListFormat::EnumMembers)?;

        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsEnumMember {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.id);

        if let Some(init) = &self.init {
            formatting_space!(emitter);
            punct!(emitter, "=");
            formatting_space!(emitter);
            emit!(init);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsEnumMemberId {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsEnumMemberId::Ident(n) => emit!(n),
            TsEnumMemberId::Str(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsExportAssignment {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "export");
        formatting_space!(emitter);
        punct!(emitter, "=");
        formatting_space!(emitter);
        emit!(self.expr);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsExprWithTypeArgs {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.expr);

        emit!(self.type_args);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsExternalModuleRef {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "require");
        punct!(emitter, "(");
        emit!(self.expr);
        punct!(emitter, ")");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsFnOrConstructorType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self {
            TsFnOrConstructorType::TsFnType(n) => emit!(n),
            TsFnOrConstructorType::TsConstructorType(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsFnParam {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsFnParam::Ident(n) => emit!(n),
            TsFnParam::Array(n) => emit!(n),
            TsFnParam::Rest(n) => emit!(n),
            TsFnParam::Object(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsFnType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.type_params);

        punct!(emitter, "(");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::Parameters)?;
        punct!(emitter, ")");

        formatting_space!(emitter);
        punct!(emitter, "=>");
        formatting_space!(emitter);

        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsImportEqualsDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.is_export {
            keyword!(emitter, "export");
            space!(emitter);
        }

        keyword!(emitter, "import");
        space!(emitter);

        if self.is_type_only {
            keyword!(emitter, "type");
            space!(emitter);
        }

        emit!(self.id);

        formatting_space!(emitter);

        punct!(emitter, "=");
        formatting_space!(emitter);

        emit!(self.module_ref);
        formatting_semi!(emitter);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsIndexSignature {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.readonly {
            keyword!(emitter, "readonly");
            formatting_space!(emitter);
        }

        punct!(emitter, "[");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::Parameters)?;
        punct!(emitter, "]");

        if let Some(type_ann) = &self.type_ann {
            punct!(emitter, ":");
            formatting_space!(emitter);
            emit!(type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsIndexedAccessType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.obj_type);

        punct!(emitter, "[");
        emit!(self.index_type);
        punct!(emitter, "]");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsInferType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "infer");
        space!(emitter);
        emit!(self.type_param);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsInterfaceBody {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "{");

        emitter.emit_list(self.span, Some(&self.body), ListFormat::InterfaceMembers)?;

        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsInterfaceDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.declare {
            keyword!(emitter, "declare");
            space!(emitter);
        }

        keyword!(emitter, "interface");
        space!(emitter);

        emit!(self.id);

        if let Some(type_params) = &self.type_params {
            emit!(type_params);
        }

        if !self.extends.is_empty() {
            space!(emitter);

            keyword!(emitter, "extends");

            space!(emitter);

            emitter.emit_list(
                self.span,
                Some(&self.extends),
                ListFormat::HeritageClauseTypes,
            )?;
        }

        formatting_space!(emitter);

        emit!(self.body);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsIntersectionType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emitter.emit_list(
            self.span,
            Some(&self.types),
            ListFormat::IntersectionTypeConstituents,
        )?;
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsKeywordType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self.kind {
            TsKeywordTypeKind::TsAnyKeyword => keyword!(emitter, self.span, "any"),
            TsKeywordTypeKind::TsUnknownKeyword => keyword!(emitter, self.span, "unknown"),
            TsKeywordTypeKind::TsNumberKeyword => keyword!(emitter, self.span, "number"),
            TsKeywordTypeKind::TsObjectKeyword => keyword!(emitter, self.span, "object"),
            TsKeywordTypeKind::TsBooleanKeyword => keyword!(emitter, self.span, "boolean"),
            TsKeywordTypeKind::TsBigIntKeyword => keyword!(emitter, self.span, "bigint"),
            TsKeywordTypeKind::TsStringKeyword => keyword!(emitter, self.span, "string"),
            TsKeywordTypeKind::TsSymbolKeyword => keyword!(emitter, self.span, "symbol"),
            TsKeywordTypeKind::TsVoidKeyword => keyword!(emitter, self.span, "void"),
            TsKeywordTypeKind::TsUndefinedKeyword => keyword!(emitter, self.span, "undefined"),
            TsKeywordTypeKind::TsNullKeyword => keyword!(emitter, self.span, "null"),
            TsKeywordTypeKind::TsNeverKeyword => keyword!(emitter, self.span, "never"),
            TsKeywordTypeKind::TsIntrinsicKeyword => keyword!(emitter, self.span, "intrinsic"),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsLit {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsLit::BigInt(n) => emit!(n),
            TsLit::Number(n) => emit!(n),
            TsLit::Str(n) => emit!(n),
            TsLit::Bool(n) => emit!(n),
            TsLit::Tpl(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTplLitType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "`");

        for i in 0..(self.quasis.len() + self.types.len()) {
            if i % 2 == 0 {
                emit!(self.quasis[i / 2]);
            } else {
                punct!(emitter, "${");
                emit!(self.types[i / 2]);
                punct!(emitter, "}");
            }
        }

        punct!(emitter, "`");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsLitType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.lit);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsMappedType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "{");
        emitter.wr.write_line()?;
        emitter.wr.increase_indent()?;

        match &self.readonly {
            None => {}
            Some(tpm) => match tpm {
                TruePlusMinus::True => {
                    keyword!(emitter, "readonly");
                    space!(emitter);
                }
                TruePlusMinus::Plus => {
                    punct!(emitter, "+");
                    keyword!(emitter, "readonly");
                    space!(emitter);
                }
                TruePlusMinus::Minus => {
                    punct!(emitter, "-");
                    keyword!(emitter, "readonly");
                    space!(emitter);
                }
                #[cfg(swc_ast_unknown)]
                _ => return Err(unknown_error()),
            },
        }

        punct!(emitter, "[");

        emit!(self.type_param.name);

        if let Some(constraints) = &self.type_param.constraint {
            space!(emitter);
            keyword!(emitter, "in");
            space!(emitter);
            emit!(constraints);
        }

        if let Some(default) = &self.type_param.default {
            formatting_space!(emitter);
            punct!(emitter, "=");
            formatting_space!(emitter);
            emit!(default);
        }

        if let Some(name_type) = &self.name_type {
            space!(emitter);
            keyword!(emitter, "as");
            space!(emitter);
            emit!(name_type);
        }

        punct!(emitter, "]");

        match self.optional {
            None => {}
            Some(tpm) => match tpm {
                TruePlusMinus::True => {
                    punct!(emitter, "?");
                }
                TruePlusMinus::Plus => {
                    punct!(emitter, "+");
                    punct!(emitter, "?");
                }
                TruePlusMinus::Minus => {
                    punct!(emitter, "-");
                    punct!(emitter, "?");
                }
                #[cfg(swc_ast_unknown)]
                _ => return Err(unknown_error()),
            },
        }

        if let Some(type_ann) = &self.type_ann {
            punct!(emitter, ":");
            space!(emitter);
            emit!(type_ann);
        }

        formatting_semi!(emitter);

        emitter.wr.write_line()?;
        emitter.wr.decrease_indent()?;
        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsMethodSignature {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.computed {
            punct!(emitter, "[");
            emit!(self.key);
            punct!(emitter, "]");
        } else {
            emit!(self.key)
        }

        if self.optional {
            punct!(emitter, "?");
        }

        if let Some(type_params) = &self.type_params {
            emit!(type_params);
        }

        punct!(emitter, "(");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::Parameters)?;
        punct!(emitter, ")");

        if let Some(ref type_ann) = self.type_ann {
            punct!(emitter, ":");
            formatting_space!(emitter);
            emit!(type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsModuleBlock {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_list(
            self.span,
            Some(&self.body),
            ListFormat::SourceFileStatements,
        )?;
        emitter.emit_leading_comments_of_span(self.span(), false)?;
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsModuleDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.declare {
            keyword!(emitter, "declare");
            space!(emitter);
        }

        if self.global {
            keyword!(emitter, "global");
        } else {
            match &self.id {
                // prefer namespace keyword because TS might
                // deprecate the module keyword in this context
                TsModuleName::Ident(_) => keyword!(emitter, "namespace"),
                TsModuleName::Str(_) => keyword!(emitter, "module"),
                #[cfg(swc_ast_unknown)]
                _ => return Err(unknown_error()),
            }
            space!(emitter);
            emit!(self.id);
        }

        if let Some(mut body) = self.body.as_ref() {
            while let TsNamespaceBody::TsNamespaceDecl(decl) = body {
                punct!(emitter, ".");
                emit!(decl.id);
                body = &*decl.body;
            }
            formatting_space!(emitter);
            emit!(body);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsModuleName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsModuleName::Ident(n) => emit!(n),
            TsModuleName::Str(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsModuleRef {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self {
            TsModuleRef::TsEntityName(n) => emit!(n),
            TsModuleRef::TsExternalModuleRef(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsNamespaceBody {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "{");
        emitter.wr.increase_indent()?;
        match self {
            TsNamespaceBody::TsModuleBlock(n) => emit!(n),
            TsNamespaceBody::TsNamespaceDecl(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        emitter.wr.decrease_indent()?;
        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsNamespaceDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.declare {
            keyword!(emitter, "declare");
            space!(emitter);
        }

        keyword!(emitter, "namespace");
        space!(emitter);
        emit!(self.id);
        formatting_space!(emitter);

        emit!(self.body);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsNamespaceExportDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "export");
        space!(emitter);
        punct!(emitter, "=");
        space!(emitter);
        emit!(self.id);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsNonNullExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.expr);
        punct!(emitter, "!");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsOptionalType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.type_ann);
        punct!(emitter, "?");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsParamProp {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emitter.emit_list(self.span, Some(&self.decorators), ListFormat::Decorators)?;

        if self.accessibility.is_some() {
            match self.accessibility.as_ref().unwrap() {
                Accessibility::Public => keyword!(emitter, "public"),
                Accessibility::Protected => keyword!(emitter, "protected"),
                Accessibility::Private => keyword!(emitter, "private"),
                #[cfg(swc_ast_unknown)]
                _ => return Err(unknown_error()),
            }
            space!(emitter);
        }

        if self.is_override {
            keyword!(emitter, "override");
            space!(emitter);
        }

        if self.readonly {
            keyword!(emitter, "readonly");
            space!(emitter);
        }

        emit!(self.param);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsParamPropParam {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self {
            TsParamPropParam::Ident(n) => emit!(n),
            TsParamPropParam::Assign(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsParenthesizedType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "(");
        emit!(self.type_ann);
        punct!(emitter, ")");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsPropertySignature {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.readonly {
            keyword!(emitter, "readonly");
            space!(emitter);
        }

        if self.computed {
            punct!(emitter, "[");
            emit!(self.key);
            punct!(emitter, "]");
        } else {
            emit!(self.key);
        }

        if self.optional {
            punct!(emitter, "?");
        }

        if let Some(type_ann) = &self.type_ann {
            punct!(emitter, ":");
            formatting_space!(emitter);
            emit!(type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsQualifiedName {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.left);
        punct!(emitter, ".");
        emit!(self.right);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsRestType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "...");
        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsThisType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, self.span, "this");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsThisTypeOrIdent {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self {
            TsThisTypeOrIdent::TsThisType(n) => emit!(n),
            TsThisTypeOrIdent::Ident(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTupleType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "[");
        emitter.emit_list(
            self.span,
            Some(&self.elem_types),
            ListFormat::TupleTypeElements,
        )?;
        punct!(emitter, "]");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTupleElement {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if let Some(label) = &self.label {
            emit!(label);
            punct!(emitter, ":");
            formatting_space!(emitter);
        }

        emit!(self.ty);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsType::TsKeywordType(n) => emit!(n),
            TsType::TsThisType(n) => emit!(n),
            TsType::TsFnOrConstructorType(n) => emit!(n),
            TsType::TsTypeRef(n) => emit!(n),
            TsType::TsTypeQuery(n) => emit!(n),
            TsType::TsTypeLit(n) => emit!(n),
            TsType::TsArrayType(n) => emit!(n),
            TsType::TsTupleType(n) => emit!(n),
            TsType::TsOptionalType(n) => emit!(n),
            TsType::TsRestType(n) => emit!(n),
            TsType::TsUnionOrIntersectionType(n) => emit!(n),
            TsType::TsConditionalType(n) => emit!(n),
            TsType::TsInferType(n) => emit!(n),
            TsType::TsParenthesizedType(n) => emit!(n),
            TsType::TsTypeOperator(n) => emit!(n),
            TsType::TsIndexedAccessType(n) => emit!(n),
            TsType::TsMappedType(n) => emit!(n),
            TsType::TsLitType(n) => emit!(n),
            TsType::TsTypePredicate(n) => emit!(n),
            TsType::TsImportType(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsImportType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "import");
        punct!(emitter, "(");
        emit!(self.arg);
        if let Some(attributes) = &self.attributes {
            punct!(emitter, ",");
            formatting_space!(emitter);
            emit!(attributes);
        }
        punct!(emitter, ")");

        if let Some(n) = &self.qualifier {
            punct!(emitter, ".");
            emit!(n);
        }

        emit!(self.type_args);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsImportCallOptions {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        punct!(emitter, "{");
        if !emitter.cfg.minify {
            emitter.wr.write_line()?;
            emitter.wr.increase_indent()?;
        }

        keyword!(emitter, "with");
        punct!(emitter, ":");
        formatting_space!(emitter);
        emit!(self.with);

        if !emitter.cfg.minify {
            emitter.wr.decrease_indent()?;
            emitter.wr.write_line()?;
        }
        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeAliasDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.declare {
            keyword!(emitter, "declare");
            space!(emitter);
        }

        keyword!(emitter, "type");

        space!(emitter);

        emit!(self.id);
        if let Some(type_params) = &self.type_params {
            emit!(type_params);
        }
        formatting_space!(emitter);

        punct!(emitter, "=");

        formatting_space!(emitter);

        emit!(self.type_ann);

        formatting_semi!(emitter);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeAnn {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeAssertion {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "<");
        emit!(self.type_ann);
        punct!(emitter, ">");
        emit!(self.expr);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsConstAssertion {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.expr);

        space!(emitter);
        keyword!(emitter, "as");
        space!(emitter);
        keyword!(emitter, "const");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeElement {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsTypeElement::TsCallSignatureDecl(n) => emit!(n),
            TsTypeElement::TsConstructSignatureDecl(n) => emit!(n),
            TsTypeElement::TsPropertySignature(n) => emit!(n),
            TsTypeElement::TsMethodSignature(n) => emit!(n),
            TsTypeElement::TsIndexSignature(n) => emit!(n),
            TsTypeElement::TsGetterSignature(n) => {
                emit!(n)
            }
            TsTypeElement::TsSetterSignature(n) => {
                emit!(n)
            }
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        formatting_semi!(emitter);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsGetterSignature {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        keyword!(emitter, "get");
        space!(emitter);

        if self.computed {
            punct!(emitter, "[");
            emit!(self.key);
            punct!(emitter, "]");
        } else {
            emit!(self.key)
        }

        punct!(emitter, "(");
        punct!(emitter, ")");

        if let Some(ty) = &self.type_ann {
            punct!(emitter, ":");
            formatting_space!(emitter);

            emit!(ty.type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsSetterSignature {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        keyword!(emitter, "set");
        space!(emitter);

        if self.computed {
            punct!(emitter, "[");
            emit!(self.key);
            punct!(emitter, "]");
        } else {
            emit!(self.key)
        }

        punct!(emitter, "(");
        emit!(self.param);
        punct!(emitter, ")");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeLit {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "{");
        emitter.emit_list(
            self.span,
            Some(&self.members),
            ListFormat::MultiLineTypeLiteralMembers,
        )?;
        punct!(emitter, "}");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeOperator {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        match self.op {
            TsTypeOperatorOp::KeyOf => keyword!(emitter, "keyof"),
            TsTypeOperatorOp::Unique => keyword!(emitter, "unique"),
            TsTypeOperatorOp::ReadOnly => keyword!(emitter, "readonly"),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        space!(emitter);
        emit!(self.type_ann);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeParam {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.is_const {
            keyword!(emitter, "const");
            space!(emitter);
        }

        if self.is_in {
            keyword!(emitter, "in");
            space!(emitter);
        }

        if self.is_out {
            keyword!(emitter, "out");
            space!(emitter);
        }

        emit!(self.name);

        if let Some(constraints) = &self.constraint {
            space!(emitter);
            keyword!(emitter, "extends");
            space!(emitter);
            emit!(constraints);
        }

        if let Some(default) = &self.default {
            formatting_space!(emitter);
            punct!(emitter, "=");
            formatting_space!(emitter);
            emit!(default);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeParamDecl {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "<");

        emitter.emit_list(self.span, Some(&self.params), ListFormat::TypeParameters)?;

        punct!(emitter, ">");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeParamInstantiation {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        punct!(emitter, "<");
        emitter.emit_list(self.span, Some(&self.params), ListFormat::TypeParameters)?;

        punct!(emitter, ">");
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypePredicate {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        if self.asserts {
            keyword!(emitter, "asserts");
            space!(emitter);
        }

        emit!(self.param_name);

        if let Some(type_ann) = &self.type_ann {
            space!(emitter);
            keyword!(emitter, "is");
            space!(emitter);
            emit!(type_ann);
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeQuery {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        keyword!(emitter, "typeof");
        space!(emitter);
        emit!(self.expr_name);
        emit!(self.type_args);
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeQueryExpr {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsTypeQueryExpr::TsEntityName(n) => emit!(n),
            TsTypeQueryExpr::Import(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsTypeRef {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.type_name);

        if let Some(n) = &self.type_params {
            punct!(emitter, "<");
            emitter.emit_list(n.span, Some(&n.params), ListFormat::TypeArguments)?;
            punct!(emitter, ">");
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsUnionOrIntersectionType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        match self {
            TsUnionOrIntersectionType::TsUnionType(n) => emit!(n),
            TsUnionOrIntersectionType::TsIntersectionType(n) => emit!(n),
            #[cfg(swc_ast_unknown)]
            _ => return Err(unknown_error()),
        }
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsUnionType {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emitter.emit_list(
            self.span,
            Some(&self.types),
            ListFormat::UnionTypeConstituents,
        )?;
        Ok(())
    }
}

#[node_impl]
impl MacroNode for TsInstantiation {
    fn emit(&mut self, emitter: &mut Macro) -> Result {
        emitter.emit_leading_comments_of_span(self.span(), false)?;

        emit!(self.expr);

        emit!(self.type_args);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::tests::assert_min_typescript;

    #[test]
    fn qualified_type() {
        assert_min_typescript(
            "var memory: WebAssembly.Memory;",
            "var memory:WebAssembly.Memory",
        );
    }

    #[test]
    fn type_arg() {
        assert_min_typescript("do_stuff<T>()", "do_stuff<T>()");
    }

    #[test]
    fn no_type_arg() {
        assert_min_typescript("do_stuff()", "do_stuff()");
    }
}
