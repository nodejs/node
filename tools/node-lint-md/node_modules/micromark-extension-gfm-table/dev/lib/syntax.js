/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').Resolver} Resolver
 * @typedef {import('micromark-util-types').Tokenizer} Tokenizer
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Token} Token
 */

/**
 * @typedef {'left'|'center'|'right'|null} Align
 */

import assert from 'assert'
import {factorySpace} from 'micromark-factory-space'
import {
  markdownLineEnding,
  markdownLineEndingOrSpace,
  markdownSpace
} from 'micromark-util-character'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'

/** @type {Extension} */
export const gfmTable = {
  flow: {null: {tokenize: tokenizeTable, resolve: resolveTable}}
}

const setextUnderlineMini = {
  tokenize: tokenizeSetextUnderlineMini,
  partial: true
}
const nextPrefixedOrBlank = {
  tokenize: tokenizeNextPrefixedOrBlank,
  partial: true
}

/** @type {Resolver} */
function resolveTable(events, context) {
  let index = -1
  /** @type {Token} */
  let token
  /** @type {boolean|undefined} */
  let inHead
  /** @type {boolean|undefined} */
  let inDelimiterRow
  /** @type {boolean|undefined} */
  let inRow
  /** @type {Token} */
  let cell
  /** @type {Token} */
  let content
  /** @type {Token} */
  let text
  /** @type {number|undefined} */
  let contentStart
  /** @type {number|undefined} */
  let contentEnd
  /** @type {number|undefined} */
  let cellStart

  while (++index < events.length) {
    token = events[index][1]

    if (inRow) {
      if (token.type === 'temporaryTableCellContent') {
        contentStart = contentStart || index
        contentEnd = index
      }

      if (
        // Combine separate content parts into one.
        (token.type === 'tableCellDivider' || token.type === 'tableRow') &&
        contentEnd
      ) {
        content = {
          type: 'tableContent',
          // @ts-expect-error `contentStart` is defined if `contentEnd` is too.
          start: events[contentStart][1].start,
          end: events[contentEnd][1].end
        }
        text = {
          type: types.chunkText,
          start: content.start,
          end: content.end,
          // @ts-expect-error It’s fine.
          contentType: constants.contentTypeText
        }

        events.splice(
          // @ts-expect-error `contentStart` is defined if `contentEnd` is too.
          contentStart,
          // @ts-expect-error `contentStart` is defined if `contentEnd` is too.
          contentEnd - contentStart + 1,
          ['enter', content, context],
          ['enter', text, context],
          ['exit', text, context],
          ['exit', content, context]
        )
        // @ts-expect-error `contentStart` is defined if `contentEnd` is too.
        index -= contentEnd - contentStart - 3
        contentStart = undefined
        contentEnd = undefined
      }
    }

    if (
      events[index][0] === 'exit' &&
      cellStart &&
      cellStart + 1 < index &&
      (token.type === 'tableCellDivider' ||
        (token.type === 'tableRow' &&
          (cellStart + 3 < index ||
            events[cellStart][1].type !== types.whitespace)))
    ) {
      cell = {
        type: inDelimiterRow
          ? 'tableDelimiter'
          : inHead
          ? 'tableHeader'
          : 'tableData',
        start: events[cellStart][1].start,
        end: events[index][1].end
      }
      events.splice(index + (token.type === 'tableCellDivider' ? 1 : 0), 0, [
        'exit',
        cell,
        context
      ])
      events.splice(cellStart, 0, ['enter', cell, context])
      index += 2
      cellStart = index + 1
    }

    if (token.type === 'tableRow') {
      inRow = events[index][0] === 'enter'

      if (inRow) {
        cellStart = index + 1
      }
    }

    if (token.type === 'tableDelimiterRow') {
      inDelimiterRow = events[index][0] === 'enter'

      if (inDelimiterRow) {
        cellStart = index + 1
      }
    }

    if (token.type === 'tableHead') {
      inHead = events[index][0] === 'enter'
    }
  }

  return events
}

