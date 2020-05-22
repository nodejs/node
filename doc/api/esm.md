# ECMAScript Modules

<!-- Actual changes have been split into the next commits -->

<!--introduced_in=v8.5.0-->
<!-- type=misc -->

> Stability: 1 - Experimental

## Resolution Algorithm

### Features

The resolver has the following properties:

* FileURL-based resolution as is used by ES modules
* Support for builtin module loading
* Relative and absolute URL resolution
* No default extensions
* No folder mains
* Bare specifier package resolution lookup through node_modules

### Resolver Algorithm

The algorithm to load an ES module specifier is given through the
**ESM_RESOLVE** method below. It returns the resolved URL for a
module specifier relative to a parentURL.

The algorithm to determine the module format of a resolved URL is
provided by **ESM_FORMAT**, which returns the unique module
format for any file. The _"module"_ format is returned for an ECMAScript
Module, while the _"commonjs"_ format is used to indicate loading through the
legacy CommonJS loader. Additional formats such as _"addon"_ can be extended in
future updates.

In the following algorithms, all subroutine errors are propagated as errors
of these top-level routines unless stated otherwise.

_defaultEnv_ is the conditional environment name priority array,
`["node", "import"]`.

The resolver can throw the following errors:
* _Invalid Module Specifier_: Module specifier is an invalid URL, package name
  or package subpath specifier.
* _Invalid Package Configuration_: package.json configuration is invalid or
  contains an invalid configuration.
* _Invalid Package Target_: Package exports define a target module within the
  package that is an invalid type or string target.
* _Package Path Not Exported_: Package exports do not define or permit a target
  subpath in the package for the given module.
* _Module Not Found_: The package or module requested does not exist.

<details>
<summary>Resolver algorithm specification</summary>

**ESM_RESOLVE**(_specifier_, _parentURL_)

> 1. Let _resolvedURL_ be **undefined**.
> 1. If _specifier_ is a valid URL, then
>    1. Set _resolvedURL_ to the result of parsing and reserializing
>       _specifier_ as a URL.
> 1. Otherwise, if _specifier_ starts with _"/"_, then
>    1. Throw an _Invalid Module Specifier_ error.
> 1. Otherwise, if _specifier_ starts with _"./"_ or _"../"_, then
>    1. Set _resolvedURL_ to the URL resolution of _specifier_ relative to
>       _parentURL_.
> 1. Otherwise,
>    1. Note: _specifier_ is now a bare specifier.
>    1. Set _resolvedURL_ the result of
>       **PACKAGE_RESOLVE**(_specifier_, _parentURL_).
> 1. If _resolvedURL_ contains any percent encodings of _"/"_ or _"\\"_ (_"%2f"_
>    and _"%5C"_ respectively), then
>    1. Throw an _Invalid Module Specifier_ error.
> 1. If the file at _resolvedURL_ is a directory, then
>    1. Throw an _Unsupported Directory Import_ error.
> 1. If the file at _resolvedURL_ does not exist, then
>    1. Throw a _Module Not Found_ error.
> 1. Set _resolvedURL_ to the real path of _resolvedURL_.
> 1. Let _format_ be the result of **ESM_FORMAT**(_resolvedURL_).
> 1. Load _resolvedURL_ as module format, _format_.
> 1. Return _resolvedURL_.

**PACKAGE_RESOLVE**(_packageSpecifier_, _parentURL_)

