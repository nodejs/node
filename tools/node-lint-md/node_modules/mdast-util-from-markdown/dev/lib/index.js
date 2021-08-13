/**
 * @typedef {import('micromark-util-types').Encoding} Encoding
 * @typedef {import('micromark-util-types').Event} Event
 * @typedef {import('micromark-util-types').ParseOptions} ParseOptions
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').Value} Value
 * @typedef {Root|Root['children'][number]} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist').Point} Point
 * @typedef {import('mdast').Break} Break
 * @typedef {import('mdast').Blockquote} Blockquote
 * @typedef {import('mdast').Code} Code
 * @typedef {import('mdast').Definition} Definition
 * @typedef {import('mdast').Emphasis} Emphasis
 * @typedef {import('mdast').Heading} Heading
 * @typedef {import('mdast').HTML} HTML
 * @typedef {import('mdast').Image} Image
 * @typedef {import('mdast').InlineCode} InlineCode
 * @typedef {import('mdast').Link} Link
 * @typedef {import('mdast').List} List
 * @typedef {import('mdast').ListItem} ListItem
 * @typedef {import('mdast').Paragraph} Paragraph
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Strong} Strong
 * @typedef {import('mdast').Text} Text
 * @typedef {import('mdast').ThematicBreak} ThematicBreak
 */

/**
 * @typedef _CompileDataFields
 * @property {boolean|undefined} expectingFirstListItemValue
 * @property {boolean|undefined} flowCodeInside
 * @property {boolean|undefined} setextHeadingSlurpLineEnding
 * @property {boolean|undefined} atHardBreak
 * @property {'collapsed'|'full'} referenceType
 * @property {boolean|undefined} inReference
 * @property {'characterReferenceMarkerHexadecimal'|'characterReferenceMarkerNumeric'} characterReferenceType
 *
 * @typedef {Record<string, unknown> & Partial<_CompileDataFields>} CompileData
 *
 * @typedef {(tree: Root) => Root|void} Transform
 * @typedef {(this: CompileContext, token: Token) => void} Handle
 * @typedef {Record<string, Handle>} Handles
 *   Token types mapping to handles
 * @typedef {Record<string, Record<string, unknown>|Array.<unknown>> & {canContainEols: Array.<string>, transforms: Array.<Transform>, enter: Handles, exit: Handles}} NormalizedExtension
 * @typedef {Partial<NormalizedExtension>} Extension
 *   An mdast extension changes how markdown tokens are turned into mdast.
 *
 * @typedef CompileContext
 *   mdast compiler context
 * @property {Array.<Node>} stack
 * @property {Array.<Token>} tokenStack
 * @property {(key: string, value?: unknown) => void} setData
 *   Set data into the key-value store.
 * @property {<K extends string>(key: K) => CompileData[K]} getData
 *   Get data from the key-value store.
 * @property {(this: CompileContext) => void} buffer
 *   Capture some of the output data.
 * @property {(this: CompileContext) => string} resume
 *   Stop capturing and access the output data.
 * @property {<N extends Node>(this: CompileContext, node: N, token: Token) => N} enter
 *   Enter a token.
 * @property {(this: CompileContext, token: Token) => Node} exit
 *   Exit a token.
 * @property {TokenizeContext['sliceSerialize']} sliceSerialize
 *   Get the string value of a token.
 * @property {NormalizedExtension} config
 *   Configuration.
 *
 * @typedef {{mdastExtensions?: Array.<Extension|Array.<Extension>>}} FromMarkdownOptions
 * @typedef {ParseOptions & FromMarkdownOptions} Options
 */

import assert from 'assert'
import {toString} from 'mdast-util-to-string'
import {parse} from 'micromark/lib/parse.js'
import {preprocess} from 'micromark/lib/preprocess.js'
import {postprocess} from 'micromark/lib/postprocess.js'
import {decodeNumericCharacterReference} from 'micromark-util-decode-numeric-character-reference'
import {normalizeIdentifier} from 'micromark-util-normalize-identifier'
import {codes} from 'micromark-util-symbol/codes.js'
import {constants} from 'micromark-util-symbol/constants.js'
import {types} from 'micromark-util-symbol/types.js'
import {decodeEntity} from 'parse-entities/decode-entity.js'
import {stringifyPosition} from 'unist-util-stringify-position'