/** @type {Tokenizer} */
function tokenizeTable(effects, ok, nok) {
  const self = this
  /** @type {Align[]} */
  const align = []
  let tableHeaderCount = 0
  /** @type {boolean|undefined} */
  let seenDelimiter
  /** @type {boolean|undefined} */
  let hasDash

  return start

  /** @type {State} */
  function start(code) {
    // @ts-expect-error Custom.
    effects.enter('table')._align = align
    effects.enter('tableHead')
    effects.enter('tableRow')

    // If we start with a pipe, we open a cell marker.
    if (code === codes.verticalBar) {
      return cellDividerHead(code)
    }

    tableHeaderCount++
    effects.enter('temporaryTableCellContent')
    // Can’t be space or eols at the start of a construct, so we’re in a cell.
    assert(!markdownLineEndingOrSpace(code), 'expected non-space')
    return inCellContentHead(code)
  }

  /** @type {State} */
  function cellDividerHead(code) {
    assert(code === codes.verticalBar, 'expected `|`')
    effects.enter('tableCellDivider')
    effects.consume(code)
    effects.exit('tableCellDivider')
    seenDelimiter = true
    return cellBreakHead
  }

  /** @type {State} */
  function cellBreakHead(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return atRowEndHead(code)
    }

    if (markdownSpace(code)) {
      effects.enter(types.whitespace)
      effects.consume(code)
      return inWhitespaceHead
    }

    if (seenDelimiter) {
      seenDelimiter = undefined
      tableHeaderCount++
    }

    if (code === codes.verticalBar) {
      return cellDividerHead(code)
    }

    // Anything else is cell content.
    effects.enter('temporaryTableCellContent')
    return inCellContentHead(code)
  }

  /** @type {State} */
  function inWhitespaceHead(code) {
    if (markdownSpace(code)) {
      effects.consume(code)
      return inWhitespaceHead
    }

    effects.exit(types.whitespace)
    return cellBreakHead(code)
  }

  /** @type {State} */
  function inCellContentHead(code) {
    // EOF, whitespace, pipe
    if (
      code === codes.eof ||
      code === codes.verticalBar ||
      markdownLineEndingOrSpace(code)
    ) {
      effects.exit('temporaryTableCellContent')
      return cellBreakHead(code)
    }

    effects.consume(code)
    return code === codes.backslash
      ? inCellContentEscapeHead
      : inCellContentHead
  }

  /** @type {State} */
  function inCellContentEscapeHead(code) {
    if (code === codes.backslash || code === codes.verticalBar) {
      effects.consume(code)
      return inCellContentHead
    }

    // Anything else.
    return inCellContentHead(code)
  }

  /** @type {State} */
  function atRowEndHead(code) {
    if (code === codes.eof) {
      return nok(code)
    }

    assert(markdownLineEnding(code), 'expected eol')
    effects.exit('tableRow')
    effects.exit('tableHead')
    return effects.attempt(
      {tokenize: tokenizeRowEnd, partial: true},
      atDelimiterLineStart,
      nok
    )(code)
  }

  /** @type {State} */
  function atDelimiterLineStart(code) {
    // To do: is the lazy setext thing still needed?
    return effects.check(
      setextUnderlineMini,
      nok,
      // Support an indent before the delimiter row.
      factorySpace(
        effects,
        rowStartDelimiter,
        types.linePrefix,
        constants.tabSize
      )
    )(code)
  }

  /** @type {State} */
  function rowStartDelimiter(code) {
    // If there’s another space, or we’re at the EOL/EOF, exit.
    if (code === codes.eof || markdownLineEndingOrSpace(code)) {
      return nok(code)
    }

    effects.enter('tableDelimiterRow')
    return atDelimiterRowBreak(code)
  }

  /** @type {State} */
  function atDelimiterRowBreak(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return rowEndDelimiter(code)
    }

    if (markdownSpace(code)) {
      effects.enter(types.whitespace)
      effects.consume(code)
      return inWhitespaceDelimiter
    }

    if (code === codes.dash) {
      effects.enter('tableDelimiterFiller')
      effects.consume(code)
      hasDash = true
      align.push(null)
      return inFillerDelimiter
    }

    if (code === codes.colon) {
      effects.enter('tableDelimiterAlignment')
      effects.consume(code)
      effects.exit('tableDelimiterAlignment')
      align.push('left')
      return afterLeftAlignment
    }

    // If we start with a pipe, we open a cell marker.
    if (code === codes.verticalBar) {
      effects.enter('tableCellDivider')
      effects.consume(code)
      effects.exit('tableCellDivider')
      return atDelimiterRowBreak
    }

    return nok(code)
  }

  /** @type {State} */
  function inWhitespaceDelimiter(code) {
    if (markdownSpace(code)) {
      effects.consume(code)
      return inWhitespaceDelimiter
    }

    effects.exit(types.whitespace)
    return atDelimiterRowBreak(code)
  }

  /** @type {State} */
  function inFillerDelimiter(code) {
    if (code === codes.dash) {
      effects.consume(code)
      return inFillerDelimiter
    }

    effects.exit('tableDelimiterFiller')

    if (code === codes.colon) {
      effects.enter('tableDelimiterAlignment')
      effects.consume(code)
      effects.exit('tableDelimiterAlignment')

      align[align.length - 1] =
        align[align.length - 1] === 'left' ? 'center' : 'right'

      return afterRightAlignment
    }

    return atDelimiterRowBreak(code)
  }

  /** @type {State} */
  function afterLeftAlignment(code) {
    if (code === codes.dash) {
      effects.enter('tableDelimiterFiller')
      effects.consume(code)
      hasDash = true
      return inFillerDelimiter
    }

    // Anything else is not ok.
    return nok(code)
  }

  /** @type {State} */
  function afterRightAlignment(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return rowEndDelimiter(code)
    }

    if (markdownSpace(code)) {
      effects.enter(types.whitespace)
      effects.consume(code)
      return inWhitespaceDelimiter
    }

    // `|`
    if (code === codes.verticalBar) {
      effects.enter('tableCellDivider')
      effects.consume(code)
      effects.exit('tableCellDivider')
      return atDelimiterRowBreak
    }

    return nok(code)
  }

  /** @type {State} */
  function rowEndDelimiter(code) {
    effects.exit('tableDelimiterRow')

    // Exit if there was no dash at all, or if the header cell count is not the
    // delimiter cell count.
    if (!hasDash || tableHeaderCount !== align.length) {
      return nok(code)
    }

    if (code === codes.eof) {
      return tableClose(code)
    }

    assert(markdownLineEnding(code), 'expected eol')
    return effects.check(
      nextPrefixedOrBlank,
      tableClose,
      effects.attempt(
        {tokenize: tokenizeRowEnd, partial: true},
        factorySpace(effects, bodyStart, types.linePrefix, constants.tabSize),
        tableClose
      )
    )(code)
  }

  /** @type {State} */
  function tableClose(code) {
    effects.exit('table')
    return ok(code)
  }

  /** @type {State} */
  function bodyStart(code) {
    effects.enter('tableBody')
    return rowStartBody(code)
  }

  /** @type {State} */
  function rowStartBody(code) {
    effects.enter('tableRow')

    // If we start with a pipe, we open a cell marker.
    if (code === codes.verticalBar) {
      return cellDividerBody(code)
    }

    effects.enter('temporaryTableCellContent')
    // Can’t be space or eols at the start of a construct, so we’re in a cell.
    return inCellContentBody(code)
  }

  /** @type {State} */
  function cellDividerBody(code) {
    assert(code === codes.verticalBar, 'expected `|`')
    effects.enter('tableCellDivider')
    effects.consume(code)
    effects.exit('tableCellDivider')
    return cellBreakBody
  }

  /** @type {State} */
  function cellBreakBody(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return atRowEndBody(code)
    }

    if (markdownSpace(code)) {
      effects.enter(types.whitespace)
      effects.consume(code)
      return inWhitespaceBody
    }

    // `|`
    if (code === codes.verticalBar) {
      return cellDividerBody(code)
    }

    // Anything else is cell content.
    effects.enter('temporaryTableCellContent')
    return inCellContentBody(code)
  }

  /** @type {State} */
  function inWhitespaceBody(code) {
    if (markdownSpace(code)) {
      effects.consume(code)
      return inWhitespaceBody
    }

    effects.exit(types.whitespace)
    return cellBreakBody(code)
  }

  /** @type {State} */
  function inCellContentBody(code) {
    // EOF, whitespace, pipe
    if (
      code === codes.eof ||
      code === codes.verticalBar ||
      markdownLineEndingOrSpace(code)
    ) {
      effects.exit('temporaryTableCellContent')
      return cellBreakBody(code)
    }

    effects.consume(code)
    return code === codes.backslash
      ? inCellContentEscapeBody
      : inCellContentBody
  }

  /** @type {State} */
  function inCellContentEscapeBody(code) {
    if (code === codes.backslash || code === codes.verticalBar) {
      effects.consume(code)
      return inCellContentBody
    }

    // Anything else.
    return inCellContentBody(code)
  }

  /** @type {State} */
  function atRowEndBody(code) {
    effects.exit('tableRow')

    if (code === codes.eof) {
      return tableBodyClose(code)
    }

    return effects.check(
      nextPrefixedOrBlank,
      tableBodyClose,
      effects.attempt(
        {tokenize: tokenizeRowEnd, partial: true},
        factorySpace(
          effects,
          rowStartBody,
          types.linePrefix,
          constants.tabSize
        ),
        tableBodyClose
      )
    )(code)
  }

  /** @type {State} */
  function tableBodyClose(code) {
    effects.exit('tableBody')
    return tableClose(code)
  }

  /** @type {Tokenizer} */
  function tokenizeRowEnd(effects, ok, nok) {
    return start

    /** @type {State} */
    function start(code) {
      assert(markdownLineEnding(code), 'expected eol')
      effects.enter(types.lineEnding)
      effects.consume(code)
      effects.exit(types.lineEnding)
      return lineStart
    }

    /** @type {State} */
    function lineStart(code) {
      return self.parser.lazy[self.now().line] ? nok(code) : ok(code)
    }
  }
}

