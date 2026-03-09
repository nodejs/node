#[cfg(feature = "serde-impl")]
use serde::{Deserialize, Serialize};
use swc_ecma_ast::EsVersion;

#[cfg(feature = "serde-impl")]
const fn true_by_default() -> bool {
    true
}

#[derive(Debug, Clone, Copy)]
#[cfg_attr(feature = "serde-impl", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde-impl", serde(rename_all = "camelCase"))]
#[non_exhaustive]
pub struct Config {
    /// The target runtime environment.
    ///
    /// This defaults to [EsVersion::latest] because it preserves input as much
    /// as possible.
    ///
    /// Note: This does not verifies if output is valid for the target runtime.
    /// e.g. `const foo = 1;` with [EsVersion::Es3] will emitted as `const foo =
    /// 1` without verification.
    /// This is because it's not a concern of the code generator.
    #[cfg_attr(feature = "serde-impl", serde(default = "EsVersion::latest"))]
    pub target: EsVersion,

    /// Forces the code generator to use only ascii characters.
    ///
    /// This is useful for environments that do not support unicode.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub ascii_only: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub minify: bool,

    /// If true, the code generator will emit the lastest semicolon.
    ///
    /// Defaults to `false`.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub omit_last_semi: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub emit_assert_for_import_attributes: bool,

    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub inline_script: bool,

    /// Transform escaped newline `'\n'` of `TplElement` to a newline character
    /// to reduce text size of javascript code
    ///
    /// Defaults to `true`
    #[cfg_attr(feature = "serde-impl", serde(default = "true_by_default"))]
    pub reduce_escaped_newline: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            target: EsVersion::latest(),
            minify: false,
            ascii_only: false,
            omit_last_semi: false,
            emit_assert_for_import_attributes: false,
            inline_script: false,
            reduce_escaped_newline: true,
        }
    }
}

impl Config {
    pub fn with_target(mut self, target: EsVersion) -> Self {
        self.target = target;
        self
    }

    pub fn with_minify(mut self, minify: bool) -> Self {
        self.minify = minify;
        self
    }

    pub fn with_ascii_only(mut self, ascii_only: bool) -> Self {
        self.ascii_only = ascii_only;
        self
    }

    pub fn with_omit_last_semi(mut self, omit_last_semi: bool) -> Self {
        self.omit_last_semi = omit_last_semi;
        self
    }

    pub fn with_emit_assert_for_import_attributes(
        mut self,
        emit_assert_for_import_attributes: bool,
    ) -> Self {
        self.emit_assert_for_import_attributes = emit_assert_for_import_attributes;
        self
    }

    pub fn with_inline_script(mut self, inline_script: bool) -> Self {
        self.inline_script = inline_script;
        self
    }

    /// Transform escaped newline `'\n'` of `TplElement` to a newline character
    /// to reduce text size of javascript code
    ///
    /// Defaults to `true`
    pub fn with_reduce_escaped_newline(mut self, reduce_escaped_newline: bool) -> Self {
        self.reduce_escaped_newline = reduce_escaped_newline;
        self
    }
}
