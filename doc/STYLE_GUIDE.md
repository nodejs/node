# Style Guide

* Documents are written in markdown files.
* Those files should be written in **`lowercase-with-dashes.md`.**
  * Underscores in filenames are allowed only when they are present in the
    topic the document will describe (e.g., `child_process`.)
  * Filenames should be **lowercase**.
  * Some files, such as top-level markdown files, are exceptions.
  * Older files may use the `.markdown` extension. These may be ported to `.md`
    at will. **Prefer `.md` for all new documents.**
* Documents should be word-wrapped at 80 characters.
* The formatting described in `.editorconfig` is preferred.
  * A [plugin][] is available for some editors to automatically apply these rules.
* Mechanical issues, like spelling and grammar, should be identified by tools,
  insofar as is possible. If not caught by a tool, they should be pointed out by
  human reviewers.
* American English spelling is preferred. "Capitalize" vs. "Capitalise",
  "color" vs. "colour", etc.
* Though controversial, the [Oxford comma][] is preferred for clarity's sake.
* Generally avoid personal pronouns in reference documentation ("I", "you",
  "we".)
  * Pronouns are acceptable in more colloquial documentation, like guides.
  * Use **gender-neutral pronouns** and **mass nouns**. Non-comprehensive
    examples:
    * **OK**: "they", "their", "them", "folks", "people", "developers", "cats"
    * **NOT OK**: "his", "hers", "him", "her", "guys", "dudes".
* When combining wrapping elements (parentheses and quotes), terminal
  punctuation should be placed:
  * Inside the wrapping element if the wrapping element contains a complete
    clause — a subject, verb, and an object.
  * Outside of the wrapping element if the wrapping element contains only a
    fragment of a clause.
* Place end-of-sentence punctuation inside wrapping elements — periods go
  inside parentheses and quotes, not after.
* Documents must start with a level-one heading. An example document will be
  linked here eventually.
* Prefer affixing links to inlining links — prefer `[a link][]` to
  `[a link](http://example.com)`.
* When documenting APIs, note the version the API was introduced in at
  the end of the section. If an API has been deprecated, also note the first
  version that the API appeared deprecated in.
* When using dashes, use emdashes ("—", Ctrl+Alt+"-" on OSX) surrounded by
  spaces, per the New York Times usage.
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
* When using underscores, asterisks and backticks please use proper escaping (**\\\_**, **\\\*** and **\\\`** instead of **\_**, **\*** and **\`**)
* References to constructor functions should use PascalCase
* References to constructor instances should be camelCased
* References to methods should be used with parentheses: `socket.end()` instead of `socket.end`

[plugin]: http://editorconfig.org/#download
[Oxford comma]: https://en.wikipedia.org/wiki/Serial_comma