// Based on micromark, but that won’t work as we’re in a table, and that expects
// content.
// <https://github.com/micromark/micromark/blob/main/lib/tokenize/setext-underline.js>
/** @type {Tokenizer} */
function tokenizeSetextUnderlineMini(effects, ok, nok) {
  return start

  /** @type {State} */
  function start(code) {
    if (code !== codes.dash) {
      return nok(code)
    }

    effects.enter('setextUnderline')
    return sequence(code)
  }

  /** @type {State} */
  function sequence(code) {
    if (code === codes.dash) {
      effects.consume(code)
      return sequence
    }

    return whitespace(code)
  }

  /** @type {State} */
  function whitespace(code) {
    if (code === codes.eof || markdownLineEnding(code)) {
      return ok(code)
    }

    if (markdownSpace(code)) {
      effects.consume(code)
      return whitespace
    }

    return nok(code)
  }
}

/** @type {Tokenizer} */
function tokenizeNextPrefixedOrBlank(effects, ok, nok) {
  let size = 0

  return start

  /** @type {State} */
  function start(code) {
    // This is a check, so we don’t care about tokens, but we open a bogus one
    // so we’re valid.
    effects.enter('check')
    // EOL.
    effects.consume(code)
    return whitespace
  }

  /** @type {State} */
  function whitespace(code) {
    if (code === codes.virtualSpace || code === codes.space) {
      effects.consume(code)
      size++
      return size === constants.tabSize ? ok : whitespace
    }

    // EOF or whitespace
    if (code === codes.eof || markdownLineEndingOrSpace(code)) {
      return ok(code)
    }

    // Anything else.
    return nok(code)
  }
}
