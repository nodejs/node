# Style Guide

* Documentation is written in markdown files with names formatted as
  `lowercase-with-dashes.md`.
  * Underscores in filenames are allowed only when they are present in the
    topic the document will describe (e.g. `child_process`).
  * Some files, such as top-level markdown files, are exceptions.
* Documents should be word-wrapped at 80 characters.
* The formatting described in `.editorconfig` is preferred.
  * A [plugin][] is available for some editors to automatically apply these
    rules.
* Changes to documentation should be checked with `make lint-md`.
* American English spelling is preferred.
  * OK: _capitalize_, _color_
  * NOT OK: _capitalise_, _colour_
* Use [serial commas][].
* Avoid personal pronouns (_I_, _you_, _we_) in reference documentation.
  * Personal pronouns are acceptable in colloquial documentation such as guides.
  * Use gender-neutral pronouns and gender-neutral plural nouns.
    * OK: _they_, _their_, _them_, _folks_, _people_, _developers_
    * NOT OK: _his_, _hers_, _him_, _her_, _guys_, _dudes_
* When combining wrapping elements (parentheses and quotes), terminal
  punctuation should be placed:
  * Inside the wrapping element if the wrapping element contains a complete
    clause — a subject, verb, and an object.
  * Outside of the wrapping element if the wrapping element contains only a
    fragment of a clause.
* Documents must start with a level-one heading.
* Prefer affixing links to inlining links — prefer `[a link][]` to
  `[a link](http://example.com)`.
* When documenting APIs, note the version the API was introduced in at
  the end of the section. If an API has been deprecated, also note the first
  version that the API appeared deprecated in.
* When using dashes, use [Em dashes][] ("—" or `Option+Shift+"-"` on macOS)
  surrounded by spaces, as per [The New York Times Manual of Style and Usage][].
* Including assets:
  * If you wish to add an illustration or full program, add it to the
    appropriate sub-directory in the `assets/` dir.
  * Link to it like so: `[Asset](/assets/{subdir}/{filename})` for file-based
    assets, and `![Asset](/assets/{subdir}/{filename})` for image-based assets.
  * For illustrations, prefer SVG to other assets. When SVG is not feasible,
    please keep a close eye on the filesize of the asset you're introducing.
* For code blocks:
  * Use language aware fences. ("```js")
  * Code need not be complete — treat code blocks as an illustration or aid to
    your point, not as complete running programs. If a complete running program
    is necessary, include it as an asset in `assets/code-examples` and link to
    it.
* When using underscores, asterisks, and backticks, please use proper escaping
  (`\_`, `\*` and ``\` `` instead of `_`, `*` and `` ` ``).
* References to constructor functions should use PascalCase.
* References to constructor instances should use camelCase.
* References to methods should be used with parentheses: for example,
  `socket.end()` instead of `socket.end`.
* Function arguments or object properties should use the following format:
  * ``` * `name` {type|type2} Optional description. **Default:** `value`. ```
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
  <!-- lint enable prohibited-strings remark-lint-->
* Use _Node.js_ and not _Node_, _NodeJS_, or similar variants.
  * When referring to the executable, _`node`_ is acceptable.

See also API documentation structure overview in [doctools README][].

[Em dashes]: https://en.wikipedia.org/wiki/Dash#Em_dash
[Javascript type]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Grammar_and_types#Data_structures_and_types
[serial commas]: https://en.wikipedia.org/wiki/Serial_comma
[The New York Times Manual of Style and Usage]: https://en.wikipedia.org/wiki/The_New_York_Times_Manual_of_Style_and_Usage
[plugin]: http://editorconfig.org/#download
[doctools README]: ../tools/doc/README.md
