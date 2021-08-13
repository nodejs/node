/**
 * While micromark is a lexer/tokenizer, the common case of going from markdown
 * to html is currently built in as this module, even though the parts can be
 * used separately to build ASTs, CSTs, or many other output formats.
 *
 * Having an HTML compiler built in is useful because it allows us to check for
 * compliancy to CommonMark, the de facto norm of markdown, specified in roughly
 * 600 input/output cases.
 *
 * This module has an interface that accepts lists of events instead of the
 * whole at once, however, because markdown can’t be truly streaming, we buffer
 * events before processing and outputting the final result.
 */

/**
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').CompileOptions} CompileOptions
 * @typedef {import('micromark-util-types').CompileData} CompileData
 * @typedef {import('micromark-util-types').CompileContext} CompileContext
 * @typedef {import('micromark-util-types').Compile} Compile
 * @typedef {import('micromark-util-types').Handle} Handle
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 * @typedef {import('micromark-util-types').NormalizedHtmlExtension} NormalizedHtmlExtension
 */

/**
 * @typedef Media
 * @property {boolean} [image]
 * @property {string} [labelId]
 * @property {string} [label]
 * @property {string} [referenceId]
 * @property {string} [destination]
 * @property {string} [title]
 *
 * @typedef Definition
 * @property {string} [destination]
 * @property {string} [title]
 */
import {decodeEntity} from 'parse-entities/decode-entity.js'
import {combineHtmlExtensions} from 'micromark-util-combine-extensions'
import {push} from 'micromark-util-chunked'
import {decodeNumericCharacterReference} from 'micromark-util-decode-numeric-character-reference'
import {encode as _encode} from 'micromark-util-encode'
import {normalizeIdentifier} from 'micromark-util-normalize-identifier'
import {sanitizeUri} from 'micromark-util-sanitize-uri'
const hasOwnProperty = {}.hasOwnProperty
/**
 * These two are allowlists of safe protocols for full URLs in respectively the
 * `href` (on `<a>`) and `src` (on `<img>`) attributes.
 * They are based on what is allowed on GitHub,
 * <https://github.com/syntax-tree/hast-util-sanitize/blob/9275b21/lib/github.json#L31>
 */

const protocolHref = /^(https?|ircs?|mailto|xmpp)$/i
const protocolSrc = /^https?$/i
/**
 * @param {CompileOptions} [options]
 * @returns {Compile}
 */

