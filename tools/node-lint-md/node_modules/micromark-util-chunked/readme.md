# micromark-util-chunked

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility to splice and push with giant arrays.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`push(list, items)`](#pushlist-items)
    *   [`splice(list, start, remove, items)`](#splicelist-start-remove-items)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-chunked
```

## Use

```js
import {push, splice} from 'micromark-util-chunked'

// …

nextEvents = push(nextEvents, [
  ['enter', events[open][1], context],
  ['exit', events[open][1], context]
])

// …

splice(events, open - 1, index - open + 3, nextEvents)

// …
```

## API

This module exports the following identifiers: `push`, `splice`.
There is no default export.

### `push(list, items)`

Append `items` (an array) at the end of `list` (another array).
When `list` was empty, returns `items` instead.

This prevents a potentially expensive operation when `list` is empty,
and adds items in batches to prevent V8 from hanging.

###### Parameters

*   `list` (`unknown[]`) — List to operate on
*   `items` (`unknown[]`) — Items to add to `list`

###### Returns

`list` or `items`

### `splice(list, start, remove, items)`

Like `Array#splice`, but smarter for giant arrays.

`Array#splice` takes all items to be inserted as individual argument which
causes a stack overflow in V8 when trying to insert 100k items for instance.

Otherwise, this does not return the removed items, and takes `items` as an
array instead of rest parameters.

###### Parameters

*   `list` (`unknown[]`) — List to operate on
*   `start` (`number`) — Index to remove/insert at (can be negative)
*   `remove` (`number`) — Number of items to remove
*   `items` (`unknown[]`) — Items to inject into `list`

###### Returns

`void`

## Security

See [`security.md`][securitymd] in [`micromark/.github`][health] for how to
submit a security report.

## Contribute

See [`contributing.md`][contributing] in [`micromark/.github`][health] for ways
to get started.
See [`support.md`][support] for ways to get help.

This project has a [code of conduct][coc].
By interacting with this repository, organisation, or community you agree to
abide by its terms.

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://github.com/micromark/micromark/workflows/main/badge.svg

[build]: https://github.com/micromark/micromark/actions

[coverage-badge]: https://img.shields.io/codecov/c/github/micromark/micromark.svg

[coverage]: https://codecov.io/github/micromark/micromark

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-chunked.svg

[downloads]: https://www.npmjs.com/package/micromark-util-chunked

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-chunked.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-chunked

[sponsors-badge]: https://opencollective.com/unified/sponsors/badge.svg

[backers-badge]: https://opencollective.com/unified/backers/badge.svg

[opencollective]: https://opencollective.com/unified

[npm]: https://docs.npmjs.com/cli/install

[chat-badge]: https://img.shields.io/badge/chat-discussions-success.svg

[chat]: https://github.com/micromark/micromark/discussions

[license]: https://github.com/micromark/micromark/blob/main/license

[author]: https://wooorm.com

[health]: https://github.com/micromark/.github

[securitymd]: https://github.com/micromark/.github/blob/HEAD/security.md

[contributing]: https://github.com/micromark/.github/blob/HEAD/contributing.md

[support]: https://github.com/micromark/.github/blob/HEAD/support.md

[coc]: https://github.com/micromark/.github/blob/HEAD/code-of-conduct.md
