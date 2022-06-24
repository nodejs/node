# Documentation style guide

This style guide helps us create organized and easy-to-read documentation. It
provides guidelines for organization, spelling, formatting, and more.

These are guidelines rather than strict rules. Content is more important than
formatting. You do not need to learn the entire style guide before contributing
to documentation. Someone can always edit your material later to conform with
this guide.

## Formatting and structure

### Headings

* Each page must have a single `#`-level title at the top.
* Chapters in the same page must have `##`-level headings.
* Sub-chapters need to increase the number of `#` in the heading according to
  their nesting depth.
* The page's title must follow [APA title case][title-case].
* All chapters must follow [sentence case][sentence-style].

### Markdown rules

* Prefer affixing links (`[a link][]`) to inlining links
  (`[a link](http://example.com)`).
* For code blocks:
  * Use [language][]-aware fences. (<code>\`\`\`js</code>)
  * For the [info string][], use one of the following.

    | Meaning       | Info string  |
    | ------------- | ------------ |
    | Bash          | `bash`       |
    | C             | `c`          |
    | C++           | `cpp`        |
    | CoffeeScript  | `coffee`     |
    | Diff          | `diff`       |
    | HTTP          | `http`       |
    | JavaScript    | `js`         |
    | JSON          | `json`       |
    | Markdown      | `markdown`   |
    | Plaintext     | `text`       |
    | Powershell    | `powershell` |
    | R             | `r`          |
    | Shell Session | `console`    |

    If one of your language-aware fences needs an info string that is not
    already on this list, you may use `text` until the grammar gets added to
    [`remark-preset-lint-node`][].
  * Code need not be complete. Treat code blocks as an illustration or aid to
    your point, not as complete running programs. If a complete running program
    is necessary, include it as an asset in `assets/code-examples` and link to
    it.
  * When using underscores, asterisks, and backticks, please use
    backslash-escaping: `\_`, `\*`, and ``\` ``.
  * No nesting lists more than 2 levels (due to the markdown renderer).
  * For unordered lists, use asterisks instead of dashes.

### Document rules

* Documentation is in markdown files with names formatted as
  `lowercase-with-dashes.md`.
  * Use an underscore in the filename only if the underscore is part of the
    topic name (e.g., `child_process`).
  * Some files, such as top-level markdown files, are exceptions.
* Documents should be word-wrapped at 80 characters.

## Language

### Spelling, punctuation, naming, and referencing rules

* [Be direct][].
* [Use US spelling][].
* [Use serial commas][].
* Avoid first-person pronouns (_I_, _we_).
  * Exception: _we recommend foo_ is preferable to _foo is recommended_.
* Use gender-neutral pronouns and gender-neutral plural nouns.
  * OK: _they_, _their_, _them_, _folks_, _people_, _developers_
  * NOT OK: _his_, _hers_, _him_, _her_, _guys_, _dudes_
* Constructors should use PascalCase.
* Instances should use camelCase.
* Denote methods with parentheses: `socket.end()` instead of `socket.end`.
* Use official styling for capitalization in products and projects.
  * OK: JavaScript, Google's V8
  <!--lint disable prohibited-strings remark-lint-->
  * NOT OK: Javascript, Google's v8
* Use _Node.js_ and not _Node_, _NodeJS_, or similar variants.
  <!-- lint enable prohibited-strings remark-lint-->
  * When referring to the executable, _`node`_ is acceptable.

## Additional context and rules

* `.editorconfig` describes the preferred formatting.
  * A [plugin][] is available for some editors to apply these rules.
* Check changes to documentation with `make test-doc -j` or `vcbuild test-doc`.
* When combining wrapping elements (parentheses and quotes), place terminal
  punctuation:
  * Inside the wrapping element if the wrapping element contains a complete
    clause.
  * Outside of the wrapping element if the wrapping element contains only a
    fragment of a clause.
* When documenting APIs, update the YAML comment associated with the API as
  appropriate. This is especially true when introducing or deprecating an API.
* When documenting APIs, every function should have a usage example or
  link to an example that uses the function.

## API references

The following rules only apply to the documentation of APIs.

### Title and description

Each module's API doc must use the actual object name returned by requiring it
as its title (such as `path`, `fs`, and `querystring`).

Directly under the page title, add a one-line description of the module
as a markdown quote (beginning with `>`).

Using the `querystring` module as an example:

```markdown
# querystring

> Utilities for parsing and formatting URL query strings.
```

### Module methods and events

For modules that are not classes, their methods and events must be listed under
the `## Methods` and `## Events` chapters.

Using `fs` as an example:

```markdown
# fs

## Events

### Event: 'close'

## Methods

### `fs.access(path[, mode], callback)`
```

### Methods and their arguments

The methods chapter must be in the following form:

```markdown
### `objectName.methodName(required[, optional]))`

* `required` string - A parameter description.
* `optional` Integer (optional) - Another parameter description.

...
```

