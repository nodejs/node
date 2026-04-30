use super::{
    AttributeContext, AttributeValidator, Attrs, Borrow, BoundedLifetime, Callback, CallbackParam,
    EnumDef, EnumPath, EnumVariant, Everywhere, IdentBuf, InputOnly, Lifetime, LifetimeEnv,
    LifetimeLowerer, LookupId, MaybeOwn, Method, NonOptional, OpaqueDef, OpaquePath, Optional,
    OutStructDef, OutStructField, OutStructPath, OutType, Param, ParamLifetimeLowerer, ParamSelf,
    PrimitiveType, ReturnLifetimeLowerer, ReturnType, ReturnableStructPath,
    SelfParamLifetimeLowerer, SelfType, Slice, SpecialMethod, SpecialMethodPresence, StructDef,
    StructField, StructPath, SuccessType, SymbolId, TraitDef, TraitParamSelf, TraitPath,
    TyPosition, Type, TypeDef, TypeId,
};
use crate::ast::attrs::AttrInheritContext;
use crate::{ast, Env};
use core::fmt;
use strck::IntoCk;

/// An error from lowering the AST to the HIR.
#[derive(Debug)]
#[non_exhaustive]
pub enum LoweringError {
    /// The purpose of having this is that translating to the HIR has enormous
    /// potential for really detailed error handling and giving suggestions.
    ///
    /// Unfortunately, working out what the error enum should look like to enable
    /// this is really hard. The plan is that once the lowering code is completely
    /// written, we ctrl+F for `"LoweringError::Other"` in the lowering code, and turn every
    /// instance into an specialized enum variant, generalizing where possible
    /// without losing any information.
    Other(String),
}

impl fmt::Display for LoweringError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Self::Other(ref s) => s.fmt(f),
        }
    }
}

#[derive(Default, Clone)]
pub struct ErrorContext {
    item: String,
    subitem: Option<String>,
}

impl fmt::Display for ErrorContext {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let Some(ref subitem) = self.subitem {
            write!(f, "{}::{subitem}", self.item)
        } else {
            self.item.fmt(f)
        }
    }
}

impl fmt::Debug for ErrorContext {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

/// An error store, which one can push errors to. It keeps track of the
/// current "context" for the error, usually a type or a type::method. `'tree`
/// is the AST/HIR tree it is temporarily borrowing from for the context.
#[derive(Default)]
pub struct ErrorStore<'tree> {
    /// The errors
    errors: Vec<ErrorAndContext>,
    /// The current context (types, modules)
    item: &'tree str,
    /// The current sub-item context (methods, etc)
    subitem: Option<&'tree str>,
}

pub type ErrorAndContext = (ErrorContext, LoweringError);

/// Where a type was found
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
enum TypeLoweringContext {
    Struct,
    Callback,
    Method,
}

impl<'tree> ErrorStore<'tree> {
    /// Push an error to the error store
    pub fn push(&mut self, error: LoweringError) {
        let context = ErrorContext {
            item: self.item.into(),
            subitem: self.subitem.map(|s| s.into()),
        };
        self.errors.push((context, error));
    }

    pub(super) fn take_errors(&mut self) -> Vec<ErrorAndContext> {
        core::mem::take(&mut self.errors)
    }

    pub(super) fn is_empty(&self) -> bool {
        self.errors.is_empty()
    }

    pub(super) fn set_item(&mut self, item: &'tree str) {
        self.item = item;
        self.subitem = None;
    }
    pub(super) fn set_subitem(&mut self, subitem: &'tree str) {
        self.subitem = Some(subitem);
    }
}

pub(super) struct LoweringContext<'ast> {
    pub lookup_id: LookupId<'ast>,
    pub errors: ErrorStore<'ast>,
    pub env: &'ast Env,
    pub attr_validator: Box<dyn AttributeValidator>,
    pub cfg: super::LoweringConfig,
}

/// An item and the info needed to
pub(crate) struct ItemAndInfo<'ast, Ast> {
    pub(crate) item: &'ast Ast,
    pub(crate) in_path: &'ast ast::Path,
    /// Any parent attributes resolved from the module, for a type context
    pub(crate) ty_parent_attrs: Attrs,

    /// Any parent attributes resolved from the module, for a method context
    pub(crate) method_parent_attrs: Attrs,
    pub(crate) id: SymbolId,
}

impl<'ast> LoweringContext<'ast> {
    /// Lowers an [`ast::Ident`]s into an [`hir::IdentBuf`].
    ///
    /// If there are any errors, they're pushed to `errors` and `Err` is returned.
    pub(super) fn lower_ident(
        &mut self,
        ident: &ast::Ident,
        context: &'static str,
    ) -> Result<IdentBuf, ()> {
        match ident.as_str().ck() {
            Ok(name) => Ok(name.to_owned()),
            Err(e) => {
                self.errors.push(LoweringError::Other(format!(
                    "Ident `{ident}` from {context} could not be turned into a Rust ident: {e}"
                )));
                Err(())
            }
        }
    }

    /// Lowers multiple items at once
    fn lower_all<Ast: 'static, Hir>(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, Ast>>,
        lower: impl Fn(&mut Self, ItemAndInfo<'ast, Ast>) -> Result<Hir, ()>,
    ) -> Result<Vec<Hir>, ()> {
        let mut hir_types = Ok(Vec::with_capacity(ast_defs.len()));

        for def in ast_defs {
            let hir_type = lower(self, def);

            match (hir_type, &mut hir_types) {
                (Ok(hir_type), Ok(hir_types)) => hir_types.push(hir_type),
                _ => hir_types = Err(()),
            }
        }

        hir_types
    }

    pub(super) fn lower_all_enums(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, ast::Enum>>,
    ) -> Result<Vec<EnumDef>, ()> {
        self.lower_all(ast_defs, Self::lower_enum)
    }
    pub(super) fn lower_all_structs(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, ast::Struct>>,
    ) -> Result<Vec<StructDef>, ()> {
        self.lower_all(ast_defs, Self::lower_struct)
    }
    pub(super) fn lower_all_out_structs(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, ast::Struct>>,
    ) -> Result<Vec<OutStructDef>, ()> {
        self.lower_all(ast_defs, Self::lower_out_struct)
    }
    pub(super) fn lower_all_opaques(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, ast::OpaqueType>>,
    ) -> Result<Vec<OpaqueDef>, ()> {
        self.lower_all(ast_defs, Self::lower_opaque)
    }
    pub(super) fn lower_all_traits(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, ast::Trait>>,
    ) -> Result<Vec<TraitDef>, ()> {
        self.lower_all(ast_defs, Self::lower_trait)
    }

    pub(super) fn lower_all_functions(
        &mut self,
        ast_defs: impl ExactSizeIterator<Item = ItemAndInfo<'ast, ast::Function>>,
    ) -> Result<Vec<Method>, ()> {
        self.lower_all(ast_defs, Self::lower_function)
    }

    fn lower_enum(&mut self, item: ItemAndInfo<'ast, ast::Enum>) -> Result<EnumDef, ()> {
        let ast_enum = item.item;
        self.errors.set_item(ast_enum.name.as_str());
        let name = self.lower_ident(&ast_enum.name, "enum name");
        let attrs = self.attr_validator.attr_from_ast(
            &ast_enum.attrs,
            &item.ty_parent_attrs,
            &mut self.errors,
        );

        let mut variants = Ok(Vec::with_capacity(ast_enum.variants.len()));
        let variant_parent_attrs = attrs.for_inheritance(AttrInheritContext::Variant);
        for (ident, discriminant, docs, attrs) in ast_enum.variants.iter() {
            let name = self.lower_ident(ident, "enum variant");
            let attrs =
                self.attr_validator
                    .attr_from_ast(attrs, &variant_parent_attrs, &mut self.errors);
            match (name, &mut variants) {
                (Ok(name), Ok(variants)) => {
                    let variant = EnumVariant {
                        docs: docs.clone(),
                        name,
                        discriminant: *discriminant,
                        attrs,
                    };
                    self.attr_validator.validate(
                        &variant.attrs,
                        AttributeContext::EnumVariant(&variant),
                        &mut self.errors,
                    );
                    variants.push(variant);
                }
                _ => variants = Err(()),
            }
        }

        let mut special_method_presence = SpecialMethodPresence::default();
        let methods = if attrs.disable {
            Vec::new()
        } else {
            self.lower_all_methods(
                &ast_enum.methods[..],
                item.in_path,
                &item.method_parent_attrs,
                item.id.try_into()?,
                &mut special_method_presence,
            )?
        };

        let def = EnumDef::new(
            ast_enum.docs.clone(),
            name?,
            variants?,
            methods,
            attrs,
            special_method_presence,
        );

        self.attr_validator.validate(
            &def.attrs,
            AttributeContext::Type(TypeDef::from(&def)),
            &mut self.errors,
        );

        Ok(def)
    }

