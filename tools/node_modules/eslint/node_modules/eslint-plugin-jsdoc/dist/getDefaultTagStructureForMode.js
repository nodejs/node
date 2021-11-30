"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

const getDefaultTagStructureForMode = mode => {
  const isJsdoc = mode === 'jsdoc';
  const isClosure = mode === 'closure';
  const isTypescript = mode === 'typescript';
  const isPermissive = mode === 'permissive';
  const isJsdocOrTypescript = isJsdoc || isTypescript;
  const isTypescriptOrClosure = isTypescript || isClosure;
  const isClosureOrPermissive = isClosure || isPermissive;
  const isJsdocTypescriptOrPermissive = isJsdocOrTypescript || isPermissive; // Properties:
  // `nameContents` - 'namepath-referencing'|'namepath-defining'|'text'|false
  // `typeAllowed` - boolean
  // `nameRequired` - boolean
  // `typeRequired` - boolean
  // `typeOrNameRequired` - boolean
  // All of `typeAllowed` have a signature with "type" except for
  //  `augments`/`extends` ("namepath")
  //  `param`/`arg`/`argument` (no signature)
  //  `property`/`prop` (no signature)
  //  `modifies` (undocumented)
  // None of the `nameContents: 'namepath-defining'` show as having curly
  //  brackets for their name/namepath
  // Among `namepath-defining` and `namepath-referencing`, these do not seem
  //  to allow curly brackets in their doc signature or examples (`modifies`
  //  references namepaths within its type brackets and `param` is
  //  name-defining but not namepath-defining, so not part of these groups)
  // Todo: Should support special processing for "name" as distinct from
  //   "namepath" (e.g., param can't define a namepath)
  // Once checking inline tags:
  // Todo: Re: `typeOrNameRequired`, `@link` (or @linkcode/@linkplain) seems
  //  to require a namepath OR URL and might be checked as such.
  // Todo: Should support a `tutorialID` type (for `@tutorial` block and
  //  inline)

  return new Map([['alias', new Map([// Signature seems to require a "namepath" (and no counter-examples)
  ['nameContents', 'namepath-referencing'], // "namepath"
  ['typeOrNameRequired', true]])], ['arg', new Map([['nameContents', 'namepath-defining'], // See `param`
  ['nameRequired', true], // Has no formal signature in the docs but shows curly brackets
  //   in the examples
  ['typeAllowed', true]])], ['argument', new Map([['nameContents', 'namepath-defining'], // See `param`
  ['nameRequired', true], // Has no formal signature in the docs but shows curly brackets
  //   in the examples
  ['typeAllowed', true]])], ['augments', new Map([// Signature seems to require a "namepath" (and no counter-examples)
  ['nameContents', 'namepath-referencing'], // Does not show curly brackets in either the signature or examples
  ['typeAllowed', true], // "namepath"
  ['typeOrNameRequired', true]])], ['borrows', new Map([// `borrows` has a different format, however, so needs special parsing;
  //   seems to require both, and as "namepath"'s
  ['nameContents', 'namepath-referencing'], // "namepath"
  ['typeOrNameRequired', true]])], ['callback', new Map([// Seems to require a "namepath" in the signature (with no
  //   counter-examples)
  ['nameContents', 'namepath-defining'], // "namepath"
  ['nameRequired', true]])], ['class', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], ['typeAllowed', true]])], ['const', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], ['typeAllowed', true]])], ['constant', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], ['typeAllowed', true]])], ['constructor', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], ['typeAllowed', true]])], ['define', new Map([['typeRequired', isClosure]])], ['emits', new Map([// Signature seems to require a "name" (of an event) and no counter-examples
  ['nameContents', 'namepath-referencing']])], ['enum', new Map([// Has example showing curly brackets but not in doc signature
  ['typeAllowed', true]])], ['event', new Map([// The doc signature of `event` seems to require a "name"
  ['nameRequired', true], // Appears to require a "name" in its signature, albeit somewhat
  //  different from other "name"'s (including as described
  //  at https://jsdoc.app/about-namepaths.html )
  ['nameContents', 'namepath-defining']])], ['exception', new Map([// Shows curly brackets in the signature and in the examples
  ['typeAllowed', true]])], ['export', new Map([['typeAllowed', isClosureOrPermissive]])], ['extends', new Map([// Signature seems to require a "namepath" (and no counter-examples)
  ['nameContents', 'namepath-referencing'], // Does not show curly brackets in either the signature or examples
  ['typeAllowed', isTypescriptOrClosure || isPermissive], ['nameRequired', isJsdoc], // "namepath"
  ['typeOrNameRequired', isTypescriptOrClosure || isPermissive]])], ['external', new Map([// Appears to require a "name" in its signature, albeit somewhat
  //  different from other "name"'s (including as described
  //  at https://jsdoc.app/about-namepaths.html )
  ['nameContents', 'namepath-defining'], // "name" (and a special syntax for the `external` name)
  ['nameRequired', true]])], ['fires', new Map([// Signature seems to require a "name" (of an event) and no
  //  counter-examples
  ['nameContents', 'namepath-referencing']])], ['function', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining']])], ['func', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining']])], ['host', new Map([// Appears to require a "name" in its signature, albeit somewhat
  //  different from other "name"'s (including as described
  //  at https://jsdoc.app/about-namepaths.html )
  ['nameContents', 'namepath-defining'], // See `external`
  ['nameRequired', true], // "namepath"
  ['typeOrNameRequired', true]])], ['interface', new Map([// Allows for "name" in signature, but indicates as optional
  ['nameContents', isJsdocTypescriptOrPermissive ? 'namepath-defining' : false]])], ['implements', new Map([// Shows curly brackets in the doc signature and examples
  // "typeExpression"
  ['typeRequired', true]])], ['lends', new Map([// Signature seems to require a "namepath" (and no counter-examples)
  ['nameContents', 'namepath-referencing'], // "namepath"
  ['typeOrNameRequired', true]])], ['listens', new Map([// Signature seems to require a "name" (of an event) and no
  //  counter-examples
  ['nameContents', 'namepath-referencing']])], ['member', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], // Has example showing curly brackets but not in doc signature
  ['typeAllowed', true]])], ['memberof', new Map([// Signature seems to require a "namepath" (and no counter-examples),
  //  though it allows an incomplete namepath ending with connecting symbol
  ['nameContents', 'namepath-referencing'], // "namepath"
  ['typeOrNameRequired', true]])], ['memberof!', new Map([// Signature seems to require a "namepath" (and no counter-examples),
  //  though it allows an incomplete namepath ending with connecting symbol
  ['nameContents', 'namepath-referencing'], // "namepath"
  ['typeOrNameRequired', true]])], ['method', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining']])], ['mixes', new Map([// Signature seems to require a "OtherObjectPath" with no
  //   counter-examples
  ['nameContents', 'namepath-referencing'], // "OtherObjectPath"
  ['typeOrNameRequired', true]])], ['mixin', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining']])], ['modifies', new Map([// Has no documentation, but test example has curly brackets, and
  //  "name" would be suggested rather than "namepath" based on example;
  //  not sure if name is required
  ['typeAllowed', true]])], ['module', new Map([// Optional "name" and no curly brackets
  //  this block impacts `no-undefined-types` and `valid-types` (search for
  //  "isNamepathDefiningTag|tagMightHaveNamepath|tagMightHaveEitherTypeOrNamePosition")
  ['nameContents', isJsdoc ? 'namepath-defining' : 'text'], // Shows the signature with curly brackets but not in the example
  ['typeAllowed', true]])], ['name', new Map([// Seems to require a "namepath" in the signature (with no
  //   counter-examples)
  ['nameContents', 'namepath-defining'], // "namepath"
  ['nameRequired', true], // "namepath"
  ['typeOrNameRequired', true]])], ['namespace', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], // Shows the signature with curly brackets but not in the example
  ['typeAllowed', true]])], ['package', new Map([// Shows the signature with curly brackets but not in the example
  // "typeExpression"
  ['typeAllowed', isClosureOrPermissive]])], ['param', new Map([['nameContents', 'namepath-defining'], // Though no signature provided requiring, per
  //  https://jsdoc.app/tags-param.html:
  // "The @param tag requires you to specify the name of the parameter you
  //  are documenting."
  ['nameRequired', true], // Has no formal signature in the docs but shows curly brackets
  //   in the examples
  ['typeAllowed', true]])], ['private', new Map([// Shows the signature with curly brackets but not in the example
  // "typeExpression"
  ['typeAllowed', isClosureOrPermissive]])], ['prop', new Map([['nameContents', 'namepath-defining'], // See `property`
  ['nameRequired', true], // Has no formal signature in the docs but shows curly brackets
  //   in the examples
  ['typeAllowed', true]])], ['property', new Map([['nameContents', 'namepath-defining'], // No docs indicate required, but since parallel to `param`, we treat as
  //   such:
  ['nameRequired', true], // Has no formal signature in the docs but shows curly brackets
  //   in the examples
  ['typeAllowed', true]])], ['protected', new Map([// Shows the signature with curly brackets but not in the example
  // "typeExpression"
  ['typeAllowed', isClosureOrPermissive]])], ['public', new Map([// Does not show a signature nor show curly brackets in the example
  ['typeAllowed', isClosureOrPermissive]])], ['returns', new Map([// Shows curly brackets in the signature and in the examples
  ['typeAllowed', true]])], ['return', new Map([// Shows curly brackets in the signature and in the examples
  ['typeAllowed', true]])], ['see', new Map([// Signature allows for "namepath" or text, so user must configure to
  //  'namepath-referencing' to enforce checks
  ['nameContents', 'text']])], ['static', new Map([// Does not show a signature nor show curly brackets in the example
  ['typeAllowed', isClosureOrPermissive]])], ['template', new Map([['nameContents', isJsdoc ? 'text' : 'namepath-referencing'], // Though defines `nameContents: 'namepath-defining'` in a sense, it is
  //   not parseable in the same way for template (e.g., allowing commas),
  //   so not adding
  ['typeAllowed', isTypescriptOrClosure || isPermissive]])], ['this', new Map([// Signature seems to require a "namepath" (and no counter-examples)
  // Not used with namepath in Closure/TypeScript, however
  ['nameContents', isJsdoc ? 'namepath-referencing' : false], ['typeRequired', isTypescriptOrClosure], // namepath
  ['typeOrNameRequired', isJsdoc]])], ['throws', new Map([// Shows curly brackets in the signature and in the examples
  ['typeAllowed', true]])], ['type', new Map([// Shows curly brackets in the doc signature and examples
  // "typeName"
  ['typeRequired', true]])], ['typedef', new Map([// Seems to require a "namepath" in the signature (with no
  //  counter-examples)
  ['nameContents', 'namepath-defining'], // "namepath"
  ['nameRequired', isJsdocTypescriptOrPermissive], // Has example showing curly brackets but not in doc signature
  ['typeAllowed', true], // "namepath"
  ['typeOrNameRequired', true]])], ['var', new Map([// Allows for "name"'s in signature, but indicated as optional
  ['nameContents', 'namepath-defining'], // Has example showing curly brackets but not in doc signature
  ['typeAllowed', true]])], ['yields', new Map([// Shows curly brackets in the signature and in the examples
  ['typeAllowed', true]])], ['yield', new Map([// Shows curly brackets in the signature and in the examples
  ['typeAllowed', true]])]]);
};

var _default = getDefaultTagStructureForMode;
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=getDefaultTagStructureForMode.js.map