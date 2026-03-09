use bytes_str::BytesStr;
use serde::{Deserialize, Serialize};

#[derive(Debug, Default, Serialize, Deserialize, Clone, Copy)]
#[serde(rename_all = "camelCase")]
pub struct Config {
    /// https://www.typescriptlang.org/tsconfig#verbatimModuleSyntax
    #[serde(default)]
    pub verbatim_module_syntax: bool,

    /// Native class properties support
    #[serde(default)]
    pub native_class_properties: bool,

    /// https://www.typescriptlang.org/tsconfig/#importsNotUsedAsValues
    #[serde(default)]
    pub import_not_used_as_values: ImportsNotUsedAsValues,

    /// Don't create `export {}`.
    /// By default, strip creates `export {}` for modules to preserve module
    /// context.
    ///
    /// https://github.com/swc-project/swc/issues/1698
    #[serde(default)]
    pub no_empty_export: bool,

    #[serde(default)]
    pub import_export_assign_config: TsImportExportAssignConfig,

    /// Disables an optimization that inlines TS enum member values
    /// within the same module that assumes the enum member values
    /// are never modified.
    ///
    /// Defaults to false.
    #[serde(default)]
    pub ts_enum_is_mutable: bool,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct TsxConfig {
    /// Note: this pass handle jsx directives in comments
    #[serde(default)]
    pub pragma: Option<BytesStr>,

    /// Note: this pass handle jsx directives in comments
    #[serde(default)]
    pub pragma_frag: Option<BytesStr>,
}

#[derive(Default, Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum TsImportExportAssignConfig {
    ///  - Rewrite `import foo = require("foo")` to `var foo = require("foo")`
    ///  - Rewrite `export =` to `module.exports = `
    ///
    /// Note: This option is deprecated as all CJS/AMD/UMD can handle it
    /// themselves.
    #[default]
    Classic,

    /// preserve for CJS/AMD/UMD
    Preserve,

    /// Rewrite `import foo = require("foo")` to
    /// ```javascript
    /// import { createRequire as _createRequire } from "module";
    /// const __require = _createRequire(import.meta.url);
    /// const foo = __require("foo");
    /// ```
    ///
    /// Report error for `export =`
    NodeNext,

    /// Both `import =` and `export =` are disabled.
    /// An error will be reported if an import/export assignment is found.
    EsNext,
}

#[derive(Debug, Default, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[non_exhaustive]
pub enum ImportsNotUsedAsValues {
    #[serde(rename = "remove")]
    #[default]
    Remove,
    #[serde(rename = "preserve")]
    Preserve,
}