    fn lower_opaque(&mut self, item: ItemAndInfo<'ast, ast::OpaqueType>) -> Result<OpaqueDef, ()> {
        let ast_opaque = item.item;
        self.errors.set_item(ast_opaque.name.as_str());
        let name = self.lower_ident(&ast_opaque.name, "opaque name");
        let dtor_abi_name = self.lower_ident(&ast_opaque.dtor_abi_name, "opaque dtor abi name");

        let attrs = self.attr_validator.attr_from_ast(
            &ast_opaque.attrs,
            &item.ty_parent_attrs,
            &mut self.errors,
        );
        let mut special_method_presence = SpecialMethodPresence::default();
        let methods = if attrs.disable {
            Vec::new()
        } else {
            self.lower_all_methods(
                &ast_opaque.methods[..],
                item.in_path,
                &item.method_parent_attrs,
                item.id.try_into()?,
                &mut special_method_presence,
            )?
        };
        let lifetimes = self.lower_type_lifetime_env(&ast_opaque.lifetimes);

        let def = OpaqueDef::new(
            ast_opaque.docs.clone(),
            name?,
            methods,
            attrs,
            lifetimes?,
            special_method_presence,
            dtor_abi_name?,
        );
        self.attr_validator.validate(
            &def.attrs,
            AttributeContext::Type(TypeDef::from(&def)),
            &mut self.errors,
        );
        Ok(def)
    }

    fn lower_struct(&mut self, item: ItemAndInfo<'ast, ast::Struct>) -> Result<StructDef, ()> {
        let ast_struct = item.item;
        self.errors.set_item(ast_struct.name.as_str());
        let struct_name = self.lower_ident(&ast_struct.name, "struct name")?;

        let mut fields = Ok(Vec::with_capacity(ast_struct.fields.len()));

        let attrs = self.attr_validator.attr_from_ast(
            &ast_struct.attrs,
            &item.ty_parent_attrs,
            &mut self.errors,
        );
        // Only compute fields if the type isn't disabled, otherwise we may encounter forbidden types
        if !attrs.disable {
            for (name, ty, docs, attrs) in ast_struct.fields.iter() {
                let name = self.lower_ident(name, "struct field name")?;
                if !ty.is_ffi_safe() {
                    let ffisafe = ty.ffi_safe_version();
                    self.errors.push(LoweringError::Other(format!(
                        "Found FFI-unsafe type {ty} in struct field {struct_name}.{name}, consider using {ffisafe}",
                    )));
                }
                let ty = self.lower_type::<Everywhere>(
                    ty,
                    &mut &ast_struct.lifetimes,
                    TypeLoweringContext::Struct,
                    item.in_path,
                );

                let field_attrs =
                    self.attr_validator
                        .attr_from_ast(attrs, &Attrs::default(), &mut self.errors);

                self.attr_validator.validate(
                    &field_attrs,
                    AttributeContext::Field,
                    &mut self.errors,
                );

                match (ty, &mut fields) {
                    (Ok(ty), Ok(fields)) => fields.push(StructField {
                        docs: docs.clone(),
                        name,
                        ty,
                        attrs: field_attrs,
                    }),
                    _ => fields = Err(()),
                }
            }
        }
        let lifetimes = self.lower_type_lifetime_env(&ast_struct.lifetimes);

        let mut special_method_presence = SpecialMethodPresence::default();
        let methods = if attrs.disable {
            Vec::new()
        } else if ast_struct.fields.is_empty() {
            if !ast_struct.methods.is_empty() {
                self.errors.push(LoweringError::Other(format!(
                    "Methods on ZST structs are not yet implemented: {}",
                    ast_struct.name
                )));
                return Err(());
            } else {
                Vec::new()
            }
        } else {
            self.lower_all_methods(
                &ast_struct.methods[..],
                item.in_path,
                &item.method_parent_attrs,
                item.id.try_into()?,
                &mut special_method_presence,
            )?
        };
        let def = StructDef::new(
            ast_struct.docs.clone(),
            struct_name,
            fields?,
            methods,
            attrs,
            lifetimes?,
            special_method_presence,
        );

        self.attr_validator.validate(
            &def.attrs,
            AttributeContext::Type(TypeDef::from(&def)),
            &mut self.errors,
        );
        Ok(def)
    }

    fn lower_trait(&mut self, item: ItemAndInfo<'ast, ast::Trait>) -> Result<TraitDef, ()> {
        let ast_trait = item.item;
        self.errors.set_item(ast_trait.name.as_str());
        let trait_name = self.lower_ident(&ast_trait.name, "trait name")?;

        let attrs = self.attr_validator.attr_from_ast(
            &ast_trait.attrs,
            &item.ty_parent_attrs,
            &mut self.errors,
        );

        if ast_trait.is_send && !self.attr_validator.attrs_supported().traits_are_send {
            self.errors.push(LoweringError::Other(
                "Traits are not safe to std::marker::Send in this backend".into(),
            ));
        }
        if ast_trait.is_sync && !self.attr_validator.attrs_supported().traits_are_sync {
            self.errors.push(LoweringError::Other(
                "Traits are not safe to std::marker::Send in this backend".into(),
            ));
        }

        let fcts = if attrs.disable {
            Vec::new()
        } else {
            let mut fcts = Vec::with_capacity(ast_trait.methods.len());
            for ast_trait_method in ast_trait.methods.iter() {
                let trait_method_attrs = self.attr_validator.attr_from_ast(
                    &ast_trait_method.attrs,
                    &attrs,
                    &mut self.errors,
                );

                if trait_method_attrs.disable {
                    continue;
                }

                fcts.push(self.lower_trait_method(ast_trait_method, item.in_path, &attrs)?);
            }
            fcts
        };
        let lifetimes = self.lower_type_lifetime_env(&ast_trait.lifetimes);
        let def = TraitDef::new(ast_trait.docs.clone(), trait_name, fcts, attrs, lifetimes?);

        self.attr_validator
            .validate(&def.attrs, AttributeContext::Trait(&def), &mut self.errors);
        Ok(def)
    }

    fn lower_trait_method(
        &mut self,
        ast_trait_method: &'ast ast::TraitMethod,
        in_path: &ast::Path,
        parent_trait_attrs: &Attrs,
    ) -> Result<Callback, ()> {
        self.errors.set_subitem(ast_trait_method.name.as_str());
        let name = ast_trait_method.name.clone();
        let self_param_ltl = SelfParamLifetimeLowerer::new(&ast_trait_method.lifetimes, self)?;
        let (param_self, mut param_ltl) =
            if let Some(self_param) = ast_trait_method.self_param.as_ref() {
                let (param_self, param_ltl) =
                    self.lower_trait_self_param(self_param, self_param_ltl, in_path)?;
                (Some(param_self), param_ltl)
            } else {
                (None, SelfParamLifetimeLowerer::no_self_ref(self_param_ltl))
            };

        let params =
            self.lower_many_callback_params(&ast_trait_method.params, &mut param_ltl, in_path)?;

        let return_type = self.lower_callback_return_type(
            ast_trait_method.output_type.as_ref(),
            &mut param_ltl,
            in_path,
        )?;

        let attrs = self.attr_validator.attr_from_ast(
            &ast_trait_method.attrs,
            parent_trait_attrs,
            &mut self.errors,
        );

        Ok(Callback {
            param_self,
            params,
            output: Box::new(return_type),
            name: Some(self.lower_ident(&name, "trait name")?),
            attrs: Some(attrs),
            docs: Some(ast_trait_method.docs.clone()),
        })
    }

