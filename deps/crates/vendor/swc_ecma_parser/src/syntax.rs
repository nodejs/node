use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields, tag = "syntax")]
pub enum Syntax {
    /// Standard
    #[serde(rename = "ecmascript")]
    Es(EsSyntax),
    /// This variant requires the cargo feature `typescript` to be enabled.
    #[cfg(feature = "typescript")]
    #[cfg_attr(docsrs, doc(cfg(feature = "typescript")))]
    #[serde(rename = "typescript")]
    Typescript(TsSyntax),
}

impl Default for Syntax {
    fn default() -> Self {
        Syntax::Es(Default::default())
    }
}

impl Syntax {
    pub fn auto_accessors(self) -> bool {
        match self {
            Syntax::Es(EsSyntax {
                auto_accessors: true,
                ..
            }) => true,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(_) => true,
            _ => false,
        }
    }

    pub fn import_attributes(self) -> bool {
        true
    }

    /// Should we parse jsx?
    pub fn jsx(self) -> bool {
        match self {
            Syntax::Es(EsSyntax { jsx: true, .. }) => true,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(TsSyntax { tsx: true, .. }) => true,
            _ => false,
        }
    }

    pub fn fn_bind(self) -> bool {
        matches!(self, Syntax::Es(EsSyntax { fn_bind: true, .. }))
    }

    pub fn decorators(self) -> bool {
        match self {
            Syntax::Es(EsSyntax {
                decorators: true, ..
            }) => true,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(TsSyntax {
                decorators: true, ..
            }) => true,
            _ => false,
        }
    }

    pub fn decorators_before_export(self) -> bool {
        match self {
            Syntax::Es(EsSyntax {
                decorators_before_export: true,
                ..
            }) => true,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(..) => true,
            _ => false,
        }
    }

    /// Should we parse typescript?
    #[cfg(not(feature = "typescript"))]
    pub const fn typescript(self) -> bool {
        false
    }

    /// Should we parse typescript?
    #[cfg(feature = "typescript")]
    pub const fn typescript(self) -> bool {
        matches!(self, Syntax::Typescript(..))
    }

    pub fn export_default_from(self) -> bool {
        matches!(
            self,
            Syntax::Es(EsSyntax {
                export_default_from: true,
                ..
            })
        )
    }

    pub fn dts(self) -> bool {
        match self {
            #[cfg(feature = "typescript")]
            Syntax::Typescript(t) => t.dts,
            _ => false,
        }
    }

    pub fn allow_super_outside_method(self) -> bool {
        match self {
            Syntax::Es(EsSyntax {
                allow_super_outside_method,
                ..
            }) => allow_super_outside_method,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(_) => true,
        }
    }

    pub fn allow_return_outside_function(self) -> bool {
        match self {
            Syntax::Es(EsSyntax {
                allow_return_outside_function,
                ..
            }) => allow_return_outside_function,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(_) => false,
        }
    }

    pub fn early_errors(self) -> bool {
        match self {
            #[cfg(feature = "typescript")]
            Syntax::Typescript(t) => !t.no_early_errors,
            Syntax::Es(..) => true,
        }
    }

    pub fn disallow_ambiguous_jsx_like(self) -> bool {
        match self {
            #[cfg(feature = "typescript")]
            Syntax::Typescript(t) => t.disallow_ambiguous_jsx_like,
            _ => false,
        }
    }

    pub fn explicit_resource_management(&self) -> bool {
        match self {
            Syntax::Es(EsSyntax {
                explicit_resource_management: using_decl,
                ..
            }) => *using_decl,
            #[cfg(feature = "typescript")]
            Syntax::Typescript(_) => true,
        }
    }