const own = {}.hasOwnProperty

/**
 * @param value Markdown to parse (`string` or `Buffer`).
 * @param [encoding] Character encoding to understand `value` as when it’s a `Buffer` (`string`, default: `'utf8'`).
 * @param [options] Configuration
 */
export const fromMarkdown =
  /**
   * @type {(
   *   ((value: Value, encoding: Encoding, options?: Options) => Root) &
   *   ((value: Value, options?: Options) => Root)
   * )}
   */
  (
    /**
     * @param {Value} value
     * @param {Encoding} [encoding]
     * @param {Options} [options]
     * @returns {Root}
     */
    function (value, encoding, options) {
      if (typeof encoding !== 'string') {
        options = encoding
        encoding = undefined
      }

      return compiler(options)(
        postprocess(
          parse(options).document().write(preprocess()(value, encoding, true))
        )
      )
    }
  )

/**
 * Note this compiler only understand complete buffering, not streaming.
 *
 * @param {Options} [options]
 */
function compiler(options = {}) {
  /** @type {NormalizedExtension} */
  // @ts-expect-error: our base has all required fields, so the result will too.
  const config = configure(
    {
      transforms: [],
      canContainEols: [
        'emphasis',
        'fragment',
        'heading',
        'paragraph',
        'strong'
      ],
      enter: {
        autolink: opener(link),
        autolinkProtocol: onenterdata,
        autolinkEmail: onenterdata,
        atxHeading: opener(heading),
        blockQuote: opener(blockQuote),
        characterEscape: onenterdata,
        characterReference: onenterdata,
        codeFenced: opener(codeFlow),
        codeFencedFenceInfo: buffer,
        codeFencedFenceMeta: buffer,
        codeIndented: opener(codeFlow, buffer),
        codeText: opener(codeText, buffer),
        codeTextData: onenterdata,
        data: onenterdata,
        codeFlowValue: onenterdata,
        definition: opener(definition),
        definitionDestinationString: buffer,
        definitionLabelString: buffer,
        definitionTitleString: buffer,
        emphasis: opener(emphasis),
        hardBreakEscape: opener(hardBreak),
        hardBreakTrailing: opener(hardBreak),
        htmlFlow: opener(html, buffer),
        htmlFlowData: onenterdata,
        htmlText: opener(html, buffer),
        htmlTextData: onenterdata,
        image: opener(image),
        label: buffer,
        link: opener(link),
        listItem: opener(listItem),
        listItemValue: onenterlistitemvalue,
        listOrdered: opener(list, onenterlistordered),
        listUnordered: opener(list),
        paragraph: opener(paragraph),
        reference: onenterreference,
        referenceString: buffer,
        resourceDestinationString: buffer,
        resourceTitleString: buffer,
        setextHeading: opener(heading),
        strong: opener(strong),
        thematicBreak: opener(thematicBreak)
      },
      exit: {
        atxHeading: closer(),
        atxHeadingSequence: onexitatxheadingsequence,
        autolink: closer(),
        autolinkEmail: onexitautolinkemail,
        autolinkProtocol: onexitautolinkprotocol,
        blockQuote: closer(),
        characterEscapeValue: onexitdata,
        characterReferenceMarkerHexadecimal: onexitcharacterreferencemarker,
        characterReferenceMarkerNumeric: onexitcharacterreferencemarker,
        characterReferenceValue: onexitcharacterreferencevalue,
        codeFenced: closer(onexitcodefenced),
        codeFencedFence: onexitcodefencedfence,
        codeFencedFenceInfo: onexitcodefencedfenceinfo,
        codeFencedFenceMeta: onexitcodefencedfencemeta,
        codeFlowValue: onexitdata,
        codeIndented: closer(onexitcodeindented),
        codeText: closer(onexitcodetext),
        codeTextData: onexitdata,
        data: onexitdata,
        definition: closer(),
        definitionDestinationString: onexitdefinitiondestinationstring,
        definitionLabelString: onexitdefinitionlabelstring,
        definitionTitleString: onexitdefinitiontitlestring,
        emphasis: closer(),
        hardBreakEscape: closer(onexithardbreak),
        hardBreakTrailing: closer(onexithardbreak),
        htmlFlow: closer(onexithtmlflow),
        htmlFlowData: onexitdata,
        htmlText: closer(onexithtmltext),
        htmlTextData: onexitdata,
        image: closer(onexitimage),
        label: onexitlabel,
        labelText: onexitlabeltext,
        lineEnding: onexitlineending,
        link: closer(onexitlink),
        listItem: closer(),
        listOrdered: closer(),
        listUnordered: closer(),
        paragraph: closer(),
        referenceString: onexitreferencestring,
        resourceDestinationString: onexitresourcedestinationstring,
        resourceTitleString: onexitresourcetitlestring,
        resource: onexitresource,
        setextHeading: closer(onexitsetextheading),
        setextHeadingLineSequence: onexitsetextheadinglinesequence,
        setextHeadingText: onexitsetextheadingtext,
        strong: closer(),
        thematicBreak: closer()
      }
    },
    options.mdastExtensions || []
  )

  /** @type {CompileData} */
  const data = {}

  return compile

  /**
   * @param {Array.<Event>} events
   * @returns {Root}
   */
  function compile(events) {
    /** @type {Root} */
    let tree = {type: 'root', children: []}
    /** @type {CompileContext['stack']} */
    const stack = [tree]
    /** @type {CompileContext['tokenStack']} */
    const tokenStack = []
    /** @type {Array.<number>} */
    const listStack = []
    /** @type {Omit<CompileContext, 'sliceSerialize'>} */
    const context = {
      stack,
      tokenStack,
      config,
      enter,
      exit,
      buffer,
      resume,
      setData,
      getData
    }
    let index = -1

    while (++index < events.length) {
      // We preprocess lists to add `listItem` tokens, and to infer whether
      // items the list itself are spread out.
      if (
        events[index][1].type === types.listOrdered ||
        events[index][1].type === types.listUnordered
      ) {
        if (events[index][0] === 'enter') {
          listStack.push(index)
        } else {
          const tail = listStack.pop()
          assert(typeof tail === 'number', 'expected list ot be open')
          index = prepareList(events, tail, index)
        }
      }
    }

    index = -1

    while (++index < events.length) {
      const handler = config[events[index][0]]

      if (own.call(handler, events[index][1].type)) {
        handler[events[index][1].type].call(
          Object.assign(
            {sliceSerialize: events[index][2].sliceSerialize},
            context
          ),
          events[index][1]
        )
      }
    }

    if (tokenStack.length > 0) {
      throw new Error(
        'Cannot close document, a token (`' +
          tokenStack[tokenStack.length - 1].type +
          '`, ' +
          stringifyPosition({
            start: tokenStack[tokenStack.length - 1].start,
            end: tokenStack[tokenStack.length - 1].end
          }) +
          ') is still open'
      )
    }

    // Figure out `root` position.
    tree.position = {
      start: point(
        events.length > 0 ? events[0][1].start : {line: 1, column: 1, offset: 0}
      ),
      end: point(
        events.length > 0
          ? events[events.length - 2][1].end
          : {line: 1, column: 1, offset: 0}
      )
    }

    index = -1
    while (++index < config.transforms.length) {
      tree = config.transforms[index](tree) || tree
    }

    return tree
  }

  /**
   * @param {Array.<Event>} events
   * @param {number} start
   * @param {number} length
   * @returns {number}
   */
  function prepareList(events, start, length) {
    let index = start - 1
    let containerBalance = -1
    let listSpread = false
    /** @type {Token|undefined} */
    let listItem
    /** @type {number|undefined} */
    let lineIndex
    /** @type {number|undefined} */
    let firstBlankLineIndex
    /** @type {boolean|undefined} */
    let atMarker

    while (++index <= length) {
      const event = events[index]

      if (
        event[1].type === types.listUnordered ||
        event[1].type === types.listOrdered ||
        event[1].type === types.blockQuote
      ) {
        if (event[0] === 'enter') {
          containerBalance++
        } else {
          containerBalance--
        }

        atMarker = undefined
      } else if (event[1].type === types.lineEndingBlank) {
        if (event[0] === 'enter') {
          if (
            listItem &&
            !atMarker &&
            !containerBalance &&
            !firstBlankLineIndex
          ) {
            firstBlankLineIndex = index
          }

          atMarker = undefined
        }
      } else if (
        event[1].type === types.linePrefix ||
        event[1].type === types.listItemValue ||
        event[1].type === types.listItemMarker ||
        event[1].type === types.listItemPrefix ||
        event[1].type === types.listItemPrefixWhitespace
      ) {
        // Empty.
      } else {
        atMarker = undefined
      }

      if (
        (!containerBalance &&
          event[0] === 'enter' &&
          event[1].type === types.listItemPrefix) ||
        (containerBalance === -1 &&
          event[0] === 'exit' &&
          (event[1].type === types.listUnordered ||
            event[1].type === types.listOrdered))
      ) {
        if (listItem) {
          let tailIndex = index
          lineIndex = undefined

          while (tailIndex--) {
            const tailEvent = events[tailIndex]

            if (
              tailEvent[1].type === types.lineEnding ||
              tailEvent[1].type === types.lineEndingBlank
            ) {
              if (tailEvent[0] === 'exit') continue

              if (lineIndex) {
                events[lineIndex][1].type = types.lineEndingBlank
                listSpread = true
              }

              tailEvent[1].type = types.lineEnding
              lineIndex = tailIndex
            } else if (
              tailEvent[1].type === types.linePrefix ||
              tailEvent[1].type === types.blockQuotePrefix ||
              tailEvent[1].type === types.blockQuotePrefixWhitespace ||
              tailEvent[1].type === types.blockQuoteMarker ||
              tailEvent[1].type === types.listItemIndent
            ) {
              // Empty
            } else {
              break
            }
          }

          if (
            firstBlankLineIndex &&
            (!lineIndex || firstBlankLineIndex < lineIndex)
          ) {
            // @ts-expect-error Patched.
            listItem._spread = true
          }

          // Fix position.
          listItem.end = Object.assign(
            {},
            lineIndex ? events[lineIndex][1].start : event[1].end
          )

          events.splice(lineIndex || index, 0, ['exit', listItem, event[2]])
          index++
          length++
        }

        // Create a new list item.
        if (event[1].type === types.listItemPrefix) {
          listItem = {
            type: 'listItem',
            // @ts-expect-error Patched
            _spread: false,
            start: Object.assign({}, event[1].start)
          }
          // @ts-expect-error: `listItem` is most definitely defined, TS...
          events.splice(index, 0, ['enter', listItem, event[2]])
          index++
          length++
          firstBlankLineIndex = undefined
          atMarker = true
        }
      }
    }

    // @ts-expect-error Patched.
    events[start][1]._spread = listSpread
    return length
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

  /**
   * @param {Point} d
   * @returns {Point}
   */
  function point(d) {
    return {line: d.line, column: d.column, offset: d.offset}
  }

  /**
   * @param {(token: Token) => Node} create
   * @param {Handle} [and]
   * @returns {Handle}
   */
  function opener(create, and) {
    return open

    /**
     * @this {CompileContext}
     * @param {Token} token
     * @returns {void}
     */
    function open(token) {
      enter.call(this, create(token), token)
      if (and) and.call(this, token)
    }
  }

  /** @type {CompileContext['buffer']} */
  function buffer() {
    // @ts-expect-error: Custom node type to collect text.
    this.stack.push({type: 'fragment', children: []})
  }

  /**
   * @type {CompileContext['enter']}
   * @template {Node} N
   * @this {CompileContext}
   * @param {N} node
   * @param {Token} token
   * @returns {N}
   */
  function enter(node, token) {
    /** @type {Parent} */
    // @ts-expect-error: Assume parent.
    const parent = this.stack[this.stack.length - 1]
    assert(parent, 'expected `parent`')
    parent.children.push(node)
    this.stack.push(node)
    this.tokenStack.push(token)
    // @ts-expect-error: `end` will be patched later.
    node.position = {start: point(token.start)}
    return node
  }

  /**
   * @param {Handle} [and]
   * @returns {Handle}
   */
  function closer(and) {
    return close

    /**
     * @this {CompileContext}
     * @param {Token} token
     * @returns {void}
     */
    function close(token) {
      if (and) and.call(this, token)
      exit.call(this, token)
    }
  }

  /** @type {CompileContext['exit']} */
  function exit(token) {
    const node = this.stack.pop()
    assert(node, 'expected `node`')
    const open = this.tokenStack.pop()

    if (!open) {
      throw new Error(
        'Cannot close `' +
          token.type +
          '` (' +
          stringifyPosition({start: token.start, end: token.end}) +
          '): it’s not open'
      )
    } else if (open.type !== token.type) {
      throw new Error(
        'Cannot close `' +
          token.type +
          '` (' +
          stringifyPosition({start: token.start, end: token.end}) +
          '): a different token (`' +
          open.type +
          '`, ' +
          stringifyPosition({start: open.start, end: open.end}) +
          ') is open'
      )
    }

    assert(node.position, 'expected `position` to be defined')
    node.position.end = point(token.end)
    return node
  }

  /**
   * @this {CompileContext}
   * @returns {string}
   */
  function resume() {
    return toString(this.stack.pop())
  }

  //
  // Handlers.
  //

  /** @type {Handle} */
  function onenterlistordered() {
    setData('expectingFirstListItemValue', true)
  }

  /** @type {Handle} */
  function onenterlistitemvalue(token) {
    if (getData('expectingFirstListItemValue')) {
      this.stack[this.stack.length - 2].start = Number.parseInt(
        this.sliceSerialize(token),
        constants.numericBaseDecimal
      )
      setData('expectingFirstListItemValue')
    }
  }

  /** @type {Handle} */
  function onexitcodefencedfenceinfo() {
    const data = this.resume()
    this.stack[this.stack.length - 1].lang = data
  }

  /** @type {Handle} */
  function onexitcodefencedfencemeta() {
    const data = this.resume()
    this.stack[this.stack.length - 1].meta = data
  }

  /** @type {Handle} */
  function onexitcodefencedfence() {
    // Exit if this is the closing fence.
    if (getData('flowCodeInside')) return
    this.buffer()
    setData('flowCodeInside', true)
  }

  /** @type {Handle} */
  function onexitcodefenced() {
    const data = this.resume()

    this.stack[this.stack.length - 1].value = data.replace(
      /^(\r?\n|\r)|(\r?\n|\r)$/g,
      ''
    )

    setData('flowCodeInside')
  }

  /** @type {Handle} */
  function onexitcodeindented() {
    const data = this.resume()
    this.stack[this.stack.length - 1].value = data.replace(/(\r?\n|\r)$/g, '')
  }

  /** @type {Handle} */
  function onexitdefinitionlabelstring(token) {
    // Discard label, use the source content instead.
    const label = this.resume()
    this.stack[this.stack.length - 1].label = label
    this.stack[this.stack.length - 1].identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase()
  }

  /** @type {Handle} */
  function onexitdefinitiontitlestring() {
    const data = this.resume()
    this.stack[this.stack.length - 1].title = data
  }

  /** @type {Handle} */
  function onexitdefinitiondestinationstring() {
    const data = this.resume()
    this.stack[this.stack.length - 1].url = data
  }

  /** @type {Handle} */
  function onexitatxheadingsequence(token) {
    if (!this.stack[this.stack.length - 1].depth) {
      this.stack[this.stack.length - 1].depth =
        this.sliceSerialize(token).length
    }
  }

  /** @type {Handle} */
  function onexitsetextheadingtext() {
    setData('setextHeadingSlurpLineEnding', true)
  }

  /** @type {Handle} */
  function onexitsetextheadinglinesequence(token) {
    this.stack[this.stack.length - 1].depth =
      this.sliceSerialize(token).charCodeAt(0) === codes.equalsTo ? 1 : 2
  }

  /** @type {Handle} */
  function onexitsetextheading() {
    setData('setextHeadingSlurpLineEnding')
  }

  /** @type {Handle} */
  function onenterdata(token) {
    /** @type {Parent} */
    // @ts-expect-error: assume parent.
    const parent = this.stack[this.stack.length - 1]
    /** @type {Node} */
    // @ts-expect-error: assume child.
    let tail = parent.children[parent.children.length - 1]

    if (!tail || tail.type !== 'text') {
      // Add a new text node.
      tail = text()
      // @ts-expect-error: we’ll add `end` later.
      tail.position = {start: point(token.start)}
      parent.children.push(tail)
    }

    this.stack.push(tail)
  }

  /** @type {Handle} */
  function onexitdata(token) {
    const tail = this.stack.pop()
    assert(tail, 'expected a `node` to be on the stack')
    assert(tail.position, 'expected `node` to have an open position')
    tail.value += this.sliceSerialize(token)
    tail.position.end = point(token.end)
  }

  /** @type {Handle} */
  function onexitlineending(token) {
    /** @type {Parent} */
    // @ts-expect-error: supposed to be a parent.
    const context = this.stack[this.stack.length - 1]
    assert(context, 'expected `node`')

    // If we’re at a hard break, include the line ending in there.
    if (getData('atHardBreak')) {
      const tail = context.children[context.children.length - 1]
      assert(tail.position, 'expected tail to have a starting position')
      tail.position.end = point(token.end)
      setData('atHardBreak')
      return
    }

    if (
      !getData('setextHeadingSlurpLineEnding') &&
      config.canContainEols.includes(context.type)
    ) {
      onenterdata.call(this, token)
      onexitdata.call(this, token)
    }
  }

  /** @type {Handle} */
  function onexithardbreak() {
    setData('atHardBreak', true)
  }

  /** @type {Handle} */
  function onexithtmlflow() {
    const data = this.resume()
    this.stack[this.stack.length - 1].value = data
  }

  /** @type {Handle} */
  function onexithtmltext() {
    const data = this.resume()
    this.stack[this.stack.length - 1].value = data
  }

  /** @type {Handle} */
  function onexitcodetext() {
    const data = this.resume()
    this.stack[this.stack.length - 1].value = data
  }

  /** @type {Handle} */
  function onexitlink() {
    const context = this.stack[this.stack.length - 1]

    // To do: clean.
    if (getData('inReference')) {
      context.type += 'Reference'
      context.referenceType = getData('referenceType') || 'shortcut'
      delete context.url
      delete context.title
    } else {
      delete context.identifier
      delete context.label
      delete context.referenceType
    }

    setData('referenceType')
  }

  /** @type {Handle} */
  function onexitimage() {
    const context = this.stack[this.stack.length - 1]

    // To do: clean.
    if (getData('inReference')) {
      context.type += 'Reference'
      context.referenceType = getData('referenceType') || 'shortcut'
      delete context.url
      delete context.title
    } else {
      delete context.identifier
      delete context.label
      delete context.referenceType
    }

    setData('referenceType')
  }

  /** @type {Handle} */
  function onexitlabeltext(token) {
    this.stack[this.stack.length - 2].identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase()
  }

  /** @type {Handle} */
  function onexitlabel() {
    const fragment = this.stack[this.stack.length - 1]
    const value = this.resume()

    this.stack[this.stack.length - 1].label = value

    // Assume a reference.
    setData('inReference', true)

    if (this.stack[this.stack.length - 1].type === 'link') {
      this.stack[this.stack.length - 1].children = fragment.children
    } else {
      this.stack[this.stack.length - 1].alt = value
    }
  }

  /** @type {Handle} */
  function onexitresourcedestinationstring() {
    const data = this.resume()
    this.stack[this.stack.length - 1].url = data
  }

  /** @type {Handle} */
  function onexitresourcetitlestring() {
    const data = this.resume()
    this.stack[this.stack.length - 1].title = data
  }

  /** @type {Handle} */
  function onexitresource() {
    setData('inReference')
  }

  /** @type {Handle} */
  function onenterreference() {
    setData('referenceType', 'collapsed')
  }

  /** @type {Handle} */
  function onexitreferencestring(token) {
    const label = this.resume()
    this.stack[this.stack.length - 1].label = label
    this.stack[this.stack.length - 1].identifier = normalizeIdentifier(
      this.sliceSerialize(token)
    ).toLowerCase()
    setData('referenceType', 'full')
  }

  /** @type {Handle} */
  function onexitcharacterreferencemarker(token) {
    setData('characterReferenceType', token.type)
  }

  /** @type {Handle} */
  function onexitcharacterreferencevalue(token) {
    const data = this.sliceSerialize(token)
    const type = getData('characterReferenceType')
    /** @type {string} */
    let value

    if (type) {
      value = decodeNumericCharacterReference(
        data,
        type === types.characterReferenceMarkerNumeric
          ? constants.numericBaseDecimal
          : constants.numericBaseHexadecimal
      )
      setData('characterReferenceType')
    } else {
      // @ts-expect-error `decodeEntity` can return false for invalid named
      // character references, but everything we’ve tokenized is valid.
      value = decodeEntity(data)
    }

    const tail = this.stack.pop()
    assert(tail, 'expected `node`')
    assert(tail.position, 'expected `node.position`')
    tail.value += value
    tail.position.end = point(token.end)
  }

  /** @type {Handle} */
  function onexitautolinkprotocol(token) {
    onexitdata.call(this, token)
    this.stack[this.stack.length - 1].url = this.sliceSerialize(token)
  }

  /** @type {Handle} */
  function onexitautolinkemail(token) {
    onexitdata.call(this, token)
    this.stack[this.stack.length - 1].url =
      'mailto:' + this.sliceSerialize(token)
  }

  //
  // Creaters.
  //

  /** @returns {Blockquote} */
  function blockQuote() {
    return {type: 'blockquote', children: []}
  }

  /** @returns {Code} */
  function codeFlow() {
    // @ts-expect-error: we’ve always used `null`.
    return {type: 'code', lang: null, meta: null, value: ''}
  }

  /** @returns {InlineCode} */
  function codeText() {
    return {type: 'inlineCode', value: ''}
  }

  /** @returns {Definition} */
  function definition() {
    return {
      type: 'definition',
      identifier: '',
      // @ts-expect-error: we’ve always used `null`.
      label: null,
      // @ts-expect-error: we’ve always used `null`.
      title: null,
      url: ''
    }
  }

  /** @returns {Emphasis} */
  function emphasis() {
    return {type: 'emphasis', children: []}
  }

  /** @returns {Heading} */
  function heading() {
    // @ts-expect-error `depth` will be set later.
    return {type: 'heading', depth: undefined, children: []}
  }

  /** @returns {Break} */
  function hardBreak() {
    return {type: 'break'}
  }

  /** @returns {HTML} */
  function html() {
    return {type: 'html', value: ''}
  }

  /** @returns {Image} */
  function image() {
    // @ts-expect-error: we’ve always used `null`.
    return {type: 'image', title: null, url: '', alt: null}
  }

  /** @returns {Link} */
  function link() {
    // @ts-expect-error: we’ve always used `null`.
    return {type: 'link', title: null, url: '', children: []}
  }

  /**
   * @param {Token} token
   * @returns {List}
   */
  function list(token) {
    return {
      type: 'list',
      ordered: token.type === 'listOrdered',
      // @ts-expect-error: we’ve always used `null`.
      start: null,
      // @ts-expect-error Patched.
      spread: token._spread,
      children: []
    }
  }

  /**
   * @param {Token} token
   * @returns {ListItem}
   */
  function listItem(token) {
    return {
      type: 'listItem',
      // @ts-expect-error Patched.
      spread: token._spread,
      // @ts-expect-error: we’ve always used `null`.
      checked: null,
      children: []
    }
  }

  /** @returns {Paragraph} */
  function paragraph() {
    return {type: 'paragraph', children: []}
  }

  /** @returns {Strong} */
  function strong() {
    return {type: 'strong', children: []}
  }

  /** @returns {Text} */
  function text() {
    return {type: 'text', value: ''}
  }

  /** @returns {ThematicBreak} */
  function thematicBreak() {
    return {type: 'thematicBreak'}
  }
}

/**
 * @param {Extension} combined
 * @param {Array.<Extension|Array.<Extension>>} extensions
 * @returns {Extension}
 */
function configure(combined, extensions) {
  let index = -1

  while (++index < extensions.length) {
    const value = extensions[index]

    if (Array.isArray(value)) {
      configure(combined, value)
    } else {
      extension(combined, value)
    }
  }

  return combined
}

/**
 * @param {Extension} combined
 * @param {Extension} extension
 * @returns {void}
 */
function extension(combined, extension) {
  /** @type {string} */
  let key

  for (key in extension) {
    if (own.call(extension, key)) {
      const list = key === 'canContainEols' || key === 'transforms'
      const maybe = own.call(combined, key) ? combined[key] : undefined
      /* c8 ignore next */
      const left = maybe || (combined[key] = list ? [] : {})
      const right = extension[key]

      if (right) {
        if (list) {
          // @ts-expect-error: `left` is an array.
          combined[key] = [...left, ...right]
        } else {
          Object.assign(left, right)
        }
      }
    }
  }
}