    fn lower_function(
        &mut self,
        ast_function: ItemAndInfo<'ast, ast::Function>,
    ) -> Result<Method, ()> {
        self.errors.set_item(ast_function.item.name.as_str());
        let name = ast_function.item.name.clone();
        let param_ltl = SelfParamLifetimeLowerer::no_self_ref(SelfParamLifetimeLowerer::new(
            &ast_function.item.lifetimes,
            self,
        )?);

        let (ast_params, takes_write) = match ast_function.item.params.split_last() {
            Some((last, remaining)) if last.is_write() => (remaining, true),
            _ => (&ast_function.item.params[..], false),
        };

        let attrs = self.attr_validator.attr_from_ast(
            &ast_function.item.attrs,
            &ast_function.ty_parent_attrs,
            &mut self.errors,
        );

        if !attrs.disable && !self.attr_validator.attrs_supported().free_functions {
            self.errors.push(LoweringError::Other(
                format!("Could not lower public function {}, backend does not support free functions. Try #[diplomat::attr(not(supports = free_functions), disable)].", ast_function.item.name.as_str())
            ));
            return Err(());
        }

        let (params, return_type, lifetime_env) = if !attrs.disable {
            let (params, return_ltl) =
                self.lower_many_params(ast_params, param_ltl, ast_function.in_path)?;

            let (return_type, lifetime_env) = self.lower_return_type(
                ast_function.item.output_type.as_ref(),
                takes_write,
                return_ltl,
                ast_function.in_path,
            )?;
            (params, return_type, lifetime_env)
        } else {
            (
                Vec::new(),
                ReturnType::Infallible(SuccessType::Unit),
                LifetimeEnv::new(smallvec::SmallVec::new(), 0),
            )
        };

        let def = Method {
            docs: ast_function.item.docs.clone(),
            name: self.lower_ident(&name, "function name")?,
            abi_name: self.lower_ident(&ast_function.item.abi_name, "function abi name")?,
            lifetime_env,
            param_self: None,
            params,
            output: return_type,
            attrs: attrs.clone(),
        };

        self.attr_validator
            .validate(&attrs, AttributeContext::Function(&def), &mut self.errors);

        Ok(def)
    }

    fn lower_out_struct(
        &mut self,
        item: ItemAndInfo<'ast, ast::Struct>,
    ) -> Result<OutStructDef, ()> {
        let ast_out_struct = item.item;
        self.errors.set_item(ast_out_struct.name.as_str());
        let name = self.lower_ident(&ast_out_struct.name, "out-struct name");

        let attrs = self.attr_validator.attr_from_ast(
            &ast_out_struct.attrs,
            &item.ty_parent_attrs,
            &mut self.errors,
        );
        let fields = {
            let mut fields = Ok(Vec::with_capacity(ast_out_struct.fields.len()));
            // Only compute fields if the type isn't disabled, otherwise we may encounter forbidden types
            if !attrs.disable {
                for (name, ty, docs, attrs) in ast_out_struct.fields.iter() {
                    let name = self.lower_ident(name, "out-struct field name");
                    let ty = self.lower_out_type(
                        ty,
                        &mut &ast_out_struct.lifetimes,
                        item.in_path,
                        TypeLoweringContext::Struct,
                        false,
                    );

                    match (name, ty, &mut fields) {
                        (Ok(name), Ok(ty), Ok(fields)) => fields.push(OutStructField {
                            docs: docs.clone(),
                            name,
                            ty,
                            attrs: self.attr_validator.attr_from_ast(
                                attrs,
                                &Attrs::default(),
                                &mut self.errors,
                            ),
                        }),
                        _ => fields = Err(()),
                    }
                }
            }

            fields
        };
        let mut special_method_presence = SpecialMethodPresence::default();
        let methods = if attrs.disable {
            Vec::new()
        } else {
            self.lower_all_methods(
                &ast_out_struct.methods[..],
                item.in_path,
                &item.method_parent_attrs,
                item.id.try_into()?,
                &mut special_method_presence,
            )?
        };

        let lifetimes = self.lower_type_lifetime_env(&ast_out_struct.lifetimes);
        let def = OutStructDef::new(
            ast_out_struct.docs.clone(),
            name?,
            fields?,
            methods,
            attrs,
            lifetimes?,
            special_method_presence,
        );

        self.attr_validator.validate(
            &def.attrs,
            AttributeContext::Type(TypeDef::from(&def)),
            &mut self.errors,
        );
        Ok(def)
    }

    /// Lowers an [`ast::Method`]s an [`hir::Method`].
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower_method(
        &mut self,
        method: &'ast ast::Method,
        in_path: &ast::Path,
        attrs: Attrs,
        self_id: TypeId,
        special_method_presence: &mut SpecialMethodPresence,
    ) -> Result<Method, ()> {
        let name = self.lower_ident(&method.name, "method name");

        let (ast_params, takes_write) = match method.params.split_last() {
            Some((last, remaining)) if last.is_write() => (remaining, true),
            _ => (&method.params[..], false),
        };

        let self_param_ltl = SelfParamLifetimeLowerer::new(&method.lifetime_env, self)?;

        let (param_self, param_ltl) = if let Some(self_param) = method.self_param.as_ref() {
            let (param_self, param_ltl) =
                self.lower_self_param(self_param, self_param_ltl, &method.abi_name, in_path)?;
            (Some(param_self), param_ltl)
        } else {
            (None, SelfParamLifetimeLowerer::no_self_ref(self_param_ltl))
        };

        let (params, return_ltl) = self.lower_many_params(ast_params, param_ltl, in_path)?;

        let (output, lifetime_env) = self.lower_return_type(
            method.return_type.as_ref(),
            takes_write,
            return_ltl,
            in_path,
        )?;

        let abi_name = self.lower_ident(&method.abi_name, "method abi name")?;
        let hir_method = Method {
            docs: method.docs.clone(),
            name: name?,
            abi_name,
            lifetime_env,
            param_self,
            params,
            output,
            attrs,
        };

        self.attr_validator.validate(
            &hir_method.attrs,
            AttributeContext::Method(&hir_method, self_id, special_method_presence),
            &mut self.errors,
        );

        let is_comparison = matches!(
            hir_method.attrs.special_method,
            Some(SpecialMethod::Comparison)
        );
        if is_comparison && method.return_type != Some(ast::TypeName::Ordering) {
            self.errors.push(LoweringError::Other(
                "Found comparison method that does not return cmp::Ordering".into(),
            ));
            return Err(());
        }

        Ok(hir_method)
    }

    /// Lowers many [`ast::Method`]s into a vector of [`hir::Method`]s.
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower_all_methods(
        &mut self,
        ast_methods: &'ast [ast::Method],
        in_path: &ast::Path,
        method_parent_attrs: &Attrs,
        self_id: TypeId,
        special_method_presence: &mut SpecialMethodPresence,
    ) -> Result<Vec<Method>, ()> {
        let mut methods = Ok(Vec::with_capacity(ast_methods.len()));

        let mut has_unnamed_constructor = false;
        for method in ast_methods {
            self.errors.set_subitem(method.name.as_str());
            let attrs = self.attr_validator.attr_from_ast(
                &method.attrs,
                method_parent_attrs,
                &mut self.errors,
            );
            if attrs.disable {
                continue;
            }
            let method =
                self.lower_method(method, in_path, attrs, self_id, special_method_presence);
            match (method, &mut methods) {
                (Ok(method), Ok(methods)) => {
                    if matches!(
                        method.attrs.special_method,
                        Some(SpecialMethod::Constructor)
                    ) {
                        if !has_unnamed_constructor {
                            methods.push(method);
                            has_unnamed_constructor = true;
                        } else {
                            self.errors.push(LoweringError::Other(format!(
                                "At most one unnamed constructor is allowed, see https://github.com/rust-diplomat/diplomat/issues/234 if you need overloading (extra abi_name: {})",
                                method.abi_name.as_str()
                            )));
                        }
                    } else {
                        methods.push(method);
                    }
                }
                _ => methods = Err(()),
            }
        }

        methods
    }

