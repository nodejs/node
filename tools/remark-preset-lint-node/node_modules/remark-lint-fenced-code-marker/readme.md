<!--This file is generated-->

# remark-lint-fenced-code-marker

Warn for violating fenced code markers.

Options: ``'`'``, `'~'`, or `'consistent'`, default: `'consistent'`.

`'consistent'` detects the first used fenced code marker style and warns
when subsequent fenced code-blocks use different styles.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-consistent`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-consistent) |  |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

###### In

```markdown
Indented code blocks are not affected by this rule:

    bravo();
```

###### Out

No messages.

##### `invalid.md`

###### In

````markdown
        ```alpha
        bravo();
        ```

        ~~~
        charlie();
        ~~~
````

###### Out

```text
5:1-7:4: Fenced code should use ` as a marker
```

##### `valid.md`

When configured with ``'`'``.

###### In

````markdown
        ```alpha
        bravo();
        ```

        ```
        charlie();
        ```
````

###### Out

No messages.

##### `valid.md`

When configured with `'~'`.

###### In

```markdown
        ~~~alpha
        bravo();
        ~~~

        ~~~
        charlie();
        ~~~
```

###### Out

No messages.

##### `invalid.md`

When configured with `'!'`.

###### Out

```text
1:1: Invalid fenced code marker `!`: use either `'consistent'`, `` '`' ``, or `'~'`
```

## Install

```sh
npm install remark-lint-fenced-code-marker
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-fenced-code-marker",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-fenced-code-marker readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-fenced-code-marker'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