export function compile(options = {}) {
  /**
   * Tags is needed because according to markdown, links and emphasis and
   * whatnot can exist in images, however, as HTML doesn’t allow content in
   * images, the tags are ignored in the `alt` attribute, but the content
   * remains.
   *
   * @type {boolean|undefined}
   */
  let tags = true
  /**
   * An object to track identifiers to media (URLs and titles) defined with
   * definitions.
   *
   * @type {Record<string, Definition>}
   */

  const definitions = {}
  /**
   * A lot of the handlers need to capture some of the output data, modify it
   * somehow, and then deal with it.
   * We do that by tracking a stack of buffers, that can be opened (with
   * `buffer`) and closed (with `resume`) to access them.
   *
   * @type {string[][]}
   */

  const buffers = [[]]
  /**
   * As we can have links in images and the other way around, where the deepest
   * ones are closed first, we need to track which one we’re in.
   *
   * @type {Media[]}
   */

  const mediaStack = []
  /**
   * Same as `mediaStack` for tightness, which is specific to lists.
   * We need to track if we’re currently in a tight or loose container.
   *
   * @type {boolean[]}
   */

  const tightStack = []
  /** @type {HtmlExtension} */

  const defaultHandlers = {
    enter: {
      blockQuote: onenterblockquote,
      codeFenced: onentercodefenced,
      codeFencedFenceInfo: buffer,
      codeFencedFenceMeta: buffer,
      codeIndented: onentercodeindented,
      codeText: onentercodetext,
      content: onentercontent,
      definition: onenterdefinition,
      definitionDestinationString: onenterdefinitiondestinationstring,
      definitionLabelString: buffer,
      definitionTitleString: buffer,
      emphasis: onenteremphasis,
      htmlFlow: onenterhtmlflow,
      htmlText: onenterhtml,
      image: onenterimage,
      label: buffer,
      link: onenterlink,
      listItemMarker: onenterlistitemmarker,
      listItemValue: onenterlistitemvalue,
      listOrdered: onenterlistordered,
      listUnordered: onenterlistunordered,
      paragraph: onenterparagraph,
      reference: buffer,
      resource: onenterresource,
      resourceDestinationString: onenterresourcedestinationstring,
      resourceTitleString: buffer,
      setextHeading: onentersetextheading,
      strong: onenterstrong
    },
    exit: {
      atxHeading: onexitatxheading,
      atxHeadingSequence: onexitatxheadingsequence,
      autolinkEmail: onexitautolinkemail,
      autolinkProtocol: onexitautolinkprotocol,
      blockQuote: onexitblockquote,
      characterEscapeValue: onexitdata,
      characterReferenceMarkerHexadecimal: onexitcharacterreferencemarker,
      characterReferenceMarkerNumeric: onexitcharacterreferencemarker,
      characterReferenceValue: onexitcharacterreferencevalue,
      codeFenced: onexitflowcode,
      codeFencedFence: onexitcodefencedfence,
      codeFencedFenceInfo: onexitcodefencedfenceinfo,
      codeFencedFenceMeta: resume,
      codeFlowValue: onexitcodeflowvalue,
      codeIndented: onexitflowcode,
      codeText: onexitcodetext,
      codeTextData: onexitdata,
      data: onexitdata,
      definition: onexitdefinition,
      definitionDestinationString: onexitdefinitiondestinationstring,
      definitionLabelString: onexitdefinitionlabelstring,
      definitionTitleString: onexitdefinitiontitlestring,
      emphasis: onexitemphasis,
      hardBreakEscape: onexithardbreak,
      hardBreakTrailing: onexithardbreak,
      htmlFlow: onexithtml,
      htmlFlowData: onexitdata,
      htmlText: onexithtml,
      htmlTextData: onexitdata,
      image: onexitmedia,
      label: onexitlabel,
      labelText: onexitlabeltext,
      lineEnding: onexitlineending,
      link: onexitmedia,
      listOrdered: onexitlistordered,
      listUnordered: onexitlistunordered,
      paragraph: onexitparagraph,
      reference: resume,
      referenceString: onexitreferencestring,
      resource: resume,
      resourceDestinationString: onexitresourcedestinationstring,
      resourceTitleString: onexitresourcetitlestring,
      setextHeading: onexitsetextheading,
      setextHeadingLineSequence: onexitsetextheadinglinesequence,
      setextHeadingText: onexitsetextheadingtext,
      strong: onexitstrong,
      thematicBreak: onexitthematicbreak
    }
  }
  /**
   * Combine the HTML extensions with the default handlers.
   * An HTML extension is an object whose fields are either `enter` or `exit`
   * (reflecting whether a token is entered or exited).
   * The values at such objects are names of tokens mapping to handlers.
   * Handlers are called, respectively when a token is opener or closed, with
   * that token, and a context as `this`.
   *
   * @type {NormalizedHtmlExtension}
   */
  // @ts-expect-error `defaultHandlers` is full, so the result will be too.

  const handlers = combineHtmlExtensions(
    [defaultHandlers].concat(options.htmlExtensions || [])
  )
  /**
   * Handlers do often need to keep track of some state.
   * That state is provided here as a key-value store (an object).
   *
   * @type {CompileData}
   */

  const data = {
    tightStack
  }
  /**
   * The context for handlers references a couple of useful functions.
   * In handlers from extensions, those can be accessed at `this`.
   * For the handlers here, they can be accessed directly.
   *
   * @type {Omit<CompileContext, 'sliceSerialize'>}
   */

  const context = {
    lineEndingIfNeeded,
    options,
    encode,
    raw,
    tag,
    buffer,
    resume,
    setData,
    getData
  }
  /**
   * Generally, micromark copies line endings (`'\r'`, `'\n'`, `'\r\n'`) in the
   * markdown document over to the compiled HTML.
   * In some cases, such as `> a`, CommonMark requires that extra line endings
   * are added: `<blockquote>\n<p>a</p>\n</blockquote>`.
   * This variable hold the default line ending when given (or `undefined`),
   * and in the latter case will be updated to the first found line ending if
   * there is one.
   */

  let lineEndingStyle = options.defaultLineEnding // Return the function that handles a slice of events.

  return compile
  /**
   * Deal w/ a slice of events.
   * Return either the empty string if there’s nothing of note to return, or the
   * result when done.
   *
   * @param {Event[]} events
   * @returns {string}
   */

  function compile(events) {
    let index = -1
    let start = 0
    /** @type {number[]} */

    const listStack = [] // As definitions can come after references, we need to figure out the media
    // (urls and titles) defined by them before handling the references.
    // So, we do sort of what HTML does: put metadata at the start (in head), and
    // then put content after (`body`).

    /** @type {Event[]} */

    let head = []
    /** @type {Event[]} */

    let body = []

    while (++index < events.length) {
      // Figure out the line ending style used in the document.
      if (
        !lineEndingStyle &&
        (events[index][1].type === 'lineEnding' ||
          events[index][1].type === 'lineEndingBlank')
      ) {
        // @ts-expect-error Hush, it’s a line ending.
        lineEndingStyle = events[index][2].sliceSerialize(events[index][1])
      } // Preprocess lists to infer whether the list is loose or not.

      if (
        events[index][1].type === 'listOrdered' ||
        events[index][1].type === 'listUnordered'
      ) {
        if (events[index][0] === 'enter') {
          listStack.push(index)
        } else {
          prepareList(events.slice(listStack.pop(), index))
        }
      } // Move definitions to the front.

      if (events[index][1].type === 'definition') {
        if (events[index][0] === 'enter') {
          body = push(body, events.slice(start, index))
          start = index
        } else {
          head = push(head, events.slice(start, index + 1))
          start = index + 1
        }
      }
    }

    head = push(head, body)
    head = push(head, events.slice(start))
    index = -1
    const result = head // Handle the start of the document, if defined.

    if (handlers.enter.null) {
      handlers.enter.null.call(context)
    } // Handle all events.

    while (++index < events.length) {
      const handler = handlers[result[index][0]]

      if (hasOwnProperty.call(handler, result[index][1].type)) {
        handler[result[index][1].type].call(
          Object.assign(
            {
              sliceSerialize: result[index][2].sliceSerialize
            },
            context
          ),
          result[index][1]
        )
      }
    } // Handle the end of the document, if defined.

    if (handlers.exit.null) {
      handlers.exit.null.call(context)
    }

    return buffers[0].join('')
  }
  /**
   * Figure out whether lists are loose or not.
   *
   * @param {Event[]} slice
   * @returns {void}
   */

  function prepareList(slice) {
    const length = slice.length
    let index = 0 // Skip open.

    let containerBalance = 0
    let loose = false
    /** @type {boolean|undefined} */

    let atMarker

    while (++index < length) {
      const event = slice[index]

      if (event[1]._container) {
        atMarker = undefined

        if (event[0] === 'enter') {
          containerBalance++
        } else {
          containerBalance--
        }
      } else
        switch (event[1].type) {
          case 'listItemPrefix': {
            if (event[0] === 'exit') {
              atMarker = true
            }

            break
          }

          case 'linePrefix': {
            // Ignore
            break
          }

          case 'lineEndingBlank': {
            if (event[0] === 'enter' && !containerBalance) {
              if (atMarker) {
                atMarker = undefined
              } else {
                loose = true
              }
            }

            break
          }

          default: {
            atMarker = undefined
          }
        }
    }

    slice[0][1]._loose = loose
  }
  /**
   * @type {CompileContext['setData']}
   * @param [value]
   */

  function setData(key, value) {
    data[key] = value
  }
  /**
   * @type {CompileContext['getData']}
   * @template {string} K
   * @param {K} key
   * @returns {CompileData[K]}
   */

  function getData(key) {
    return data[key]
  }
  /** @type {CompileContext['buffer']} */

  function buffer() {
    buffers.push([])
  }
  /** @type {CompileContext['resume']} */

  function resume() {
    const buf = buffers.pop()
    return buf.join('')
  }
  /** @type {CompileContext['tag']} */

  function tag(value) {
    if (!tags) return
    setData('lastWasTag', true)
    buffers[buffers.length - 1].push(value)
  }
  /** @type {CompileContext['raw']} */

  function raw(value) {
    setData('lastWasTag')
    buffers[buffers.length - 1].push(value)
  }
  /**
   * Output an extra line ending.
   *
   * @returns {void}
   */

  function lineEnding() {
    raw(lineEndingStyle || '\n')
  }
  /** @type {CompileContext['lineEndingIfNeeded']} */

  function lineEndingIfNeeded() {
    const buffer = buffers[buffers.length - 1]
    const slice = buffer[buffer.length - 1]
    const previous = slice ? slice.charCodeAt(slice.length - 1) : null

    if (previous === 10 || previous === 13 || previous === null) {
      return
    }

    lineEnding()
  }
  /** @type {CompileContext['encode']} */

  function encode(value) {
    return getData('ignoreEncode') ? value : _encode(value)
  } //
  // Handlers.
  //

  /** @type {Handle} */

  function onenterlistordered(token) {
    tightStack.push(!token._loose)
    lineEndingIfNeeded()
    tag('<ol')
    setData('expectFirstItem', true)
  }
  /** @type {Handle} */

  function onenterlistunordered(token) {
    tightStack.push(!token._loose)
    lineEndingIfNeeded()
    tag('<ul')
    setData('expectFirstItem', true)
  }
  /** @type {Handle} */

  function onenterlistitemvalue(token) {
    if (getData('expectFirstItem')) {
      const value = Number.parseInt(this.sliceSerialize(token), 10)

      if (value !== 1) {
        tag(' start="' + encode(String(value)) + '"')
      }
    }
  }
  /** @type {Handle} */

  function onenterlistitemmarker() {
    if (getData('expectFirstItem')) {
      tag('>')
    } else {
      onexitlistitem()
    }

    lineEndingIfNeeded()
    tag('<li>')
    setData('expectFirstItem') // “Hack” to prevent a line ending from showing up if the item is empty.

    setData('lastWasTag')
  }
  /** @type {Handle} */

  function onexitlistordered() {
    onexitlistitem()
    tightStack.pop()
    lineEnding()
    tag('</ol>')
  }
  /** @type {Handle} */

  function onexitlistunordered() {
    onexitlistitem()
    tightStack.pop()
    lineEnding()
    tag('</ul>')
  }
  /** @type {Handle} */

  function onexitlistitem() {
    if (getData('lastWasTag') && !getData('slurpAllLineEndings')) {
      lineEndingIfNeeded()
    }

    tag('</li>')
    setData('slurpAllLineEndings')
  }
  /** @type {Handle} */

  function onenterblockquote() {
    tightStack.push(false)
    lineEndingIfNeeded()
    tag('<blockquote>')
  }
  /** @type {Handle} */

  function onexitblockquote() {
    tightStack.pop()
    lineEndingIfNeeded()
    tag('</blockquote>')
    setData('slurpAllLineEndings')
  }
  /** @type {Handle} */

  function onenterparagraph() {
    if (!tightStack[tightStack.length - 1]) {
      lineEndingIfNeeded()
      tag('<p>')
    }

    setData('slurpAllLineEndings')
  }
  /** @type {Handle} */

  function onexitparagraph() {
    if (tightStack[tightStack.length - 1]) {
      setData('slurpAllLineEndings', true)
    } else {
      tag('</p>')
    }
  }
  /** @type {Handle} */

  function onentercodefenced() {
    lineEndingIfNeeded()
    tag('<pre><code')
    setData('fencesCount', 0)
  }
  /** @type {Handle} */

  function onexitcodefencedfenceinfo() {
    const value = resume()
    tag(' class="language-' + value + '"')
  }
  /** @type {Handle} */

  function onexitcodefencedfence() {
    const count = getData('fencesCount') || 0

    if (!count) {
      tag('>')
      setData('slurpOneLineEnding', true)
    }

    setData('fencesCount', count + 1)
  }
  /** @type {Handle} */

  function onentercodeindented() {
    lineEndingIfNeeded()
    tag('<pre><code>')
  }
  /** @type {Handle} */

  function onexitflowcode() {
    const count = getData('fencesCount') // One special case is if we are inside a container, and the fenced code was
    // not closed (meaning it runs to the end).
    // In that case, the following line ending, is considered *outside* the
    // fenced code and block quote by micromark, but CM wants to treat that
    // ending as part of the code.

    if (
      count !== undefined &&
      count < 2 && // @ts-expect-error `tightStack` is always set.
      data.tightStack.length > 0 &&
      !getData('lastWasTag')
    ) {
      lineEnding()
    } // But in most cases, it’s simpler: when we’ve seen some data, emit an extra
    // line ending when needed.

    if (getData('flowCodeSeenData')) {
      lineEndingIfNeeded()
    }

    tag('</code></pre>')
    if (count !== undefined && count < 2) lineEndingIfNeeded()
    setData('flowCodeSeenData')
    setData('fencesCount')
    setData('slurpOneLineEnding')
  }
  /** @type {Handle} */

  function onenterimage() {
    mediaStack.push({
      image: true
    })
    tags = undefined // Disallow tags.
  }
  /** @type {Handle} */

  function onenterlink() {
    mediaStack.push({})
  }
  /** @type {Handle} */

  function onexitlabeltext(token) {
    mediaStack[mediaStack.length - 1].labelId = this.sliceSerialize(token)
  }
  /** @type {Handle} */

  function onexitlabel() {
    mediaStack[mediaStack.length - 1].label = resume()
  }
  /** @type {Handle} */

  function onexitreferencestring(token) {
    mediaStack[mediaStack.length - 1].referenceId = this.sliceSerialize(token)
  }
  /** @type {Handle} */

  function onenterresource() {
    buffer() // We can have line endings in the resource, ignore them.

    mediaStack[mediaStack.length - 1].destination = ''
  }
  /** @type {Handle} */

  function onenterresourcedestinationstring() {
    buffer() // Ignore encoding the result, as we’ll first percent encode the url and
    // encode manually after.

    setData('ignoreEncode', true)
  }
  /** @type {Handle} */

  function onexitresourcedestinationstring() {
    mediaStack[mediaStack.length - 1].destination = resume()
    setData('ignoreEncode')
  }
  /** @type {Handle} */

  function onexitresourcetitlestring() {
    mediaStack[mediaStack.length - 1].title = resume()
  }
  /** @type {Handle} */

  function onexitmedia() {
    let index = mediaStack.length - 1 // Skip current.

    const media = mediaStack[index]
    const id = media.referenceId || media.labelId
    const context =
      media.destination === undefined
        ? definitions[normalizeIdentifier(id)]
        : media
    tags = true

    while (index--) {
      if (mediaStack[index].image) {
        tags = undefined
        break
      }
    }

    if (media.image) {
      tag(
        '<img src="' +
          sanitizeUri(
            context.destination,
            options.allowDangerousProtocol ? undefined : protocolSrc
          ) +
          '" alt="'
      )
      raw(media.label)
      tag('"')
    } else {
      tag(
        '<a href="' +
          sanitizeUri(
            context.destination,
            options.allowDangerousProtocol ? undefined : protocolHref
          ) +
          '"'
      )
    }

    tag(context.title ? ' title="' + context.title + '"' : '')

    if (media.image) {
      tag(' />')
    } else {
      tag('>')
      raw(media.label)
      tag('</a>')
    }

    mediaStack.pop()
  }
  /** @type {Handle} */

  function onenterdefinition() {
    buffer()
    mediaStack.push({})
  }
  /** @type {Handle} */

  function onexitdefinitionlabelstring(token) {
    // Discard label, use the source content instead.
    resume()
    mediaStack[mediaStack.length - 1].labelId = this.sliceSerialize(token)
  }
  /** @type {Handle} */

  function onenterdefinitiondestinationstring() {
    buffer()
    setData('ignoreEncode', true)
  }
  /** @type {Handle} */

  function onexitdefinitiondestinationstring() {
    mediaStack[mediaStack.length - 1].destination = resume()
    setData('ignoreEncode')
  }
  /** @type {Handle} */

  function onexitdefinitiontitlestring() {
    mediaStack[mediaStack.length - 1].title = resume()
  }
  /** @type {Handle} */

  function onexitdefinition() {
    const media = mediaStack[mediaStack.length - 1]
    const id = normalizeIdentifier(media.labelId)
    resume()

    if (!hasOwnProperty.call(definitions, id)) {
      definitions[id] = mediaStack[mediaStack.length - 1]
    }

    mediaStack.pop()
  }
  /** @type {Handle} */

  function onentercontent() {
    setData('slurpAllLineEndings', true)
  }
  /** @type {Handle} */

  function onexitatxheadingsequence(token) {
    // Exit for further sequences.
    if (getData('headingRank')) return
    setData('headingRank', this.sliceSerialize(token).length)
    lineEndingIfNeeded()
    tag('<h' + getData('headingRank') + '>')
  }
  /** @type {Handle} */

  function onentersetextheading() {
    buffer()
    setData('slurpAllLineEndings')
  }
  /** @type {Handle} */

  function onexitsetextheadingtext() {
    setData('slurpAllLineEndings', true)
  }
  /** @type {Handle} */

  function onexitatxheading() {
    tag('</h' + getData('headingRank') + '>')
    setData('headingRank')
  }
  /** @type {Handle} */

  function onexitsetextheadinglinesequence(token) {
    setData(
      'headingRank',
      this.sliceSerialize(token).charCodeAt(0) === 61 ? 1 : 2
    )
  }
  /** @type {Handle} */

  function onexitsetextheading() {
    const value = resume()
    lineEndingIfNeeded()
    tag('<h' + getData('headingRank') + '>')
    raw(value)
    tag('</h' + getData('headingRank') + '>')
    setData('slurpAllLineEndings')
    setData('headingRank')
  }
  /** @type {Handle} */

  function onexitdata(token) {
    raw(encode(this.sliceSerialize(token)))
  }
  /** @type {Handle} */

  function onexitlineending(token) {
    if (getData('slurpAllLineEndings')) {
      return
    }

    if (getData('slurpOneLineEnding')) {
      setData('slurpOneLineEnding')
      return
    }

    if (getData('inCodeText')) {
      raw(' ')
      return
    }

    raw(encode(this.sliceSerialize(token)))
  }
  /** @type {Handle} */

  function onexitcodeflowvalue(token) {
    raw(encode(this.sliceSerialize(token)))
    setData('flowCodeSeenData', true)
  }
  /** @type {Handle} */

  function onexithardbreak() {
    tag('<br />')
  }
  /** @type {Handle} */

  function onenterhtmlflow() {
    lineEndingIfNeeded()
    onenterhtml()
  }
  /** @type {Handle} */

  function onexithtml() {
    setData('ignoreEncode')
  }
  /** @type {Handle} */

  function onenterhtml() {
    if (options.allowDangerousHtml) {
      setData('ignoreEncode', true)
    }
  }
  /** @type {Handle} */

  function onenteremphasis() {
    tag('<em>')
  }
  /** @type {Handle} */

  function onenterstrong() {
    tag('<strong>')
  }
  /** @type {Handle} */

  function onentercodetext() {
    setData('inCodeText', true)
    tag('<code>')
  }
  /** @type {Handle} */

  function onexitcodetext() {
    setData('inCodeText')
    tag('</code>')
  }
  /** @type {Handle} */

  function onexitemphasis() {
    tag('</em>')
  }
  /** @type {Handle} */

  function onexitstrong() {
    tag('</strong>')
  }
  /** @type {Handle} */

  function onexitthematicbreak() {
    lineEndingIfNeeded()
    tag('<hr />')
  }
  /** @type {Handle} */

  function onexitcharacterreferencemarker(token) {
    setData('characterReferenceType', token.type)
  }
  /** @type {Handle} */

  function onexitcharacterreferencevalue(token) {
    let value = this.sliceSerialize(token) // @ts-expect-error `decodeEntity` can return false for invalid named
    // character references, but everything we’ve tokenized is valid.

    value = getData('characterReferenceType')
      ? decodeNumericCharacterReference(
          value,
          getData('characterReferenceType') ===
            'characterReferenceMarkerNumeric'
            ? 10
            : 16
        )
      : decodeEntity(value)
    raw(encode(value))
    setData('characterReferenceType')
  }
  /** @type {Handle} */

  function onexitautolinkprotocol(token) {
    const uri = this.sliceSerialize(token)
    tag(
      '<a href="' +
        sanitizeUri(
          uri,
          options.allowDangerousProtocol ? undefined : protocolHref
        ) +
        '">'
    )
    raw(encode(uri))
    tag('</a>')
  }
  /** @type {Handle} */

  function onexitautolinkemail(token) {
    const uri = this.sliceSerialize(token)
    tag('<a href="' + sanitizeUri('mailto:' + uri) + '">')
    raw(encode(uri))
    tag('</a>')
  }
}
