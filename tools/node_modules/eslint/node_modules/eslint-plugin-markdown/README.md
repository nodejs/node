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

## Unsatisfiable Rules

Since code blocks are not files themselves but embedded inside a Markdown document, some rules do not apply to Markdown code blocks, and messages from these rules are automatically suppressed:

- `eol-last`

## Contributing

```sh
$ git clone https://github.com/eslint/eslint-plugin-markdown.git
$ cd eslint-plugin-markdown
$ npm install
$ npm test
```

This project follows the [ESLint contribution guidelines](http://eslint.org/docs/developer-guide/contributing/).
