# micromark-util-resolve-all

[![Build][build-badge]][build]
[![Coverage][coverage-badge]][coverage]
[![Downloads][downloads-badge]][downloads]
[![Size][bundle-size-badge]][bundle-size]
[![Sponsors][sponsors-badge]][opencollective]
[![Backers][backers-badge]][opencollective]
[![Chat][chat-badge]][chat]

micromark utility to resolve subtokens.

[Resolvers][resolver] are functions that take events and manipulate them.
This is needed for example because media (links, images) and attention (strong,
italic) aren’t parsed left-to-right.
Instead, their openings and closings are parsed, and when done, their openings
and closings are matched, and left overs are turned into plain text.
Because media and attention can’t overlap, we need to perform that operation
when one closing matches an opening, too.

## Contents

*   [Install](#install)
*   [Use](#use)
*   [API](#api)
    *   [`resolveAll(constructs, events, context)`](#resolveallconstructs-events-context)
*   [Security](#security)
*   [Contribute](#contribute)
*   [License](#license)

## Install

[npm][]:

```sh
npm install micromark-util-resolve-all
```

## Use

```js
import {push} from 'micromark-util-chunked'
import {resolveAll} from 'micromark-util-resolve-all'

/**
 * @type {Resolver}
 */
function resolveAllAttention(events, context) {
  // …

  // Walk through all events.
  while (++index < events.length) {
    // Find a token that can close.
    if (
      events[index][0] === 'enter' &&
      events[index][1].type === 'attentionSequence' &&
      events[index][1]._close
    ) {
      open = index

      // Now walk back to find an opener.
      while (open--) {
        // Find a token that can open the closer.
        if (
          // …
        ) {
          // …

          // Opening.
          nextEvents = push(nextEvents, [
            // …
          ])

          // Between.
          nextEvents = push(
            nextEvents,
            resolveAll(
              context.parser.constructs.insideSpan.null,
              events.slice(open + 1, index),
              context
            )
          )

          // Closing.
          nextEvents = push(nextEvents, [
            // …
          ])

          // …
        }
      }
    }
  }

  // …
}
```

## API

This module exports the following identifiers: `resolveAll`.
There is no default export.

### `resolveAll(constructs, events, context)`

Call all `resolveAll`s in `constructs`.

###### Parameters

*   `constructs` (`Construct[]`) — List of constructs, optionally with
    `resolveAll`s
*   `events` (`Event[]`) — List of events
*   `context` (`TokenizeContext`) — Context used by `tokenize`

###### Returns

`Events[]` — Changed events.

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

[downloads-badge]: https://img.shields.io/npm/dm/micromark-util-resolve-all.svg

[downloads]: https://www.npmjs.com/package/micromark-util-resolve-all

[bundle-size-badge]: https://img.shields.io/bundlephobia/minzip/micromark-util-resolve-all.svg

[bundle-size]: https://bundlephobia.com/result?p=micromark-util-resolve-all

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

[resolver]: https://github.com/micromark/micromark/blob/a571c09/packages/micromark-util-types/index.js#L219