    /// Lowers an [`ast::TypeName`]s into a [`hir::Type`] (for non-output types)
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower_type<P: TyPosition<StructPath = StructPath, OpaqueOwnership = Borrow>>(
        &mut self,
        ty: &ast::TypeName,
        ltl: &mut impl LifetimeLowerer,
        context: TypeLoweringContext,
        in_path: &ast::Path,
    ) -> Result<Type<P>, ()> {
        let mut disallow_in_callbacks = |msg: &str| {
            if context == TypeLoweringContext::Callback {
                self.errors.push(LoweringError::Other(msg.into()));
                Err(())
            } else {
                Ok(())
            }
        };
        match ty {
            ast::TypeName::Primitive(prim) => Ok(Type::Primitive(PrimitiveType::from_ast(*prim))),
            ast::TypeName::Ordering => {
                self.errors.push(LoweringError::Other("Found cmp::Ordering in parameter or struct field, it is only allowed in return types".to_string()));
                Err(())
            }
            ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => match path
                .resolve(in_path, self.env)
            {
                ast::CustomType::Struct(strct) => {
                    if strct.fields.is_empty() {
                        self.errors.push(LoweringError::Other(format!(
                            "zero-size types are not allowed as method arguments: {ty} in {path}"
                        )));
                        return Err(());
                    }
                    if let Some(tcx_id) = self.lookup_id.resolve_struct(strct) {
                        let lifetimes =
                            ltl.lower_generics(&path.lifetimes[..], &strct.lifetimes, ty.is_self());

                        Ok(Type::Struct(StructPath::new(
                            lifetimes,
                            tcx_id,
                            MaybeOwn::Own,
                        )))
                    } else if self.lookup_id.resolve_out_struct(strct).is_some() {
                        self.errors.push(LoweringError::Other(format!("found struct in input that is marked with #[diplomat::out]: {ty} in {path}")));
                        Err(())
                    } else {
                        unreachable!("struct `{}` wasn't found in the set of structs or out-structs, this is a bug.", strct.name);
                    }
                }
                ast::CustomType::Opaque(_) => {
                    self.errors.push(LoweringError::Other(format!(
                        "Opaque passed by value: {path}"
                    )));
                    Err(())
                }
                ast::CustomType::Enum(enm) => {
                    let tcx_id = self
                        .lookup_id
                        .resolve_enum(enm)
                        .expect("can't find enum in lookup map, which contains all enums from env");

                    Ok(Type::Enum(EnumPath::new(tcx_id)))
                }
            },
            ast::TypeName::ImplTrait(path) => {
                if !self.attr_validator.attrs_supported().traits {
                    self.errors.push(LoweringError::Other(
                        "Traits are not supported by this backend".into(),
                    ));
                }
                let trt = path.resolve_trait(in_path, self.env);
                let tcx_id = self
                    .lookup_id
                    .resolve_trait(&trt)
                    .expect("can't find trait in lookup map, which contains all traits from env");
                let lifetimes =
                    ltl.lower_generics(&path.lifetimes[..], &trt.lifetimes, ty.is_self());

                Ok(Type::ImplTrait(P::build_trait_path(TraitPath::new(
                    lifetimes, tcx_id,
                ))))
            }
            ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, self.env) {
                        ast::CustomType::Opaque(opaque) => {
                            let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                            let lifetimes = ltl.lower_generics(
                                &path.lifetimes[..],
                                &opaque.lifetimes,
                                ref_ty.is_self(),
                            );
                            let tcx_id = self.lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                            Ok(Type::Opaque(OpaquePath::new(
                                lifetimes,
                                Optional(false),
                                borrow,
                                tcx_id,
                            )))
                        }
                        ast::CustomType::Struct(st) => {
                            disallow_in_callbacks(
                                "Cannot return references to structs from callbacks",
                            )?;
                            if self.attr_validator.attrs_supported().struct_refs {
                                let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                                let lifetimes = ltl.lower_generics(
                                    &path.lifetimes[..],
                                    &st.lifetimes,
                                    ref_ty.is_self(),
                                );
                                let tcx_id = self.lookup_id.resolve_struct(st).expect(
                                    "Can't find struct in lookup map, which contains all structs from env"
                                );
                                Ok(Type::Struct(StructPath::new(
                                    lifetimes,
                                    tcx_id,
                                    MaybeOwn::Borrow(borrow),
                                )))
                            } else {
                                self.errors.push(LoweringError::Other("found &T in input where T is a struct. The backend must support struct_refs.".to_string()));
                                Err(())
                            }
                        }
                        _ => {
                            self.errors.push(LoweringError::Other(format!("found &T in input where T is a custom type, but not opaque. T = {ref_ty}")));
                            Err(())
                        }
                    }
                }
                _ => {
                    self.errors.push(LoweringError::Other(format!("found &T in input where T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                    Err(())
                }
            },
            ast::TypeName::Box(box_ty) => {
                self.errors.push(match box_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, self.env) {
                        ast::CustomType::Opaque(_) => LoweringError::Other(format!("found Box<T> in input where T is an opaque, but owned opaques aren't allowed in inputs. try &T instead? T = {path}")),
                        _ => LoweringError::Other(format!("found Box<T> in input where T is a custom type but not opaque. non-opaques can't be behind pointers, and opaques in inputs can't be owned. T = {path}")),
                    }
                }
                _ => LoweringError::Other(format!("found Box<T> in input where T isn't a custom type. T = {box_ty}")),
            });
                Err(())
            }
            ast::TypeName::Option(opt_ty, stdlib) => {
                match opt_ty.as_ref() {
                    ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref()
                    {
                        ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => match path
                            .resolve(in_path, self.env)
                        {
                            ast::CustomType::Opaque(opaque) => {
                                if *stdlib == ast::StdlibOrDiplomat::Diplomat {
                                    self.errors.push(LoweringError::Other("found DiplomatOption<&T>, please use Option<&T> (DiplomatOption is for primitives, structs, and enums)".to_string()));
                                    return Err(());
                                }
                                let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                                let lifetimes = ltl.lower_generics(
                                    &path.lifetimes,
                                    &opaque.lifetimes,
                                    ref_ty.is_self(),
                                );
                                let tcx_id = self.lookup_id.resolve_opaque(opaque).expect(
                                    "can't find opaque in lookup map, which contains all opaques from env",
                                );

                                Ok(Type::Opaque(OpaquePath::new(
                                    lifetimes,
                                    Optional(true),
                                    borrow,
                                    tcx_id,
                                )))
                            }
                            _ => {
                                self.errors.push(LoweringError::Other(format!("found Option<&T> in input where T is a custom type, but it's not opaque. T = {ref_ty}")));
                                Err(())
                            }
                        },
                        _ => {
                            self.errors.push(LoweringError::Other(format!("found Option<&T> in input, but T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                            Err(())
                        }
                    },
                    ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                        match path.resolve(in_path, self.env) {
                            ast::CustomType::Opaque(_) => {
                                self.errors.push(LoweringError::Other("Found Option<T> where T is opaque, opaque types must be behind a reference".into()));
                                Err(())
                            }
                            _ => {
                                if context == TypeLoweringContext::Struct
                                    && *stdlib == ast::StdlibOrDiplomat::Stdlib
                                {
                                    self.errors.push(LoweringError::Other("Found Option<T> for struct/enum T in a struct field, please use DiplomatOption<T>".into()));
                                    return Err(());
                                }
                                if !self.attr_validator.attrs_supported().option {
                                    self.errors.push(LoweringError::Other("Options of structs/enums/primitives not supported by this backend".into()));
                                }
                                let inner = self.lower_type(opt_ty, ltl, context, in_path)?;
                                Ok(Type::DiplomatOption(Box::new(inner)))
                            }
                        }
                    }
                    ast::TypeName::Primitive(prim) => {
                        if context == TypeLoweringContext::Struct
                            && *stdlib == ast::StdlibOrDiplomat::Stdlib
                        {
                            self.errors.push(LoweringError::Other("Found Option<T> for primitive T in a struct field, please use DiplomatOption<T>".into()));
                            return Err(());
                        }
                        if !self.attr_validator.attrs_supported().option {
                            self.errors.push(LoweringError::Other(
                                "Options of structs/enums/primitives not supported by this backend"
                                    .into(),
                            ));
                        }
                        Ok(Type::DiplomatOption(Box::new(Type::Primitive(
                            PrimitiveType::from_ast(*prim),
                        ))))
                    }
                    ast::TypeName::StrSlice(encoding, _stdlib) => Ok(Type::DiplomatOption(
                        Box::new(Type::Slice(Slice::Strs(*encoding))),
                    )),
                    ast::TypeName::StrReference(..) | ast::TypeName::PrimitiveSlice(..) => {
                        let inner = self.lower_type(opt_ty, ltl, context, in_path)?;
                        Ok(Type::DiplomatOption(Box::new(inner)))
                    }
                    ast::TypeName::Box(box_ty) => {
                        // we could see whats in the box here too
                        self.errors.push(LoweringError::Other(format!("found Option<Box<T>> in input, but box isn't allowed in inputs. T = {box_ty}")));
                        Err(())
                    }
                    _ => {
                        self.errors.push(LoweringError::Other(format!("found Option<T> in input, where T isn't a reference but Option<T> in inputs requires that T is a reference to an opaque. T = {opt_ty}")));
                        Err(())
                    }
                }
            }
            ast::TypeName::Result(_, _, _) => {
                self.errors.push(LoweringError::Other(
                    "Results can only appear as the top-level return type of methods".into(),
                ));
                Err(())
            }
            ast::TypeName::Write => {
                self.errors.push(LoweringError::Other(
                    "DiplomatWrite can only appear as the last parameter of a method".into(),
                ));
                Err(())
            }
            ast::TypeName::StrReference(lifetime, encoding, _stdlib) => {
                if lifetime.is_none() {
                    disallow_in_callbacks("Cannot return owned slices from callbacks")?;
                }
                let new_lifetime = lifetime.as_ref().map(|lt| ltl.lower_lifetime(lt));
                if let Some(super::MaybeStatic::Static) = new_lifetime {
                    if !self.attr_validator.attrs_supported().static_slices {
                        self.errors.push(LoweringError::Other(
                            "'static string slice types are not supported. Try #[diplomat::attr(not(supports = static_slices), disable)]".into()
                        ));
                    }
                }
                Ok(Type::Slice(Slice::Str(new_lifetime, *encoding)))
            }
            ast::TypeName::StrSlice(encoding, _stdlib) => Ok(Type::Slice(Slice::Strs(*encoding))),
            ast::TypeName::PrimitiveSlice(lm, prim, _stdlib) => {
                if lm.is_none() {
                    disallow_in_callbacks("Cannot return owned slices from callbacks")?;
                }
                let new_lifetime = lm
                    .as_ref()
                    .map(|(lt, m)| Borrow::new(ltl.lower_lifetime(lt), *m));

                if let Some(b) = new_lifetime {
                    if let super::MaybeStatic::Static = b.lifetime {
                        if !self.attr_validator.attrs_supported().static_slices {
                            self.errors.push(LoweringError::Other(
                                format!("'static {prim:?} slice types not supported. Try #[diplomat::attr(not(supports = static_slices), disable)]")
                            ));
                        }
                    }
                }

                Ok(Type::Slice(Slice::Primitive(
                    new_lifetime.into(),
                    PrimitiveType::from_ast(*prim),
                )))
            }
            ast::TypeName::CustomTypeSlice(lm, type_name) => {
                match type_name.as_ref() {
                    ast::TypeName::Named(path) => match path.resolve(in_path, self.env) {
                        ast::CustomType::Struct(..) => {
                            if !self.attr_validator.attrs_supported().abi_compatibles {
                                self.errors.push(LoweringError::Other(
                                    "Primitive struct slices are not supported by this backend"
                                        .into(),
                                ));
                            }
                        }
                        _ => self.errors.push(LoweringError::Other(format!(
                            "{type_name} slices are not supported."
                        ))),
                    },
                    _ => self.errors.push(LoweringError::Other(format!(
                        "{type_name} slices are not supported."
                    ))),
                }

                let new_lifetime = lm
                    .as_ref()
                    .map(|(lt, m)| Borrow::new(ltl.lower_lifetime(lt), *m));

                if let Some(b) = new_lifetime {
                    if let super::MaybeStatic::Static = b.lifetime {
                        if !self.attr_validator.attrs_supported().static_slices {
                            self.errors.push(LoweringError::Other(
                                format!("'static {type_name:?} slice types not supported. Try #[diplomat::attr(not(supports = static_slices), disable)]")
                            ));
                        }
                    }
                }

                match type_name.as_ref() {
                    ast::TypeName::Named(path) => match path.resolve(in_path, self.env) {
                        ast::CustomType::Struct(..) => {
                            let inner = self.lower_type::<P>(type_name, ltl, context, in_path)?;
                            match inner {
                                Type::Struct(st) => {
                                    Ok(Type::Slice(Slice::Struct(new_lifetime.into(), st)))
                                }
                                _ => unreachable!(),
                            }
                        }
                        _ => {
                            self.errors.push(LoweringError::Other(
                                    format!("Cannot have custom type {type_name} in a slice. Custom slices can only contain primitive-only structs.")
                                ));
                            Err(())
                        }
                    },
                    _ => {
                        self.errors.push(LoweringError::Other(format!(
                            "Cannot make a slice from type {type_name}"
                        )));
                        Err(())
                    }
                }
            }
            ast::TypeName::Function(input_types, out_type, _mutability) => {
                disallow_in_callbacks("Cannot nest callbacks")?;
                if !self.attr_validator.attrs_supported().callbacks {
                    self.errors.push(LoweringError::Other(
                        "Callback arguments are not supported by this backend".into(),
                    ));
                }
                if context == TypeLoweringContext::Struct {
                    self.errors.push(LoweringError::Other(
                        "Callbacks currently unsupported in structs".into(),
                    ));
                    return Err(());
                }
                let mut params: Vec<CallbackParam> = Vec::new();
                for in_ty in input_types.iter() {
                    let param =
                        self.lower_callback_param(/* anonymous */ None, in_ty, ltl, in_path)?;

                    params.push(param)
                }

                Ok(Type::Callback(P::build_callback(Callback {
                    param_self: None,
                    params,
                    output: Box::new(self.lower_callback_return_type(
                        Some(out_type),
                        ltl,
                        in_path,
                    )?),
                    name: None,
                    attrs: None,
                    docs: None,
                })))
            }
            ast::TypeName::Unit => {
                self.errors.push(LoweringError::Other("Unit types can only appear as the return value of a method, or as the Ok/Err variants of a returned result".into()));
                Err(())
            }
        }
    }

    /// Lowers an [`ast::TypeName`]s into an [`hir::OutType`].
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower_out_type(
        &mut self,
        ty: &ast::TypeName,
        ltl: &mut impl LifetimeLowerer,
        in_path: &ast::Path,
        context: TypeLoweringContext,
        in_result_option: bool,
    ) -> Result<OutType, ()> {
        match ty {
            ast::TypeName::Primitive(prim) => {
                Ok(OutType::Primitive(PrimitiveType::from_ast(*prim)))
            }
            ast::TypeName::Ordering => {
                if context == TypeLoweringContext::Struct {
                    self.errors.push(LoweringError::Other(
                        "Found cmp::Ordering in struct field, it is only allowed in return types"
                            .to_string(),
                    ));
                    Err(())
                } else {
                    Ok(Type::Primitive(PrimitiveType::Ordering))
                }
            }
            ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                match path.resolve(in_path, self.env) {
                    ast::CustomType::Struct(strct) => {
                        if !in_result_option && strct.fields.is_empty() {
                            self.errors.push(LoweringError::Other(format!("Found zero-size struct outside a `Result` or `Option`: {ty} in {in_path}")));
                            return Err(());
                        }
                        let lifetimes =
                            ltl.lower_generics(&path.lifetimes, &strct.lifetimes, ty.is_self());

                        if let Some(tcx_id) = self.lookup_id.resolve_struct(strct) {
                            Ok(OutType::Struct(ReturnableStructPath::Struct(
                                StructPath::new(lifetimes, tcx_id, MaybeOwn::Own),
                            )))
                        } else if let Some(tcx_id) = self.lookup_id.resolve_out_struct(strct) {
                            Ok(OutType::Struct(ReturnableStructPath::OutStruct(
                                OutStructPath::new(lifetimes, tcx_id, MaybeOwn::Own),
                            )))
                        } else {
                            unreachable!("struct `{}` wasn't found in the set of structs or out-structs, this is a bug.", strct.name);
                        }
                    }
                    ast::CustomType::Opaque(_) => {
                        self.errors.push(LoweringError::Other(format!(
                            "Opaque passed by value in input: {path}"
                        )));
                        Err(())
                    }
                    ast::CustomType::Enum(enm) => {
                        let tcx_id = self.lookup_id.resolve_enum(enm).expect(
                            "can't find enum in lookup map, which contains all enums from env",
                        );

                        Ok(OutType::Enum(EnumPath::new(tcx_id)))
                    }
                }
            }
            ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, self.env) {
                        ast::CustomType::Opaque(opaque) => {
                            let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                            let lifetimes = ltl.lower_generics(
                                &path.lifetimes,
                                &opaque.lifetimes,
                                ref_ty.is_self(),
                            );
                            let tcx_id = self.lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                            Ok(OutType::Opaque(OpaquePath::new(
                                lifetimes,
                                Optional(false),
                                MaybeOwn::Borrow(borrow),
                                tcx_id,
                            )))
                        }
                        _ => {
                            self.errors.push(LoweringError::Other(format!("found &T in output where T is a custom type, but not opaque. T = {ref_ty}")));
                            Err(())
                        }
                    }
                }
                _ => {
                    self.errors.push(LoweringError::Other(format!("found &T in output where T isn't a custom type and therefore not opaque. T = {ref_ty}, path = {in_path:?}")));
                    Err(())
                }
            },
            ast::TypeName::Box(box_ty) => match box_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, self.env) {
                        ast::CustomType::Opaque(opaque) => {
                            let lifetimes = ltl.lower_generics(
                                &path.lifetimes,
                                &opaque.lifetimes,
                                box_ty.is_self(),
                            );
                            let tcx_id = self.lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                            Ok(OutType::Opaque(OpaquePath::new(
                                lifetimes,
                                Optional(false),
                                MaybeOwn::Own,
                                tcx_id,
                            )))
                        }
                        _ => {
                            self.errors.push(LoweringError::Other(format!("found Box<T> in output where T is a custom type but not opaque. non-opaques can't be behind pointers. T = {path}")));
                            Err(())
                        }
                    }
                }
                _ => {
                    self.errors.push(LoweringError::Other(format!(
                        "found Box<T> in output where T isn't a custom type. T = {box_ty}"
                    )));
                    Err(())
                }
            },
            ast::TypeName::Option(opt_ty, stdlib) => match opt_ty.as_ref() {
                ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
                    ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                        match path.resolve(in_path, self.env) {
                            ast::CustomType::Opaque(opaque) => {
                                if *stdlib == ast::StdlibOrDiplomat::Diplomat {
                                    self.errors.push(LoweringError::Other("found DiplomatOption<&T>, please use Option<&T> (DiplomatOption is for primitives, structs, and enums)".to_string()));
                                    return Err(());
                                }
                                let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                                let lifetimes = ltl.lower_generics(
                                    &path.lifetimes,
                                    &opaque.lifetimes,
                                    ref_ty.is_self(),
                                );
                                let tcx_id = self.lookup_id.resolve_opaque(opaque).expect(
                                "can't find opaque in lookup map, which contains all opaques from env",
                            );

                                Ok(OutType::Opaque(OpaquePath::new(
                                    lifetimes,
                                    Optional(true),
                                    MaybeOwn::Borrow(borrow),
                                    tcx_id,
                                )))
                            }
                            _ => {
                                self.errors.push(LoweringError::Other(format!("found Option<&T> where T is a custom type, but it's not opaque. T = {ref_ty}")));
                                Err(())
                            }
                        }
                    }
                    _ => {
                        self.errors.push(LoweringError::Other(format!("found Option<&T>, but T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                        Err(())
                    }
                },
                ast::TypeName::Box(box_ty) => match box_ty.as_ref() {
                    ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                        match path.resolve(in_path, self.env) {
                            ast::CustomType::Opaque(opaque) => {
                                if *stdlib == ast::StdlibOrDiplomat::Diplomat {
                                    self.errors.push(LoweringError::Other("found DiplomatOption<Box<T>>, please use Option<Box<T>> (DiplomatOption is for primitives, structs, and enums)".to_string()));
                                    return Err(());
                                }
                                let lifetimes = ltl.lower_generics(
                                    &path.lifetimes,
                                    &opaque.lifetimes,
                                    box_ty.is_self(),
                                );
                                let tcx_id = self.lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                                Ok(OutType::Opaque(OpaquePath::new(
                                    lifetimes,
                                    Optional(true),
                                    MaybeOwn::Own,
                                    tcx_id,
                                )))
                            }
                            _ => {
                                self.errors.push(LoweringError::Other(format!("found Option<Box<T>> where T is a custom type, but it's not opaque. T = {box_ty}")));
                                Err(())
                            }
                        }
                    }
                    _ => {
                        self.errors.push(LoweringError::Other(format!("found Option<Box<T>>, but T isn't a custom type and therefore not opaque. T = {box_ty}")));
                        Err(())
                    }
                },
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, self.env) {
                        ast::CustomType::Opaque(_) => {
                            self.errors.push(LoweringError::Other("Found Option<T> where T is opaque, opaque types must be behind a reference".into()));
                            Err(())
                        }
                        _ => {
                            if context == TypeLoweringContext::Struct
                                && *stdlib == ast::StdlibOrDiplomat::Stdlib
                            {
                                self.errors.push(LoweringError::Other("Found Option<T> for struct/enum T in a struct field, please use DiplomatOption<T>".into()));
                                return Err(());
                            }
                            if !self.attr_validator.attrs_supported().option {
                                self.errors.push(LoweringError::Other("Options of structs/enums/primitives not supported by this backend".into()));
                            }
                            let inner = self.lower_out_type(opt_ty, ltl, in_path, context, true)?;
                            Ok(Type::DiplomatOption(Box::new(inner)))
                        }
                    }
                }
                ast::TypeName::Primitive(prim) => {
                    if context == TypeLoweringContext::Struct
                        && *stdlib == ast::StdlibOrDiplomat::Stdlib
                    {
                        self.errors.push(LoweringError::Other("Found Option<T> for primitive T in a struct field, please use DiplomatOption<T>".into()));
                        return Err(());
                    }
                    if !self.attr_validator.attrs_supported().option {
                        self.errors.push(LoweringError::Other(
                            "Options of structs/enums/primitives not supported by this backend"
                                .into(),
                        ));
                    }
                    Ok(Type::DiplomatOption(Box::new(Type::Primitive(
                        PrimitiveType::from_ast(*prim),
                    ))))
                }
                _ => {
                    self.errors.push(LoweringError::Other(format!("found Option<T>, where T isn't a reference but Option<T> requires that T is a reference to an opaque. T = {opt_ty}")));
                    Err(())
                }
            },
            ast::TypeName::Result(_, _, _) => {
                self.errors.push(LoweringError::Other(
                    "Results can only appear as the top-level return type of methods".into(),
                ));
                Err(())
            }
            ast::TypeName::Write => {
                self.errors.push(LoweringError::Other(
                    "DiplomatWrite can only appear as the last parameter of a method".into(),
                ));
                Err(())
            }
            ast::TypeName::PrimitiveSlice(None, _, _stdlib)
            | ast::TypeName::StrReference(None, _, _stdlib) => {
                self.errors.push(LoweringError::Other(
                    "Owned slices cannot be returned".into(),
                ));
                Err(())
            }
            ast::TypeName::StrReference(Some(l), encoding, _stdlib) => Ok(OutType::Slice(
                Slice::Str(Some(ltl.lower_lifetime(l)), *encoding),
            )),
            ast::TypeName::StrSlice(.., _stdlib) => {
                self.errors.push(LoweringError::Other(
                    "String slices can only be an input type".into(),
                ));
                Err(())
            }
            ast::TypeName::PrimitiveSlice(Some((lt, m)), prim, _stdlib) => {
                Ok(OutType::Slice(Slice::Primitive(
                    MaybeOwn::Borrow(Borrow::new(ltl.lower_lifetime(lt), *m)),
                    PrimitiveType::from_ast(*prim),
                )))
            }
            ast::TypeName::CustomTypeSlice(ltmt, type_name) => {
                let new_lifetime = ltmt
                    .as_ref()
                    .map(|(lt, m)| Borrow::new(ltl.lower_lifetime(lt), *m));

                if let Some(b) = new_lifetime {
                    if let super::MaybeStatic::Static = b.lifetime {
                        if !self.attr_validator.attrs_supported().static_slices {
                            self.errors.push(LoweringError::Other(
                                format!("'static {type_name:?} slice types not supported. Try #[diplomat::attr(not(supports = static_slices), disable)]")
                            ));
                        }
                    }
                }

                match &type_name.as_ref() {
                    ast::TypeName::Named(path) => match path.resolve(in_path, self.env) {
                        ast::CustomType::Struct(..) => {
                            let inner = self.lower_out_type(
                                type_name,
                                ltl,
                                in_path,
                                context,
                                in_result_option,
                            )?;
                            match inner {
                                Type::Struct(st) => {
                                    Ok(Type::Slice(Slice::Struct(new_lifetime.into(), st)))
                                }
                                _ => unreachable!(),
                            }
                        }
                        _ => {
                            self.errors.push(LoweringError::Other(
                                    format!("Cannot have custom type {type_name} in a slice. Custom slices can only contain primitive-only structs.")
                                ));
                            Err(())
                        }
                    },
                    _ => {
                        self.errors.push(LoweringError::Other(format!(
                            "Cannot make a slice from type {type_name}"
                        )));
                        Err(())
                    }
                }
            }
            ast::TypeName::Unit => {
                self.errors.push(LoweringError::Other("Unit types can only appear as the return value of a method, or as the Ok/Err variants of a returned result".into()));
                Err(())
            }
            ast::TypeName::Function(..) => {
                self.errors.push(LoweringError::Other(
                    "Function types can only be an input type".into(),
                ));
                Err(())
            }
            ast::TypeName::ImplTrait(_) => {
                self.errors.push(LoweringError::Other(
                    "Trait impls can only be an input type".into(),
                ));
                Err(())
            }
        }
    }

    /// Lowers an [`ast::SelfParam`] into an [`hir::ParamSelf`].
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower_self_param(
        &mut self,
        self_param: &ast::SelfParam,
        self_param_ltl: SelfParamLifetimeLowerer<'ast>,
        method_full_path: &ast::Ident, // for better error msg
        in_path: &ast::Path,
    ) -> Result<(ParamSelf, ParamLifetimeLowerer<'ast>), ()> {
        match self_param.path_type.resolve(in_path, self.env) {
            ast::CustomType::Struct(strct) => {
                if let Some(tcx_id) = self.lookup_id.resolve_struct(strct) {
                    let (borrow, mut param_ltl) = if let Some((lt, mt)) = &self_param.reference {
                        if self.attr_validator.attrs_supported().struct_refs {
                            let (borrow_lt, param_ltl) = self_param_ltl.lower_self_ref(lt);
                            let borrow = Borrow::new(borrow_lt, *mt);

                            (MaybeOwn::Borrow(borrow), param_ltl)
                        } else {
                            self.errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes a reference to a struct as a self parameter, which isn't allowed. Backend must support struct_refs.")));
                            return Err(());
                        }
                    } else {
                        (MaybeOwn::Own, self_param_ltl.no_self_ref())
                    };

                    let attrs = self.attr_validator.attr_from_ast(
                        &self_param.attrs,
                        &Attrs::default(),
                        &mut self.errors,
                    );

                    // Even if we explicitly write out the type of `self` like
                    // `self: Foo<'a>`, the `'a` is still not considered for
                    // elision according to rustc, so is_self=true.
                    let type_lifetimes = param_ltl.lower_generics(
                        &self_param.path_type.lifetimes[..],
                        &strct.lifetimes,
                        true,
                    );

                    self.attr_validator.validate(
                        &attrs,
                        AttributeContext::SelfParam,
                        &mut self.errors,
                    );

                    Ok((
                        ParamSelf::new(
                            SelfType::Struct(StructPath::new(type_lifetimes, tcx_id, borrow)),
                            attrs,
                        ),
                        param_ltl,
                    ))
                } else if self.lookup_id.resolve_out_struct(strct).is_some() {
                    if let Some((lifetime, _)) = &self_param.reference {
                        self.errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes an out-struct as the self parameter, which isn't allowed. Also, it's behind a reference, `{lifetime}`, but only opaques can be behind references")));
                        Err(())
                    } else {
                        self.errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes an out-struct as the self parameter, which isn't allowed")));
                        Err(())
                    }
                } else {
                    unreachable!(
                    "struct `{}` wasn't found in the set of structs or out-structs, this is a bug.",
                    strct.name
                );
                }
            }
            ast::CustomType::Opaque(opaque) => {
                let tcx_id = self
                    .lookup_id
                    .resolve_opaque(opaque)
                    .expect("opaque is in env");

                if let Some((lifetime, mutability)) = &self_param.reference {
                    let (borrow_lifetime, mut param_ltl) = self_param_ltl.lower_self_ref(lifetime);
                    let borrow = Borrow::new(borrow_lifetime, *mutability);
                    let lifetimes = param_ltl.lower_generics(
                        &self_param.path_type.lifetimes,
                        &opaque.lifetimes,
                        true,
                    );

                    let attrs = self.attr_validator.attr_from_ast(
                        &self_param.attrs,
                        &Attrs::default(),
                        &mut self.errors,
                    );

                    self.attr_validator.validate(
                        &attrs,
                        AttributeContext::SelfParam,
                        &mut self.errors,
                    );

                    Ok((
                        ParamSelf::new(
                            SelfType::Opaque(OpaquePath::new(
                                lifetimes,
                                NonOptional,
                                borrow,
                                tcx_id,
                            )),
                            attrs,
                        ),
                        param_ltl,
                    ))
                } else {
                    self.errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes an opaque by value as the self parameter, but opaques as inputs must be behind refs")));
                    Err(())
                }
            }
            ast::CustomType::Enum(enm) => {
                let tcx_id = self.lookup_id.resolve_enum(enm).expect("enum is in env");

                let attrs = self.attr_validator.attr_from_ast(
                    &self_param.attrs,
                    &Attrs::default(),
                    &mut self.errors,
                );

                self.attr_validator
                    .validate(&attrs, AttributeContext::SelfParam, &mut self.errors);

                Ok((
                    ParamSelf::new(SelfType::Enum(EnumPath::new(tcx_id)), attrs),
                    self_param_ltl.no_self_ref(),
                ))
            }
        }
    }

    fn lower_trait_self_param(
        &mut self,
        self_param: &ast::TraitSelfParam,
        self_param_ltl: SelfParamLifetimeLowerer<'ast>,
        in_path: &ast::Path,
    ) -> Result<(TraitParamSelf, ParamLifetimeLowerer<'ast>), ()> {
        let trt = self_param.path_trait.resolve_trait(in_path, self.env);
        if let Some(tcx_id) = self.lookup_id.resolve_trait(&trt) {
            // check this -- I think we should be able to have both self and non-self
            if let Some((lifetime, _)) = &self_param.reference {
                let (_, mut param_ltl) = self_param_ltl.lower_self_ref(lifetime);
                let lifetimes = param_ltl.lower_generics(
                    &self_param.path_trait.lifetimes,
                    &trt.lifetimes,
                    true,
                );

                Ok((
                    TraitParamSelf::new(TraitPath::new(lifetimes, tcx_id)),
                    param_ltl,
                ))
            } else {
                let mut param_ltl = self_param_ltl.no_self_ref();

                let type_lifetimes = param_ltl.lower_generics(
                    &self_param.path_trait.lifetimes[..],
                    &trt.lifetimes,
                    true,
                );

                Ok((
                    TraitParamSelf::new(TraitPath::new(type_lifetimes, tcx_id)),
                    param_ltl,
                ))
            }
        } else {
            unreachable!(
                "Trait `{}` wasn't found in the set of traits; this is a bug.",
                trt.name
            );
        }
    }

    /// Lowers an [`ast::Param`] into an [`hir::Param`].
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    ///
    /// Note that this expects that if there was a DiplomatWrite param at the end in
    /// the method, it's not passed into here.
    fn lower_param(
        &mut self,
        param: &ast::Param,
        ltl: &mut impl LifetimeLowerer,
        in_path: &ast::Path,
    ) -> Result<Param, ()> {
        let name = self.lower_ident(&param.name, "param name");
        let ty = self.lower_type::<InputOnly>(&param.ty, ltl, TypeLoweringContext::Method, in_path);

        // No parent attrs because parameters do not have a strictly clear parent.
        let attrs =
            self.attr_validator
                .attr_from_ast(&param.attrs, &Attrs::default(), &mut self.errors);

        self.attr_validator
            .validate(&attrs, AttributeContext::Param, &mut self.errors);

        Ok(Param::new(name?, ty?, attrs))
    }

    /// Lowers many [`ast::Param`]s into a vector of [`hir::Param`]s.
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    ///
    /// Note that this expects that if there was a DiplomatWrite param at the end in
    /// the method, `ast_params` was sliced to not include it. This happens in
    /// `self.lower_method`, the caller of this function.
    fn lower_many_params(
        &mut self,
        ast_params: &[ast::Param],
        mut param_ltl: ParamLifetimeLowerer<'ast>,
        in_path: &ast::Path,
    ) -> Result<(Vec<Param>, ReturnLifetimeLowerer<'ast>), ()> {
        let mut params = Ok(Vec::with_capacity(ast_params.len()));

        for param in ast_params {
            let param = self.lower_param(param, &mut param_ltl, in_path);

            match (param, &mut params) {
                (Ok(param), Ok(params)) => {
                    params.push(param);
                }
                _ => params = Err(()),
            }
        }

        Ok((params?, param_ltl.into_return_ltl()))
    }

    fn lower_callback_param(
        &mut self,
        name: Option<IdentBuf>,
        ty: &ast::TypeName,
        ltl: &mut impl LifetimeLowerer,
        in_path: &ast::Path,
    ) -> Result<CallbackParam, ()> {
        let ty = self.lower_out_type(
            ty,
            ltl,
            in_path,
            TypeLoweringContext::Callback,
            false, /* in_result_option */
        )?;

        if !self.cfg.unsafe_references_in_callbacks
            && ty
                .lifetimes()
                .any(|lt| matches!(lt, super::MaybeStatic::NonStatic(..)))
        {
            // Slices are copied in non-memory sharing backends
            if !matches!(ty, Type::Slice(_)) {
                self.errors.push(LoweringError::Other("Callbacks cannot take references since they can be unsafely persisted, set `unsafe_references_in_callbacks` config to override.".into() ));
            }
        }

        Ok(CallbackParam { name, ty })
    }

    fn lower_many_callback_params(
        &mut self,
        ast_params: &[ast::Param],
        param_ltl: &mut ParamLifetimeLowerer<'ast>,
        in_path: &ast::Path,
    ) -> Result<Vec<CallbackParam>, ()> {
        let mut params = Ok(Vec::with_capacity(ast_params.len()));

        for param in ast_params {
            let name = self.lower_ident(&param.name, "param name")?;
            let param = self.lower_callback_param(Some(name), &param.ty, param_ltl, in_path);

            match (param, &mut params) {
                (Ok(param), Ok(params)) => {
                    params.push(param);
                }
                _ => params = Err(()),
            }
        }
        params
    }

    /// Lowers the return type of an [`ast::Method`] into a [`hir::ReturnFallability`].
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower_return_type(
        &mut self,
        return_type: Option<&ast::TypeName>,
        takes_write: bool,
        mut return_ltl: ReturnLifetimeLowerer<'_>,
        in_path: &ast::Path,
    ) -> Result<(ReturnType, LifetimeEnv), ()> {
        let write_or_unit = if takes_write {
            SuccessType::Write
        } else {
            SuccessType::Unit
        };
        match return_type.unwrap_or(&ast::TypeName::Unit) {
            ast::TypeName::Result(ok_ty, err_ty, _) => {
                let ok_ty = match ok_ty.as_ref() {
                    ast::TypeName::Unit => Ok(write_or_unit),
                    ty => self
                        .lower_out_type(
                            ty,
                            &mut return_ltl,
                            in_path,
                            TypeLoweringContext::Method,
                            true,
                        )
                        .map(SuccessType::OutType),
                };
                let err_ty = match err_ty.as_ref() {
                    ast::TypeName::Unit => Ok(None),
                    ty => self
                        .lower_out_type(
                            ty,
                            &mut return_ltl,
                            in_path,
                            TypeLoweringContext::Method,
                            true,
                        )
                        .map(Some),
                };

                match (ok_ty, err_ty) {
                    (Ok(ok_ty), Ok(err_ty)) => Ok(ReturnType::Fallible(ok_ty, err_ty)),
                    _ => Err(()),
                }
            }
            ty @ ast::TypeName::Option(value_ty, _stdlib) => match &**value_ty {
                ast::TypeName::Box(..) | ast::TypeName::Reference(..) => self
                    .lower_out_type(
                        ty,
                        &mut return_ltl,
                        in_path,
                        TypeLoweringContext::Method,
                        true,
                    )
                    .map(SuccessType::OutType)
                    .map(ReturnType::Infallible),
                ast::TypeName::Unit => Ok(ReturnType::Nullable(write_or_unit)),
                _ => self
                    .lower_out_type(
                        value_ty,
                        &mut return_ltl,
                        in_path,
                        TypeLoweringContext::Method,
                        true,
                    )
                    .map(SuccessType::OutType)
                    .map(ReturnType::Nullable),
            },
            ast::TypeName::Unit => Ok(ReturnType::Infallible(write_or_unit)),
            ty => self
                .lower_out_type(
                    ty,
                    &mut return_ltl,
                    in_path,
                    TypeLoweringContext::Method,
                    false,
                )
                .map(|ty| ReturnType::Infallible(SuccessType::OutType(ty))),
        }
        .map(|r_ty| (r_ty, return_ltl.finish()))
    }

    fn lower_callback_return_type(
        &mut self,
        return_type: Option<&ast::TypeName>,
        ltl: &mut impl LifetimeLowerer,
        in_path: &ast::Path,
    ) -> Result<ReturnType<InputOnly>, ()> {
        match return_type.unwrap_or(&ast::TypeName::Unit) {
            ast::TypeName::Result(ok_ty, err_ty, _) => {
                let ok_ty = match ok_ty.as_ref() {
                    ast::TypeName::Unit => Ok(SuccessType::Unit),
                    ty => self
                        .lower_type(ty, ltl, TypeLoweringContext::Callback, in_path)
                        .map(SuccessType::OutType),
                };
                let err_ty = match err_ty.as_ref() {
                    ast::TypeName::Unit => Ok(None),
                    ty => self
                        .lower_type(ty, ltl, TypeLoweringContext::Callback, in_path)
                        .map(Some),
                };

                match (ok_ty, err_ty) {
                    (Ok(ok_ty), Ok(err_ty)) => Ok(ReturnType::Fallible(ok_ty, err_ty)),
                    _ => Err(()),
                }
            }
            ty @ ast::TypeName::Option(value_ty, stdlib) => match &**value_ty {
                ast::TypeName::Box(t) | ast::TypeName::Reference(_, _, t) => {
                    match &**t {
                        ast::TypeName::Named(t) | ast::TypeName::SelfType(t) => {
                            let ty = t.resolve(&t.path, self.env);
                            if let ast::CustomType::Opaque(..) = ty {
                                if *stdlib == ast::StdlibOrDiplomat::Diplomat {
                                    self.errors.push(LoweringError::Other("found DiplomatOption<T>, where T is opaque. Please use Option<&T> (DiplomatOption is for primitives, structs, and enums)".to_string()));
                                    return Err(());
                                }
                            }
                        }
                        _ => {}
                    }
                    self.lower_type(ty, ltl, TypeLoweringContext::Callback, in_path)
                        .map(SuccessType::OutType)
                        .map(ReturnType::Infallible)
                }
                ast::TypeName::Unit => Ok(ReturnType::Nullable(SuccessType::Unit)),
                _ => self
                    .lower_type(value_ty, ltl, TypeLoweringContext::Callback, in_path)
                    .map(SuccessType::OutType)
                    .map(ReturnType::Nullable),
            },
            ast::TypeName::Unit => Ok(ReturnType::Infallible(SuccessType::Unit)),
            ty => self
                .lower_type(ty, ltl, TypeLoweringContext::Callback, in_path)
                .map(|ty| ReturnType::Infallible(SuccessType::OutType(ty))),
        }
    }

    fn lower_named_lifetime(
        &mut self,
        lifetime: &ast::lifetimes::LifetimeNode,
    ) -> Result<BoundedLifetime, ()> {
        Ok(BoundedLifetime {
            ident: self.lower_ident(lifetime.lifetime.name(), "lifetime")?,
            longer: lifetime.longer.iter().copied().map(Lifetime::new).collect(),
            shorter: lifetime
                .shorter
                .iter()
                .copied()
                .map(Lifetime::new)
                .collect(),
        })
    }

    /// Lowers a lifetime env found on a type
    ///
    /// Should not be extended to return LifetimeEnv<Method>, which needs to use the lifetime
    /// lowerers to handle elision.
    fn lower_type_lifetime_env(&mut self, ast: &ast::LifetimeEnv) -> Result<LifetimeEnv, ()> {
        let nodes = ast
            .nodes
            .iter()
            .map(|lt| self.lower_named_lifetime(lt))
            .collect::<Result<_, ()>>()?;

        Ok(LifetimeEnv::new(nodes, ast.nodes.len()))
    }
}
