---
title: .npm-extension
section: 5
description: Imperative, root-owned manifest repairs
---

### Description

A root-owned `.npm-extension.mjs` or `.npm-extension.cjs` file lets a project imperatively repair the manifests of third-party dependencies before npm resolves the dependency tree. It exports a `transformManifest(pkg, context)` function that receives a candidate dependency manifest and returns the effective manifest npm should use.

`.npm-extension` is the imperative counterpart to the declarative [`packageExtensions`](/configuring-npm/package-json#packageextensions) field, and runs in the same pre-resolution phase, **before** `packageExtensions`. Prefer `packageExtensions` for simple, data-only repairs; reach for `.npm-extension` when you need comments and links explaining a repair, conditional logic, repeated repairs expressed as code, deletion or range rewrites, stale-repair guards, or a policy location outside `package.json`.

### Example

```js
// .npm-extension.mjs
export function transformManifest (pkg, context) {
  if (pkg.name === 'foo' && pkg.version.startsWith('1.')) {
    pkg.dependencies = { ...pkg.dependencies, bar: '^2.0.0' }
    context.log(`added bar to ${pkg.name}@${pkg.version}`)
  }
  return pkg
}
```

The `.cjs` form uses CommonJS exports instead:

```js
// .npm-extension.cjs
module.exports = {
  transformManifest (pkg, context) {
    return pkg
  },
}
```

### The `transformManifest` function

`transformManifest(pkg, context)` receives a deeply isolated copy of a candidate dependency manifest. It may mutate and return that copy, or return a new manifest object. It **must** return a manifest object synchronously; returning `null`, `undefined`, a primitive, an array, or a promise fails the install.

The `context` argument is intentionally small:

* `context.log(message)` writes an npm debug log message.
* `context.root` is the absolute path to the project root.
* `context.extensionPoint` is the string `"transformManifest"`.

npm provides no registry, fetch, lockfile, or extraction helpers. Keep the extension file self-contained or limited to Node builtins; npm does not guarantee that project dependencies are available when the file is loaded.

### Supported mutations

Only the four resolution-affecting fields may change:

* `dependencies`
* `optionalDependencies`
* `peerDependencies`
* `peerDependenciesMeta`

Within those fields you may add, replace, or delete entries. Changing any other field (such as `scripts`, `bin`, `engines`, `os`, `cpu`, `exports`, or `main`) is rejected, and the install fails with an error naming `.npm-extension` and the package being processed. The package tarball and the installed `node_modules/<pkg>/package.json` are never rewritten.

### Discovery and `extension-file`

npm looks for a single `.npm-extension.mjs` or `.npm-extension.cjs` at the project root (the workspace root in a workspace project). Having both files present is an error. A `.npm-extension` file in a dependency or in a non-root workspace is ignored; a non-root workspace file produces a warning.

The [`extension-file`](/using-npm/config#extension-file) config selects a different project-local file. It must resolve inside the project root and use a `.mjs` or `.cjs` extension, and it is honored only from project config or the command line — never from user, global, or builtin config.

### Interaction with `packageExtensions` and `overrides`

When both are present, `transformManifest` runs first and `packageExtensions` is applied to its output. Avoid targeting the same package with both unless you intend to rely on that ordering. `overrides` still controls the final resolution target of any edge, including edges created by `transformManifest`.

### Lockfile and `npm ci`

A lockfile influenced by `.npm-extension` records an `npmExtensionHash` (a digest of the selected file's bytes and module format) on its root entry, and minimal `npmExtensionApplied` provenance on each affected package entry. Extension state requires `lockfileVersion: 4`.

Changing the file's contents makes `npm install` re-resolve the affected packages. `npm ci` does **not** import or execute `.npm-extension`; it verifies the recorded hash against the file and reifies the locked graph, failing if the file and lockfile disagree (or if one has extension state and the other does not).

The hash proves only that the install uses the same extension file bytes that generated the lockfile. It does not make arbitrary JavaScript deterministic: extension output that depends on environment variables, the network, the clock, or files imported by the extension can still produce non-reproducible installs. Treat `.npm-extension` as trusted, deterministic project code, and only enable it in repositories you trust.

### Disabling

Set [`ignore-extension`](/using-npm/config#ignore-extension) to skip importing and executing `.npm-extension`. [`ignore-scripts`](/using-npm/config#ignore-scripts) implies `ignore-extension`, since both disable root-owned install-time code. `npm ci` still verifies the file hash even when execution is disabled.

### Publishing

`.npm-extension.mjs` and `.npm-extension.cjs` are project configuration, not package contents. npm excludes the root file from the package tarball produced by `npm pack` and `npm publish`, even when the package's `files` list would include it, so a public package can keep `.npm-extension` in its repository for local use without publishing it.

### See also

* [package.json `packageExtensions`](/configuring-npm/package-json#packageextensions)
* [package-lock.json](/configuring-npm/package-lock-json)
* [config](/using-npm/config)
