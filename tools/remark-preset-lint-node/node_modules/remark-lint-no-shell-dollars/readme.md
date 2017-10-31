<!--This file is generated-->

# remark-lint-no-shell-dollars

Warn when shell code is prefixed by dollar-characters.

Ignores indented code blocks and fenced code blocks without language flag.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

###### In

````markdown
        ```sh
        echo a
        echo a > file
        ```

        ```zsh
        $ echo a
        a
        $ echo a > file
        ```

        It’s fine to use dollars in non-shell code.

        ```js
        $('div').remove();
        ```
````

###### Out

No messages.

##### `invalid.md`

###### In

````markdown
        ```bash
        $ echo a
        $ echo a > file
        ```
````

###### Out

```text
1:1-4:4: Do not use dollar signs before shell-commands
```

## Install

```sh
npm install remark-lint-no-shell-dollars
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-shell-dollars",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-shell-dollars readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-shell-dollars'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