    pub fn into_flags(self) -> SyntaxFlags {
        match self {
            Syntax::Es(es) => es.into_flags(),
            #[cfg(feature = "typescript")]
            Syntax::Typescript(ts) => ts.into_flags(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TsSyntax {
    #[serde(default)]
    pub tsx: bool,

    #[serde(default)]
    pub decorators: bool,

    /// `.d.ts`
    #[serde(skip, default)]
    pub dts: bool,

    #[serde(skip, default)]
    pub no_early_errors: bool,

    /// babel: `disallowAmbiguousJSXLike`
    /// Even when JSX parsing is not enabled, this option disallows using syntax
    /// that would be ambiguous with JSX (`<X> y` type assertions and
    /// `<X>()=>{}` type arguments)
    /// see: https://babeljs.io/docs/en/babel-plugin-transform-typescript#disallowambiguousjsxlike
    #[serde(skip, default)]
    pub disallow_ambiguous_jsx_like: bool,
}

#[cfg(feature = "typescript")]
impl TsSyntax {
    fn into_flags(self) -> SyntaxFlags {
        let mut flags = SyntaxFlags::TS
            .union(SyntaxFlags::AUTO_ACCESSORS)
            .union(SyntaxFlags::IMPORT_ATTRIBUTES)
            .union(SyntaxFlags::DECORATORS_BEFORE_EXPORT)
            .union(SyntaxFlags::ALLOW_SUPER_OUTSIDE_METHOD)
            .union(SyntaxFlags::EXPLICIT_RESOURCE_MANAGEMENT);

        if self.tsx {
            flags |= SyntaxFlags::JSX;
        }
        if self.decorators {
            flags |= SyntaxFlags::DECORATORS;
        }
        if self.dts {
            flags |= SyntaxFlags::DTS;
        }
        if self.no_early_errors {
            flags |= SyntaxFlags::NO_EARLY_ERRORS;
        }
        if self.disallow_ambiguous_jsx_like {
            flags |= SyntaxFlags::DISALLOW_AMBIGUOUS_JSX_LIKE;
        }
        flags
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct EsSyntax {
    #[serde(default)]
    pub jsx: bool,

    /// Support function bind expression.
    #[serde(rename = "functionBind")]
    #[serde(default)]
    pub fn_bind: bool,

    /// Enable decorators.
    #[serde(default)]
    pub decorators: bool,

    /// babel: `decorators.decoratorsBeforeExport`
    ///
    /// Effective only if `decorator` is true.
    #[serde(rename = "decoratorsBeforeExport")]
    #[serde(default)]
    pub decorators_before_export: bool,

    #[serde(default)]
    pub export_default_from: bool,

    /// Stage 4
    /// Always true in swc
    #[serde(default, alias = "importAssertions")]
    pub import_attributes: bool,

    #[serde(default, rename = "allowSuperOutsideMethod")]
    pub allow_super_outside_method: bool,

    #[serde(default, rename = "allowReturnOutsideFunction")]
    pub allow_return_outside_function: bool,

    #[serde(default)]
    pub auto_accessors: bool,

    #[serde(default)]
    pub explicit_resource_management: bool,
}

impl EsSyntax {
    fn into_flags(self) -> SyntaxFlags {
        let mut flags = SyntaxFlags::empty();
        if self.jsx {
            flags |= SyntaxFlags::JSX;
        }
        if self.fn_bind {
            flags |= SyntaxFlags::FN_BIND;
        }
        if self.decorators {
            flags |= SyntaxFlags::DECORATORS;
        }
        if self.decorators_before_export {
            flags |= SyntaxFlags::DECORATORS_BEFORE_EXPORT;
        }
        if self.export_default_from {
            flags |= SyntaxFlags::EXPORT_DEFAULT_FROM;
        }
        if self.import_attributes {
            flags |= SyntaxFlags::IMPORT_ATTRIBUTES;
        }
        if self.allow_super_outside_method {
            flags |= SyntaxFlags::ALLOW_SUPER_OUTSIDE_METHOD;
        }
        if self.allow_return_outside_function {
            flags |= SyntaxFlags::ALLOW_RETURN_OUTSIDE_FUNCTION;
        }
        if self.auto_accessors {
            flags |= SyntaxFlags::AUTO_ACCESSORS;
        }
        if self.explicit_resource_management {
            flags |= SyntaxFlags::EXPLICIT_RESOURCE_MANAGEMENT;
        }
        flags
    }
}

impl SyntaxFlags {
    #[inline(always)]
    pub const fn auto_accessors(&self) -> bool {
        self.contains(Self::AUTO_ACCESSORS)
    }

    #[inline(always)]
    pub const fn import_attributes(&self) -> bool {
        true
    }

    /// Should we parse jsx?
    #[inline(always)]
    pub const fn jsx(&self) -> bool {
        self.contains(SyntaxFlags::JSX)
    }

    #[inline(always)]
    pub const fn fn_bind(&self) -> bool {
        self.contains(SyntaxFlags::FN_BIND)
    }

    #[inline(always)]
    pub const fn decorators(&self) -> bool {
        self.contains(SyntaxFlags::DECORATORS)
    }

    #[inline(always)]
    pub const fn decorators_before_export(&self) -> bool {
        self.contains(SyntaxFlags::DECORATORS_BEFORE_EXPORT)
    }

    /// Should we parse typescript?
    #[cfg(not(feature = "typescript"))]
    #[inline(always)]
    pub const fn typescript(&self) -> bool {
        false
    }

    /// Should we parse typescript?
    #[cfg(feature = "typescript")]
    #[inline(always)]
    pub const fn typescript(&self) -> bool {
        self.contains(SyntaxFlags::TS)
    }

    #[inline(always)]
    pub const fn export_default_from(&self) -> bool {
        self.contains(SyntaxFlags::EXPORT_DEFAULT_FROM)
    }

    #[inline(always)]
    pub const fn dts(&self) -> bool {
        self.contains(SyntaxFlags::DTS)
    }

    #[inline(always)]
    pub const fn allow_super_outside_method(&self) -> bool {
        self.contains(SyntaxFlags::ALLOW_SUPER_OUTSIDE_METHOD)
    }

    #[inline(always)]
    pub const fn allow_return_outside_function(&self) -> bool {
        self.contains(SyntaxFlags::ALLOW_RETURN_OUTSIDE_FUNCTION)
    }

    #[inline(always)]
    pub const fn early_errors(&self) -> bool {
        !self.contains(SyntaxFlags::NO_EARLY_ERRORS)
    }

    #[inline(always)]
    pub const fn disallow_ambiguous_jsx_like(&self) -> bool {
        self.contains(SyntaxFlags::TS.union(SyntaxFlags::DISALLOW_AMBIGUOUS_JSX_LIKE))
    }

    #[inline(always)]
    pub const fn explicit_resource_management(&self) -> bool {
        self.contains(SyntaxFlags::EXPLICIT_RESOURCE_MANAGEMENT)
    }
}

bitflags::bitflags! {
    #[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
    pub struct SyntaxFlags: u16 {
        const JSX = 1 << 0;
        const FN_BIND = 1 << 1;
        const DECORATORS = 1 << 2;
        const DECORATORS_BEFORE_EXPORT = 1 << 3;
        const EXPORT_DEFAULT_FROM = 1 << 4;
        const IMPORT_ATTRIBUTES = 1 << 5;
        const ALLOW_SUPER_OUTSIDE_METHOD = 1 << 6;
        const ALLOW_RETURN_OUTSIDE_FUNCTION = 1 << 7;
        const AUTO_ACCESSORS = 1 << 8;
        const EXPLICIT_RESOURCE_MANAGEMENT = 1 << 9;
        const DTS = 1 << 10;
        const NO_EARLY_ERRORS = 1 << 11;
        const DISALLOW_AMBIGUOUS_JSX_LIKE = 1 << 12;
        const TS = 1 << 13;
    }
}
