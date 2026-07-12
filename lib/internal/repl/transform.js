'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSort,
  JSONStringify,
  StringPrototypeIncludes,
  StringPrototypeSlice,
} = primordials;

const {
  parse: acornParse,
} = require('internal/deps/acorn/acorn/dist/acorn');
const { ancestor: acornAncestorWalk } = require('internal/deps/acorn/acorn-walk/dist/walk');

// Name of the global helper that performs dynamic imports on behalf of the
// REPL. Shared with `eval.js`, which defines the actual function.
const kDynamicImportName = '__nodeREPLDynamicImport';

// Builds the right-hand side of a transformed import, e.g.
// `await __nodeREPLDynamicImport("node:fs")`.
function awaitImport(specifier, attributes) {
  let attributesArg = '';
  if (attributes?.length > 0) {
    const entries = [];
    for (const attribute of attributes) {
      ArrayPrototypePush(
        entries,
        `${JSONStringify(attribute.key.name ?? attribute.key.value)}: ` +
          JSONStringify(attribute.value.value),
      );
    }
    attributesArg = `, { ${ArrayPrototypeJoin(entries, ', ')} }`;
  }
  return `await ${kDynamicImportName}(${JSONStringify(specifier)}${attributesArg})`;
}

// Turns a single `import` declaration node into an equivalent script statement.
function importDeclarationToScript(node) {
  const { specifiers } = node;
  const specifier = node.source.value;
  const attributes = node.attributes;

  if (specifiers.length === 0) {
    return `${awaitImport(specifier, attributes)};`;
  }

  // A namespace import (`import * as ns from 'mod'`) cannot be mixed with named
  // imports, but it can be mixed with a default import. Handle the namespace
  // binding (and an optional default) on its own since it maps to the module
  // object directly rather than to a destructured property.
  let namespaceName;
  let defaultName;
  const named = [];
  for (const spec of specifiers) {
    switch (spec.type) {
      case 'ImportNamespaceSpecifier':
        namespaceName = spec.local.name;
        break;
      case 'ImportDefaultSpecifier':
        defaultName = spec.local.name;
        break;
      default: {
        // `imported` is an Identifier or, for arbitrary module export names,
        // a string Literal (`import { "a-b" as c }`).
        const imported = spec.imported.name ?? spec.imported.value;
        named.push(imported === spec.local.name ?
          imported :
          `${JSONStringify(imported)}: ${spec.local.name}`);
      }
    }
  }

  if (namespaceName !== undefined) {
    let out = `const ${namespaceName} = ${awaitImport(specifier, attributes)};`;
    if (defaultName !== undefined) {
      out += ` const ${defaultName} = ${namespaceName}.default;`;
    }
    return out;
  }

  if (defaultName !== undefined) {
    ArrayPrototypePush(named, `default: ${defaultName}`);
  }

  return `const { ${ArrayPrototypeJoin(named, ', ')} } = ${awaitImport(specifier, attributes)};`;
}

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

  let ast;
  try {
    ast = acornParse(code, {
      __proto__: null,
      sourceType: 'module',
      ecmaVersion: 'latest',
      allowAwaitOutsideFunction: true,
    });
  } catch {
    // Not parseable as a module (incomplete input, plain script with a
    // top-level return, etc.). Leave it untouched; the evaluator reports any
    // real error and decides whether the input is recoverable.
    return { code, hadModuleSyntax: false };
  }

  const edits = [];
  const replace = (node, text) => ArrayPrototypePush(edits, { start: node.start, end: node.end, text });

  acornAncestorWalk(ast, {
    ImportDeclaration(node) {
      replace(node, importDeclarationToScript(node));
    },
    ImportExpression(node) {
      // Re-route `import(...)` through the helper while preserving the original
      // argument list (specifier and optional import-attributes object).
      const original = StringPrototypeSlice(code, node.start, node.end);
      replace(node, kDynamicImportName + StringPrototypeSlice(original, 'import'.length));
    },
    ExportNamedDeclaration(node) {
      if (node.declaration) {
        // `export const x = 1` becomes `const x = 1`
        replace(node, StringPrototypeSlice(code, node.declaration.start, node.end));
      } else {
        // `export { a, b }` / `export { a } from 'mod'` have no effect;
        // drop 'em!
        replace(node, '');
      }
    },
    ExportDefaultDeclaration(node) {
      // `export default <expr>` becomes `<expr>`
      replace(node, StringPrototypeSlice(code, node.declaration.start, node.end));
    },
    ExportAllDeclaration(node) {
      replace(node, '');
    },
  });

  if (edits.length === 0) {
    return { code, hadModuleSyntax: false };
  }

  // Apply the edits from the end of the source backwards so earlier offsets
  // stay valid as the string is spliced.
  ArrayPrototypeSort(edits, (a, b) => b.start - a.start);
  let out = code;
  for (const { start, end, text } of edits) {
    out = StringPrototypeSlice(out, 0, start) + text + StringPrototypeSlice(out, end);
  }

  return { code: out, hadModuleSyntax: true };
}

module.exports = {
  kDynamicImportName,
  transformModuleSyntax,
};
