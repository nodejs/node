# remark-message-control

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[**remark**][remark] plugin to enable, disable, and ignore messages.

## Note!

This plugin is ready for the new parser in remark
([`remarkjs/remark#536`](https://github.com/remarkjs/remark/pull/536)).
No change is needed: it works exactly the same now as it did before!

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install remark-message-control
```

## Use

Say we have the following file, `example.md`:

```markdown
<!--foo ignore-->

## Heading
```

And our script, `example.js`, looks as follows:

```js
import {readSync} from 'to-vfile'
import {reporter} from 'vfile-reporter'
import remark from 'remark'
import control from 'remark-message-control'

const file = readSync('example.md')

remark()
  .use(() => (tree, file) => {
    file.message('Whoops!', tree.children[1], 'foo:thing')
  })
  .use(control, {name: 'foo'})
  .process(file)
  .then((file) => {
    console.error(reporter(file))
  })
```

Now, running `node example` yields:

```markdown
example.md: no issues found
```

## API

This package exports no identifiers.
The default export is `remarkMessageControl`.

### `unified().use(remarkMessageControl, options)`

Let comment markers control messages from a certain sources.

##### Options

###### `options.name`

Name of markers that can control the message sources (`string`).

For example, `{name: 'alpha'}` controls `alpha` markers:

```markdown
<!--alpha ignore-->
```

###### `options.known`

List of allowed `ruleId`s (`Array.<string>`, optional).
When given, a warning is shown when someone tries to control an unknown rule.

For example, `{name: 'alpha', known: ['bravo']}` results in a warning if
`charlie` is configured:

```markdown
<!--alpha ignore charlie-->
```

###### `options.reset`

Whether to treat all messages as turned off initially (`boolean`, default:
`false`).

###### `options.enable`

List of `ruleId`s to initially turn on if `reset: true` (`Array.<string>`,
optional).
By default (`reset: false`), all rules are turned on.

###### `options.disable`

List of `ruleId`s to turn on if `reset: false` (`Array.<string>`, optional).

###### `options.sources`

Sources that can be controlled with `name` markers (`string` or
`Array.<string>`, default: `options.name`).

### Markers

###### `disable`

The **disable** keyword turns off all messages of the given rule identifiers.
When without identifiers, all messages are turned off.

For example, to turn off certain messages:

```markdown
<!--lint disable list-item-bullet-indent strong-marker-->

*   **foo**

A paragraph, and now another list.

  * __bar__
```

###### `enable`

The **enable** keyword turns on all messages of the given rule identifiers.
When without identifiers, all messages are turned on.

For example, to enable certain messages:

```markdown
<!--lint enable strong-marker-->

**foo** and __bar__.
```

###### `ignore`

The **ignore** keyword turns off all messages of the given `ruleId`s occurring
in the following node.
When without `ruleId`s, all messages are ignored.

After the end of the following node, messages are turned on again.

For example, to turn off certain messages for the next node:

```markdown
<!--lint ignore list-item-bullet-indent strong-marker-->

*   **foo**
  * __bar__
```

## Security

Use of `remark-message-control` does not involve [**rehype**][rehype]
([**hast**][hast]) or user content so there are no openings for
[cross-site scripting (XSS)][xss] attacks.
Messages may be hidden from user content though, causing builds to fail or pass,
or changing a report.

## Contribute

See [`contributing.md`][contributing] in [`remarkjs/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/remarkjs/remark-message-control/workflows/main/badge.svg

[build]: https://github.com/remarkjs/remark-message-control/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/remarkjs/remark-message-control.svg

[coverage]: https://codecov.io/github/remarkjs/remark-message-control

[downloads-badge]: https://img.shields.io/npm/dm/remark-message-control.svg

[downloads]: https://www.npmjs.com/package/remark-message-control

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark-message-control.svg

[size]: https://bundlephobia.com/result?p=remark-message-control

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/remarkjs/remark/discussions

[npm]: https://docs.npmjs.com/cli/install

[health]: https://github.com/remarkjs/.github

[contributing]: https://github.com/remarkjs/.github/blob/HEAD/contributing.md

[support]: https://github.com/remarkjs/.github/blob/HEAD/support.md

[coc]: https://github.com/remarkjs/.github/blob/HEAD/code-of-conduct.md

[license]: license

[author]: https://wooorm.com

[remark]: https://github.com/remarkjs/remark

[xss]: https://en.wikipedia.org/wiki/Cross-site_scripting

[rehype]: https://github.com/rehypejs/rehype

[hast]: https://github.com/syntax-tree/hast
