# ECMAScript Modules

<!-- Actual changes have been split into the next commits -->

<!--introduced_in=v8.5.0-->
<!-- type=misc -->

> Stability: 1 - Experimental

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