#### Function signature

For modules, the `objectName` is the module's name. For classes, it must be the
name of the instance of the class, and must not be the same as the module's
name.

Optional arguments are notated by square brackets `[]` surrounding the optional
argument as well as the comma required if this optional argument follows
another argument:

```markdown
required[, optional]
```

#### Heading level

The heading can be `###` or `####`-levels depending on whether the method
belongs to a module or a class.

### Classes

* API classes or classes that are part of modules must be listed under a
  `## Class: TheClassName` chapter.
* One page can have multiple classes.
* Constructors must be listed with `###`-level headings.
* [Static Methods](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Classes/static)
  must be listed under a `### Static Methods` chapter.
* [Instance Methods](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Classes#Prototype_methods)
  must be listed under an `### Instance Methods` chapter.
* All methods that have a return value must start their description with
  ``Returns `[TYPE]` - (Return description)``
  * If the method returns an `Object`, its structure can be specified
    using a colon followed by a newline then an unordered list of properties in
    the same style as function parameters.
* Instance Events must be listed under an `### Instance Events` chapter.
* Instance Properties must be listed under an `### Instance Properties` chapter.
  * Instance Properties must start with `A [Property Type] ...`

Using the `v8` classes as an example of some of the outlined structure:

```markdown
# `v8`

## Methods

### `v8.cachedDataVersionTag()`

## Class: Serializer

### Instance methods

#### `serializer.writeHeader()`

## Class: Deserializer

### Instance methods

#### `deserializer.readHeader()`

...
```

### Methods and their arguments

The methods chapter must be in the following form:

```markdown
### `objectName.methodName(required[, optional]))`

* `required` string - A parameter description.
* `optional` Integer (optional) - Another parameter description.

...
```

#### Heading level

The heading can be `###` or `####`-levels depending on whether the method
belongs to a module or a class.

#### Function signature

For modules, the `objectName` is the module's name. For classes, it must be the
name of the instance of the class, and must not be the same as the module's
name.

Optional arguments are notated by square brackets `[]` surrounding the optional
argument as well as the comma required if this optional argument follows
another argument:

```markdown
required[, optional]
```

#### Argument descriptions

More detailed information on each of the arguments is noted in an unordered list
below the method. The type of argument is notated by either JavaScript
primitives (e.g. `string`, `Promise`, or `Object`), a custom API structure,
or the wildcard `any`.

If the argument is of type `Array`, use `[]` shorthand with the type of value
inside the array (for example,`any[]` or `string[]`).

If the argument is of type `Promise`, parametrize the type with what the promise
resolves to (for example, `Promise<void>` or `Promise<string>`).

If an argument can be of multiple types, separate the types with `|`.

The description for `Function` type arguments should make it clear how it may be
called and list the types of the parameters that will be passed to it.

#### Platform-specific functionality

If an argument or a method is unique to certain platforms, those platforms are
denoted using a space-delimited italicized list following the datatype. Values
can be `macOS`, `Windows` or `Linux`.

```markdown
* `path` boolean (optional) _macOS_ _Windows_ - the path to write a file to.
```

### Events

The events chapter must be in following form:

```markdown
#### Event: 'message'

Returns:

* `value` any - The transmitted value.

...
```

The heading can be `###` or `####`-levels depending on whether the event
belongs to a module or a class.

The arguments of an event follow the same rules as methods.

### Properties

The properties chapter must be in following form:

```markdown
#### `port.close()`

...
```

The heading can be `###` or `####`-levels depending on whether the property
belongs to a module or a class.

## See also

* API documentation structure overview in [doctools README][].
* [Microsoft Writing Style Guide][].

[Be direct]: https://docs.microsoft.com/en-us/style-guide/word-choice/use-simple-words-concise-sentences
[Microsoft Writing Style Guide]: https://docs.microsoft.com/en-us/style-guide/welcome/
[Use US spelling]: https://docs.microsoft.com/en-us/style-guide/word-choice/use-us-spelling-avoid-non-english-words
[Use serial commas]: https://docs.microsoft.com/en-us/style-guide/punctuation/commas
[`remark-preset-lint-node`]: https://github.com/nodejs/remark-preset-lint-node
[doctools README]: ../tools/doc/README.md
[info string]: https://github.github.com/gfm/#info-string
[language]: https://github.com/highlightjs/highlight.js/blob/HEAD/SUPPORTED_LANGUAGES.md
[plugin]: https://editorconfig.org/#download
[sentence-style]: https://docs.microsoft.com/en-us/style-guide/scannable-content/headings#formatting-headings
[title-case]: https://apastyle.apa.org/style-grammar-guidelines/capitalization/title-case
