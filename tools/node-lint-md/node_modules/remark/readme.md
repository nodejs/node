# remark

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][size-badge]][size]
[![Sponsors][sponsors-badge]][collective]
[![Backers][backers-badge]][collective]
[![Chat][chat-badge]][chat]

[**unified**][unified] processor to parse and serialize markdown.
Built on [micromark][].
Powered by [plugins][].
Part of the [unified][] collective.

*   API by [**unified**][unified]
*   Parses markdown to a syntax tree with [`remark-parse`][parse]
*   [**mdast**][mdast] syntax tree
*   [Plugins][] transform the tree
*   Serializes syntax trees to markdown with [`remark-stringify`][stringify]

Don’t need the parser?
Or compiler?
[That’s OK: use **unified** directly][unified-usage].

## Install

This package is [ESM only](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c):
Node 12+ is needed to use it and it must be `import`ed instead of `require`d.

[npm][]:

```sh
npm install remark
```

## Use

[See **unified** for more examples »][unified]

###### Common example

This example lints markdown and turns it into HTML.

```js
import {reporter} from 'vfile-reporter'
import {remark} from 'remark'
import remarkPresetLintRecommended from 'remark-preset-lint-recommended'
import remarkHtml from 'remark-html'

remark()
  .use(remarkPresetLintRecommended)
  .use(remarkHtml)
  .process('## Hello world!')
  .then((file) => {
    console.error(reporter(file))
    console.log(String(file))
  })
```

Yields:

```txt
  1:1  warning  Missing newline character at end of file  final-newline  remark-lint

⚠ 1 warning
```

```html
<h2>Hello world!</h2>
```

###### Settings through data

This example prettifies markdown and configures [`remark-stringify`][stringify]
through [data][].

```js
import {remark} from 'remark'

remark()
  .data('settings', {emphasis: '*', strong: '*'})
  .process('_Emphasis_ and __importance__')
  .then((file) => {
    console.log(String(file))
  })
```

Yields:

```markdown
*Emphasis* and **importance**
```

###### Settings through a preset

This example prettifies markdown and configures [`remark-parse`][parse] and
[`remark-stringify`][stringify] through a [preset][].

```js
import {remark} from 'remark'

remark()
  .use({settings: {emphasis: '*', strong: '*'}})
  .process('_Emphasis_ and __importance__')
  .then((file) => {
    console.log(String(file))
  })
```

Yields:

```markdown
*Emphasis* and **importance**
```

## API

[See **unified** for API docs »][unified]

This package exports the following identifier: `remark`.
There is no default export.

## Security

As markdown is sometimes used for HTML, and improper use of HTML can open you up
to a [cross-site scripting (XSS)][xss] attack, use of remark can also be unsafe.
When going to HTML, use remark in combination with the [**rehype**][rehype]
ecosystem, and use [`rehype-sanitize`][sanitize] to make the tree safe.

Use of remark plugins could also open you up to other attacks.
Carefully assess each plugin and the risks involved in using them.

## Contribute

See [`contributing.md`][contributing] in [`remarkjs/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.
Ideas for new plugins and tools can be posted in [`remarkjs/ideas`][ideas].

A curated list of awesome remark resources can be found in [**awesome
remark**][awesome].

This project has a [code of conduct][coc].
By interacting with this repository, organization, or community you agree to
abide by its terms.

## Sponsor

Support this effort and give back by sponsoring on [OpenCollective][collective]!

<!--lint ignore no-html-->

<table>
<tr valign="middle">
<td width="20%" align="center" colspan="2">
  <a href="https://www.gatsbyjs.org">Gatsby</a> 🥇<br><br>
  <a href="https://www.gatsbyjs.org"><img src="https://avatars1.githubusercontent.com/u/12551863?s=256&v=4" width="128"></a>
</td>
<td width="20%" align="center" colspan="2">
  <a href="https://vercel.com">Vercel</a> 🥇<br><br>
  <a href="https://vercel.com"><img src="https://avatars1.githubusercontent.com/u/14985020?s=256&v=4" width="128"></a>
</td>
<td width="20%" align="center" colspan="2">
  <a href="https://www.netlify.com">Netlify</a><br><br>
  <!--OC has a sharper image-->
  <a href="https://www.netlify.com"><img src="https://images.opencollective.com/netlify/4087de2/logo/256.png" width="128"></a>
</td>
<td width="10%" align="center">
  <a href="https://www.holloway.com">Holloway</a><br><br>
  <a href="https://www.holloway.com"><img src="https://avatars1.githubusercontent.com/u/35904294?s=128&v=4" width="64"></a>
</td>
<td width="10%" align="center">
  <a href="https://themeisle.com">ThemeIsle</a><br><br>
  <a href="https://themeisle.com"><img src="https://avatars1.githubusercontent.com/u/58979018?s=128&v=4" width="64"></a>
</td>
<td width="10%" align="center">
  <a href="https://boosthub.io">Boost Hub</a><br><br>
  <a href="https://boosthub.io"><img src="https://images.opencollective.com/boosthub/6318083/logo/128.png" width="64"></a>
</td>
<td width="10%" align="center">
  <a href="https://expo.io">Expo</a><br><br>
  <a href="https://expo.io"><img src="https://avatars1.githubusercontent.com/u/12504344?s=128&v=4" width="64"></a>
</td>
</tr>
<tr valign="middle">
<td width="100%" align="center" colspan="10">
  <br>
  <a href="https://opencollective.com/unified"><strong>You?</strong></a>
  <br><br>
</td>
</tr>
</table>

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/remarkjs/remark/workflows/main/badge.svg

[build]: https://github.com/remarkjs/remark/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/remarkjs/remark.svg

[coverage]: https://codecov.io/github/remarkjs/remark

[downloads-badge]: https://img.shields.io/npm/dm/remark.svg

[downloads]: https://www.npmjs.com/package/remark

[size-badge]: https://img.shields.io/bundlephobia/minzip/remark.svg

[size]: https://bundlephobia.com/result?p=remark

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[collective]: https://opencollective.com/unified

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/remarkjs/remark/discussions

[health]: https://github.com/remarkjs/.github

[contributing]: https://github.com/remarkjs/.github/blob/HEAD/contributing.md

[support]: https://github.com/remarkjs/.github/blob/HEAD/support.md

[coc]: https://github.com/remarkjs/.github/blob/HEAD/code-of-conduct.md

[ideas]: https://github.com/remarkjs/ideas

[awesome]: https://github.com/remarkjs/awesome-remark

[license]: https://github.com/remarkjs/remark/blob/main/license

[author]: https://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[unified]: https://github.com/unifiedjs/unified

[mdast]: https://github.com/syntax-tree/mdast

[parse]: https://github.com/remarkjs/remark/blob/main/packages/remark-parse

[stringify]: https://github.com/remarkjs/remark/blob/main/packages/remark-stringify

[plugins]: https://github.com/remarkjs/remark/blob/main/doc/plugins.md

[unified-usage]: https://github.com/unifiedjs/unified#usage

[preset]: https://github.com/unifiedjs/unified#preset

[data]: https://github.com/unifiedjs/unified#processordatakey-value

[xss]: https://en.wikipedia.org/wiki/Cross-site_scripting

[rehype]: https://github.com/rehypejs/rehype

[sanitize]: https://github.com/rehypejs/rehype-sanitize

[micromark]: https://github.com/micromark/micromark
