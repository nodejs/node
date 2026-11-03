'use strict';

const {
  StringPrototypeIncludes,
} = primordials;

// Name of the global helper that performs dynamic imports on behalf of the
// REPL. Shared with `eval.js`, which defines the actual function, and with
// Amaro's `transformModuleSyntax`, which emits calls to a helper of this name.
const kDynamicImportName = '__nodeREPLDynamicImport';

// Analyses `code` and returns the rewritten script plus a flag indicating
// whether module syntax was present. When `code` cannot be parsed as a module
// (for example, because it is incomplete) the original source is returned
// unchanged and the caller deals with the resulting evaluation error.
function transformModuleSyntax(code) {
  // Every construct this transform rewrites (`import` declarations,
  // `import(...)`/`import.meta`, and the various `export` forms) necessarily
  // contains the substring `import` or `export`. When neither appears there is
  // nothing to rewrite, so skip the comparatively expensive full module parse
  // that would otherwise run on every REPL line. A false positive (the word
  // appearing only inside a string or comment) merely falls back to the parse
  // below, so this is purely a fast path.
  if (!StringPrototypeIncludes(code, 'import') &&
      !StringPrototypeIncludes(code, 'export')) {
    return { code, hadModuleSyntax: false };
  }

  const amaro = require('internal/deps/amaro/dist/internal');
  try {
    return amaro.transformModuleSyntax(code);
  } catch {
    // Not parseable as a module (incomplete input, plain script with a
    // top-level return, etc.). Leave it untouched; the evaluator reports any
    // real error and decides whether the input is recoverable.
    return { code, hadModuleSyntax: false };
  }
}

module.exports = {
  kDynamicImportName,
  transformModuleSyntax,
};