> 1. Let _packageName_ be *undefined*.
> 1. Let _packageSubpath_ be *undefined*.
> 1. If _packageSpecifier_ is an empty string, then
>    1. Throw an _Invalid Module Specifier_ error.
> 1. Otherwise,
>    1. If _packageSpecifier_ does not contain a _"/"_ separator, then
>       1. Throw an _Invalid Module Specifier_ error.
>    1. Set _packageName_ to the substring of _packageSpecifier_
>       until the second _"/"_ separator or the end of the string.
> 1. If _packageName_ starts with _"."_ or contains _"\\"_ or _"%"_, then
>    1. Throw an _Invalid Module Specifier_ error.
> 1. Let _packageSubpath_ be _undefined_.
> 1. If the length of _packageSpecifier_ is greater than the length of
>    _packageName_, then
>    1. Set _packageSubpath_ to _"."_ concatenated with the substring of
>       _packageSpecifier_ from the position at the length of _packageName_.
> 1. If _packageSubpath_ contains any _"."_ or _".."_ segments or percent
>    encoded strings for _"/"_ or _"\\"_, then
>    1. Throw an _Invalid Module Specifier_ error.
> 1. Set _selfUrl_ to the result of
>    **SELF_REFERENCE_RESOLVE**(_packageName_, _packageSubpath_, _parentURL_).
> 1. If _selfUrl_ isn't empty, return _selfUrl_.
> 1. If _packageSubpath_ is _undefined_ and _packageName_ is a Node.js builtin
>    module, then
>    1. Return the string _"nodejs:"_ concatenated with _packageSpecifier_.
> 1. While _parentURL_ is not the file system root,
>    1. Let _packageURL_ be the URL resolution of _"node_modules/"_
>       concatenated with _packageSpecifier_, relative to _parentURL_.
>    1. Set _parentURL_ to the parent folder URL of _parentURL_.
>    1. If the folder at _packageURL_ does not exist, then
>       1. Set _parentURL_ to the parent URL path of _parentURL_.
>       1. Continue the next loop iteration.
>    1. Let _pjson_ be the result of **READ_PACKAGE_JSON**(_packageURL_).
>    1. If _packageSubpath_ is equal to _"./"_, then
>       1. Return _packageURL_ + _"/"_.
>    1. If _packageSubpath_ is _undefined__, then
>       1. Return the result of **PACKAGE_MAIN_RESOLVE**(_packageURL_,
>          _pjson_).
>    1. Otherwise,
>       1. If _pjson_ is not **null** and _pjson_ has an _"exports"_ key, then
>          1. Let _exports_ be _pjson.exports_.
>          1. If _exports_ is not **null** or **undefined**, then
>             1. Return **PACKAGE_EXPORTS_RESOLVE**(_packageURL_,
>                _packageSubpath_, _pjson.exports_).
>       1. Return the URL resolution of _packageSubpath_ in _packageURL_.
> 1. Throw a _Module Not Found_ error.

**SELF_REFERENCE_RESOLVE**(_packageName_, _packageSubpath_, _parentURL_)

> 1. Let _packageURL_ be the result of **READ_PACKAGE_SCOPE**(_parentURL_).
> 1. If _packageURL_ is **null**, then
>    1. Return **undefined**.
> 1. Let _pjson_ be the result of **READ_PACKAGE_JSON**(_packageURL_).
> 1. If _pjson_ does not include an _"exports"_ property, then
>    1. Return **undefined**.
> 1. If _pjson.name_ is equal to _packageName_, then
>    1. If _packageSubpath_ is equal to _"./"_, then
>       1. Return _packageURL_ + _"/"_.
>    1. If _packageSubpath_ is _undefined_, then
>       1. Return the result of **PACKAGE_MAIN_RESOLVE**(_packageURL_, _pjson_).
>    1. Otherwise,
>       1. If _pjson_ is not **null** and _pjson_ has an _"exports"_ key, then
>          1. Let _exports_ be _pjson.exports_.
>          1. If _exports_ is not **null** or **undefined**, then
>             1. Return **PACKAGE_EXPORTS_RESOLVE**(_packageURL_, _subpath_,
>                _pjson.exports_).
>       1. Return the URL resolution of _subpath_ in _packageURL_.
> 1. Otherwise, return **undefined**.

**PACKAGE_MAIN_RESOLVE**(_packageURL_, _pjson_)

> 1. If _pjson_ is **null**, then
>    1. Throw a _Module Not Found_ error.
> 1. If _pjson.exports_ is not **null** or **undefined**, then
>    1. If _exports_ is an Object with both a key starting with _"."_ and a key
>       not starting with _"."_, throw an _Invalid Package Configuration_ error.
>    1. If _pjson.exports_ is a String or Array, or an Object containing no
>       keys starting with _"."_, then
>       1. Return **PACKAGE_EXPORTS_TARGET_RESOLVE**(_packageURL_,
>          _pjson.exports_, _""_).
>    1. If _pjson.exports_ is an Object containing a _"."_ property, then
>       1. Let _mainExport_ be the _"."_ property in _pjson.exports_.
>       1. Return **PACKAGE_EXPORTS_TARGET_RESOLVE**(_packageURL_,
>          _mainExport_, _""_).
>    1. Throw a _Package Path Not Exported_ error.
> 1. Let _legacyMainURL_ be the result applying the legacy
>    **LOAD_AS_DIRECTORY** CommonJS resolver to _packageURL_, throwing a
>    _Module Not Found_ error for no resolution.
> 1. Return _legacyMainURL_.

