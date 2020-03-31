# Style Guide

* Documentation is in markdown files with names formatted as
  `lowercase-with-dashes.md`.
  * Use an underscore in the filename only if the underscore is part of the
    topic name (e.g., `child_process`).
  * Some files, such as top-level markdown files, are exceptions.
* Documents should be word-wrapped at 80 characters.
* `.editorconfig` describes the preferred formatting.
  * A [plugin][] is available for some editors to apply these rules.
* Check changes to documentation with `make lint-md`.
* Use American English spelling.
  * OK: _capitalize_, _color_
  * NOT OK: _capitalise_, _colour_
* Use [serial commas][].
* Avoid personal pronouns (_I_, _you_, _we_) in reference documentation.
  * Personal pronouns are acceptable in colloquial documentation such as guides.
  * Use gender-neutral pronouns and gender-neutral plural nouns.
    * OK: _they_, _their_, _them_, _folks_, _people_, _developers_
    * NOT OK: _his_, _hers_, _him_, _her_, _guys_, _dudes_
* When combining wrapping elements (parentheses and quotes), place terminal
  punctuation:
  * Inside the wrapping element if the wrapping element contains a complete
    clause.
  * Outside of the wrapping element if the wrapping element contains only a
    fragment of a clause.
* Documents must start with a level-one heading.
* Prefer affixing links (`[a link][]`) to inlining links
  (`[a link](http://example.com)`).
* When documenting APIs, update the YAML comment associated with the API as
  appropriate. This is especially true when introducing or deprecating an API.
* For code blocks:
  * Use language aware fences. ("```js")
  * Code need not be complete. Treat code blocks as an illustration or aid to
    your point, not as complete running programs. If a complete running program
    is necessary, include it as an asset in `assets/code-examples` and link to
    it.
* When using underscores, asterisks, and backticks, please use
  backslash-escaping: `\_`, `\*`, and ``\` ``.
* Constructors should use PascalCase.
* Instances should use camelCase.
* Denote methods with parentheses: `socket.end()` instead of `socket.end`.
* Function arguments or object properties should use the following format:
  * ```* `name` {type|type2} Optional description. **Default:** `value`.```
  <!--lint disable maximum-line-length remark-lint-->
  * For example: <code>* `byteOffset` {integer} Index of first byte to expose. **Default:** `0`.</code>
  <!--lint enable maximum-line-length remark-lint-->
  * The `type` should refer to a Node.js type or a [JavaScript type][].
* Function returns should use the following format:
  * <code>* Returns: {type|type2} Optional description.</code>
  * E.g. <code>* Returns: {AsyncHook} A reference to `asyncHook`.</code>
* Use official styling for capitalization in products and projects.
  * OK: JavaScript, Google's V8
  <!--lint disable prohibited-strings remark-lint-->
  * NOT OK: Javascript, Google's v8
* Use _Node.js_ and not _Node_, _NodeJS_, or similar variants.
  <!-- lint enable prohibited-strings remark-lint-->
  * When referring to the executable, _`node`_ is acceptable.
* Be direct.
  * OK: The return value is a string.
  <!-- lint disable prohibited-strings remark-lint-->
  * NOT OK: It is important to note that, in all cases, the return value will be
    a string regardless.
  <!-- lint enable prohibited-strings remark-lint-->

See also API documentation structure overview in [doctools README][].

[Javascript type]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Grammar_and_types#Data_structures_and_types
[serial commas]: https://en.wikipedia.org/wiki/Serial_comma
[plugin]: https://editorconfig.org/#download
[doctools README]: ../../tools/doc/README.md
