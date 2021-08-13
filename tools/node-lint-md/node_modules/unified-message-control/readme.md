# unified-message-control

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

Enable, disable, and ignore messages with [**unified**][unified].

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install unified-message-control
```

## Use

Say we have the following file, `example.md`:

```markdown
<!--foo ignore-->

## Heading
```

And our script, `example.js`, looks as follows:

```js
import {toVFile} from 'to-vfile'
import {reporter} from 'vfile-reporter'
import {commentMarker} from 'mdast-comment-marker'
import {unified} from 'unified'
import unifiedMessageControl from 'unified-message-control'
import remarkParse from 'remark-parse'
import remarkStringify from 'remark-stringify'

toVFile.read('example.md').then((file) => {
  unified()
    .use(remarkParse)
    .use(remarkStringify)
    .use(() => (tree, file) => {
      file.message('Whoops!', tree.children[1], 'foo:thing')
    })
    .use(unifiedMessageControl, {
      name: 'foo',
      marker: commentMarker,
      test: 'html'
    })
    .process(file)
    .then((file) => {
      console.error(reporter(file))
    })
})
```

Now, running `node example` yields:

```markdown
example.md: no issues found
```

## API

This package exports the following identifiers: `messageControl`.
There is no default export.

### `unified().use(messageControl, options)`

Let comment markers control messages from certain sources.

##### `options`

###### `options.name`

Name of markers that can control the message sources (`string`).

For example, `{name: 'alpha'}` controls `alpha` markers:

```markdown
<!--alpha ignore-->
```

###### `options.test`

Test for possible markers (`Function`, `string`, `Object`, or `Array.<Test>`).
See [`unist-util-is`][test].

###### `options.marker`

Parse a possible marker to a [comment marker object][marker] (`Function`).
If the possible marker actually isn’t a marker, should return `null`.

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

List of `ruleId`s to initially turn on if `reset: true`
(`Array.<string>`, optional).
By default (`reset: false`), all rules are turned on.

###### `options.disable`

List of `ruleId`s to turn on if `reset: false` (`Array.<string>`, optional).

###### `options.sources`

Sources that can be controlled with `name` markers (`string` or
`Array.<string>`, default: `options.name`)

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

## Contribute

See [`contributing.md`][contributing] in [`unifiedjs/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/unifiedjs/unified-message-control/workflows/main/badge.svg

[build]: https://github.com/unifiedjs/unified-message-control/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/unifiedjs/unified-message-control.svg

[coverage]: https://codecov.io/github/unifiedjs/unified-message-control

[downloads-badge]: https://img.shields.io/npm/dm/unified-message-control.svg

[downloads]: https://www.npmjs.com/package/unified-message-control

[size-badge]: https://img.shields.io/bundlephobia/minzip/unified-message-control.svg

[size]: https://bundlephobia.com/result?p=unified-message-control

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/unifiedjs/unified/discussions

[npm]: https://docs.npmjs.com/cli/install

[health]: https://github.com/unifiedjs/.github

[contributing]: https://github.com/unifiedjs/.github/blob/HEAD/contributing.md

[support]: https://github.com/unifiedjs/.github/blob/HEAD/support.md

[coc]: https://github.com/unifiedjs/.github/blob/HEAD/code-of-conduct.md

[license]: license

[author]: https://wooorm.com

[marker]: https://github.com/syntax-tree/mdast-comment-marker#marker

[unified]: https://github.com/unifiedjs/unified

[test]: https://github.com/syntax-tree/unist-util-is#api
