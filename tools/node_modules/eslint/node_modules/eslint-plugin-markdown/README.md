# eslint-plugin-markdown

![Screenshot](screenshot.png)

An [ESLint](http://eslint.org/) plugin to lint JavaScript in Markdown.

Supported extensions are `.markdown`, `.mdown`, `.mkdn`, and `.md`.

## Usage

Install the plugin:

```sh
npm install --save-dev eslint eslint-plugin-markdown
```

Add it to your `.eslintrc`:

```json
{
    "plugins": [
        "markdown"
    ]
}
```

Run ESLint on `.md` files:

```sh
eslint --ext md .
```

It will lint `js`, `javascript`, `jsx`, or `node` [fenced code blocks](https://help.github.com/articles/github-flavored-markdown/#fenced-code-blocks) in your Markdown documents:

    ```js
    // This gets linted
    var answer = 6 * 7;
    console.log(answer);
    ```

    ```JavaScript
    // This also gets linted

    /* eslint quotes: [2, "double"] */

    function hello() {
        console.log("Hello, world!");
    }
    hello();
    ```

    ```jsx
    // This gets linted too
    var div = <div className="jsx"></div>;
    ```

    ```node
    // And this
    console.log(process.version);
    ```

Blocks that don't specify either `js`, `javascript`, `jsx`, or `node` syntax are ignored:

    ```
    This is plain text and doesn't get linted.
    ```

    ```python
    print("This doesn't get linted either.")
    ```

## Configuration Comments

The processor will convert HTML comments immediately preceding a code block into JavaScript block comments and insert them at the beginning of the source code that it passes to ESLint. This permits configuring ESLint via configuration comments while keeping the configuration comments themselves hidden when the markdown is rendered. Comment bodies are passed through unmodified, so the plugin supports any [configuration comments](http://eslint.org/docs/user-guide/configuring) supported by ESLint itself.

This example enables the `browser` environment, disables the `no-alert` rule, and configures the `quotes` rule to prefer single quotes:

    <!-- eslint-env browser -->
    <!-- eslint-disable no-alert -->
    <!-- eslint quotes: ["error", "single"] -->

    ```js
    alert('Hello, world!');
    ```

Each code block in a file is linted separately, so configuration comments apply only to the code block that immediately follows.

    Assuming `no-alert` is enabled in `.eslintrc`, the first code block will have no error from `no-alert`:

    <!-- eslint-env browser -->
    <!-- eslint-disable no-alert -->

    ```js
    alert("Hello, world!");
    ```

    But the next code block will have an error from `no-alert`:

    <!-- eslint-env browser -->

    ```js
    alert("Hello, world!");
    ```

## Skipping Blocks

Sometimes it can be useful to have code blocks marked with `js` even though they don't contain valid JavaScript syntax, such as commented JSON blobs that need `js` syntax highlighting. Standard `eslint-disable` comments only silence rule reporting, but ESLint still reports any syntax errors it finds. In cases where a code block should not even be parsed, insert a non-standard `<!-- eslint-skip -->` comment before the block, and this plugin will hide the following block from ESLint. Neither rule nor syntax errors will be reported.

    There are comments in this JSON, so we use `js` syntax for better
    highlighting. Skip the block to prevent warnings about invalid syntax.

    <!-- eslint-skip -->

    ```js
    {
        // This code block is hidden from ESLint.
        "hello": "world"
    }
    ```

    ```js
    console.log("This code block is linted normally.");
    ```

## Fix issues automatically

This plugin can attempt to fix some of the issues automatically using [`fix` ESLint option](https://eslint.org/docs/user-guide/command-line-interface#fixing-problems). This option instructs ESLint to try to fix as many issues as possible. To enable this option you can add `--fix` to your ESLint call, for example:

```bash
eslint --fix --ext md .
```

## Unsatisfiable Rules

Since code blocks are not files themselves but embedded inside a Markdown document, some rules do not apply to Markdown code blocks, and messages from these rules are automatically suppressed:

- `eol-last`
- `unicode-bom`

### Project or directory-wide overrides for code snippets

Given that code snippets often lack full context, and adding full context
through configuration comments may be too cumbersome to apply for each snippet,
one may wish to instead set defaults for all one's JavaScript snippets in a
manner that applies to all Markdown files within your project (or a specific
directory).

ESLint allows a configuration property `overrides` which has a `files`
property which accepts a
[glob pattern](https://eslint.org/docs/user-guide/configuring#configuration-based-on-glob-patterns), allowing you to designate files (such as all `md` files) whose rules will
be overridden.

The following example shows the disabling of a few commonly problematic rules
for code snippets. It also points to the fact that some rules
(e.g., `padded-blocks`) may be more appealing for disabling given that
one may wish for documentation to be more liberal in providing padding for
readability.

```js
// .eslintrc.json
{
    // ...
    "overrides": [{
        "files": ["**/*.md"],
        "rules": {
            "no-undef": "off",
            "no-unused-vars": "off",
            "no-console": "off",
            "padded-blocks": "off"
        }
    }]
}
```

#### Overriding `strict`

The `strict` rule is technically satisfiable inside of Markdown code blocks, but writing a `"use strict"` directive at the top of every code block is tedious and distracting. We recommend a glob pattern for `.md` files to disable `strict` and enable the `impliedStrict` [parser option](https://eslint.org/docs/user-guide/configuring#specifying-parser-options) so the code blocks still parse in strict mode:

```js
// .eslintrc.json
{
    // ...
    "overrides": [{
        "files": ["**/*.md"],
        "parserOptions": {
            "ecmaFeatures": {
                "impliedStrict": true
            }
        },
        "rules": {
            "strict": "off"
        }
    }]
}
```

## Tips for use with Atom linter-eslint

The [linter-eslint](https://atom.io/packages/linter-eslint) package allows for
linting within the [Atom IDE](https://atom.io/).

In order to see `eslint-plugin-markdown` work its magic within Markdown code
blocks in your Atom editor, you can go to `linter-eslint`'s settings and
within "List of scopes to run ESLint on...", add the cursor scope "source.gfm".

However, this reports a problem when viewing Markdown which does not have
configuration, so you may wish to use the cursor scope "source.embedded.js",
but note that `eslint-plugin-markdown` configuration comments and skip
directives won't work in this context.

## Contributing

```sh
$ git clone https://github.com/eslint/eslint-plugin-markdown.git
$ cd eslint-plugin-markdown
$ npm install
$ npm test
```

This project follows the [ESLint contribution guidelines](http://eslint.org/docs/developer-guide/contributing/).