**PACKAGE_EXPORTS_RESOLVE**(_packageURL_, _packagePath_, _exports_)
> 1. If _exports_ is an Object with both a key starting with _"."_ and a key not
>    starting with _"."_, throw an _Invalid Package Configuration_ error.
> 1. If _exports_ is an Object and all keys of _exports_ start with _"."_, then
>    1. Set _packagePath_ to _"./"_ concatenated with _packagePath_.
>    1. If _packagePath_ is a key of _exports_, then
>       1. Let _target_ be the value of _exports\[packagePath\]_.
>       1. Return **PACKAGE_EXPORTS_TARGET_RESOLVE**(_packageURL_, _target_,
>          _""_, _defaultEnv_).
>    1. Let _directoryKeys_ be the list of keys of _exports_ ending in
>       _"/"_, sorted by length descending.
>    1. For each key _directory_ in _directoryKeys_, do
>       1. If _packagePath_ starts with _directory_, then
>          1. Let _target_ be the value of _exports\[directory\]_.
>          1. Let _subpath_ be the substring of _target_ starting at the index
>             of the length of _directory_.
>          1. Return **PACKAGE_EXPORTS_TARGET_RESOLVE**(_packageURL_, _target_,
>             _subpath_, _defaultEnv_).
> 1. Throw a _Package Path Not Exported_ error.

**PACKAGE_EXPORTS_TARGET_RESOLVE**(_packageURL_, _target_, _subpath_, _env_)

> 1. If _target_ is a String, then
>    1. If _target_ does not start with _"./"_ or contains any _"node_modules"_
>       segments including _"node_modules"_ percent-encoding, throw an
>       _Invalid Package Target_ error.
>    1. Let _resolvedTarget_ be the URL resolution of the concatenation of
>       _packageURL_ and _target_.
>    1. If _resolvedTarget_ is not contained in _packageURL_, throw an
>       _Invalid Package Target_ error.
>    1. If _subpath_ has non-zero length and _target_ does not end with _"/"_,
>       throw an _Invalid Module Specifier_ error.
>    1. Let _resolved_ be the URL resolution of the concatenation of
>       _subpath_ and _resolvedTarget_.
>    1. If _resolved_ is not contained in _resolvedTarget_, throw an
>       _Invalid Module Specifier_ error.
>    1. Return _resolved_.
> 1. Otherwise, if _target_ is a non-null Object, then
>    1. If _exports_ contains any index property keys, as defined in ECMA-262
>       [6.1.7 Array Index][], throw an _Invalid Package Configuration_ error.
>    1. For each property _p_ of _target_, in object insertion order as,
>       1. If _p_ equals _"default"_ or _env_ contains an entry for _p_, then
>          1. Let _targetValue_ be the value of the _p_ property in _target_.
>          1. Return the result of **PACKAGE_EXPORTS_TARGET_RESOLVE**(
>             _packageURL_, _targetValue_, _subpath_, _env_), continuing the
>             loop on any _Package Path Not Exported_ error.
>    1. Throw a _Package Path Not Exported_ error.
> 1. Otherwise, if _target_ is an Array, then
>    1. If _target.length is zero, throw a _Package Path Not Exported_ error.
>    1. For each item _targetValue_ in _target_, do
>       1. If _targetValue_ is an Array, continue the loop.
>       1. Return the result of **PACKAGE_EXPORTS_TARGET_RESOLVE**(_packageURL_,
>          _targetValue_, _subpath_, _env_), continuing the loop on any
>          _Package Path Not Exported_ or _Invalid Package Target_ error.
>    1. Throw the last fallback resolution error.
> 1. Otherwise, if _target_ is _null_, throw a _Package Path Not Exported_
>    error.
> 1. Otherwise throw an _Invalid Package Target_ error.

**ESM_FORMAT**(_url_)

> 1. Assert: _url_ corresponds to an existing file.
> 1. Let _pjson_ be the result of **READ_PACKAGE_SCOPE**(_url_).
> 1. If _url_ ends in _".mjs"_, then
>    1. Return _"module"_.
> 1. If _url_ ends in _".cjs"_, then
>    1. Return _"commonjs"_.
> 1. If _pjson?.type_ exists and is _"module"_, then
>    1. If _url_ ends in _".js"_, then
>       1. Return _"module"_.
>    1. Throw an _Unsupported File Extension_ error.
> 1. Otherwise,
>    1. Throw an _Unsupported File Extension_ error.

**READ_PACKAGE_SCOPE**(_url_)

> 1. Let _scopeURL_ be _url_.
> 1. While _scopeURL_ is not the file system root,
>    1. If _scopeURL_ ends in a _"node_modules"_ path segment, return **null**.
>    1. Let _pjson_ be the result of **READ_PACKAGE_JSON**(_scopeURL_).
>    1. If _pjson_ is not **null**, then
>       1. Return _pjson_.
>    1. Set _scopeURL_ to the parent URL of _scopeURL_.
> 1. Return **null**.

**READ_PACKAGE_JSON**(_packageURL_)

> 1. Let _pjsonURL_ be the resolution of _"package.json"_ within _packageURL_.
> 1. If the file at _pjsonURL_ does not exist, then
>    1. Return **null**.
> 1. If the file at _packageURL_ does not parse as valid JSON, then
>    1. Throw an _Invalid Package Configuration_ error.
> 1. Return the parsed JSON source of the file at _pjsonURL_.

</details>

### Customizing ESM specifier resolution algorithm

The current specifier resolution does not support all default behavior of
the CommonJS loader. One of the behavior differences is automatic resolution
of file extensions and the ability to import directories that have an index
file.

The `--experimental-specifier-resolution=[mode]` flag can be used to customize
the extension resolution algorithm. The default mode is `explicit`, which
requires the full path to a module be provided to the loader. To enable the
automatic extension resolution and importing from directories that include an
index file use the `node` mode.

```bash
$ node index.mjs
success!
$ node index # Failure!
Error: Cannot find module
$ node --experimental-specifier-resolution=node index
success!
```

[Babel]: https://babeljs.io/
[CommonJS]: modules.html
[Conditional Exports]: #esm_conditional_exports
[Dynamic `import()`]: https://wiki.developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import#Dynamic_Imports
[ECMAScript-modules implementation]: https://github.com/nodejs/modules/blob/master/doc/plan-for-new-modules-implementation.md
[ECMAScript Top-Level `await` proposal]: https://github.com/tc39/proposal-top-level-await/
[ES Module Integration Proposal for Web Assembly]: https://github.com/webassembly/esm-integration
[Node.js EP for ES Modules]: https://github.com/nodejs/node-eps/blob/master/002-es-modules.md
[Terminology]: #esm_terminology
[WHATWG JSON modules specification]: https://html.spec.whatwg.org/#creating-a-json-module-script
[`data:` URLs]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/Data_URIs
[`esm`]: https://github.com/standard-things/esm#readme
[`export`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/export
[`getFormat` hook]: #esm_code_getformat_code_hook
[`import()`]: #esm_import_expressions
[`import.meta.url`]: #esm_import_meta
[`import`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import
[`module.createRequire()`]: modules.html#modules_module_createrequire_filename
[`module.syncBuiltinESMExports()`]: modules.html#modules_module_syncbuiltinesmexports
[`transformSource` hook]: #esm_code_transformsource_code_hook
[ArrayBuffer]: http://www.ecma-international.org/ecma-262/6.0/#sec-arraybuffer-constructor
[SharedArrayBuffer]: https://tc39.es/ecma262/#sec-sharedarraybuffer-constructor
[string]: http://www.ecma-international.org/ecma-262/6.0/#sec-string-constructor
[TypedArray]: http://www.ecma-international.org/ecma-262/6.0/#sec-typedarray-objects
[Uint8Array]: http://www.ecma-international.org/ecma-262/6.0/#sec-uint8array
[`util.TextDecoder`]: util.html#util_class_util_textdecoder
[dynamic instantiate hook]: #esm_code_dynamicinstantiate_code_hook
[import an ES or CommonJS module for its side effects only]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import#Import_a_module_for_its_side_effects_only
[special scheme]: https://url.spec.whatwg.org/#special-scheme
[the full specifier path]: #esm_mandatory_file_extensions
[the official standard format]: https://tc39.github.io/ecma262/#sec-modules
[the dual CommonJS/ES module packages section]: #esm_dual_commonjs_es_module_packages
[transpiler loader example]: #esm_transpiler_loader
[6.1.7 Array Index]: https://tc39.es/ecma262/#integer-index
[Top-Level Await]: https://github.com/tc39/proposal-top-level-await
