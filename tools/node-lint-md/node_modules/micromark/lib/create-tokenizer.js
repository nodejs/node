/**
 * @typedef {import('micromark-util-types').Code} Code
 * @typedef {import('micromark-util-types').Chunk} Chunk
 * @typedef {import('micromark-util-types').Point} Point
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').Effects} Effects
 * @typedef {import('micromark-util-types').State} State
 * @typedef {import('micromark-util-types').Construct} Construct
 * @typedef {import('micromark-util-types').InitialConstruct} InitialConstruct
 * @typedef {import('micromark-util-types').ConstructRecord} ConstructRecord
 * @typedef {import('micromark-util-types').TokenizeContext} TokenizeContext
 * @typedef {import('micromark-util-types').ParseContext} ParseContext
 */

/**
 * @typedef Info
 * @property {() => void} restore
 * @property {number} from
 *
 * @callback ReturnHandle
 *   Handle a successful run.
 * @param {Construct} construct
 * @param {Info} info
 * @returns {void}
 */
import {markdownLineEnding} from 'micromark-util-character'
import {push, splice} from 'micromark-util-chunked'
import {resolveAll} from 'micromark-util-resolve-all'

/**
 * Create a tokenizer.
 * Tokenizers deal with one type of data (e.g., containers, flow, text).
 * The parser is the object dealing with it all.
 * `initialize` works like other constructs, except that only its `tokenize`
 * function is used, in which case it doesn’t receive an `ok` or `nok`.
 * `from` can be given to set the point before the first character, although
 * when further lines are indented, they must be set with `defineSkip`.
 *
 * @param {ParseContext} parser
 * @param {InitialConstruct} initialize
 * @param {Omit<Point, '_index'|'_bufferIndex'>} [from]
 * @returns {TokenizeContext}
 */
export function createTokenizer(parser, initialize, from) {
  /** @type {Point} */
  let point = Object.assign(
    from
      ? Object.assign({}, from)
      : {
          line: 1,
          column: 1,
          offset: 0
        },
    {
      _index: 0,
      _bufferIndex: -1
    }
  )
  /** @type {Record<string, number>} */

  const columnStart = {}
  /** @type {Construct[]} */

  const resolveAllConstructs = []
  /** @type {Chunk[]} */

  let chunks = []
  /** @type {Token[]} */

  let stack = []
  /** @type {boolean|undefined} */

  let consumed = true
  /**
   * Tools used for tokenizing.
   *
   * @type {Effects}
   */

  const effects = {
    consume,
    enter,
    exit,
    attempt: constructFactory(onsuccessfulconstruct),
    check: constructFactory(onsuccessfulcheck),
    interrupt: constructFactory(onsuccessfulcheck, {
      interrupt: true
    })
  }
  /**
   * State and tools for resolving and serializing.
   *
   * @type {TokenizeContext}
   */

  const context = {
    previous: null,
    code: null,
    containerState: {},
    events: [],
    parser,
    sliceStream,
    sliceSerialize,
    now,
    defineSkip,
    write
  }
  /**
   * The state function.
   *
   * @type {State|void}
   */

  let state = initialize.tokenize.call(context, effects)
  /**
   * Track which character we expect to be consumed, to catch bugs.
   *
   * @type {Code}
   */

  let expectedCode

  if (initialize.resolveAll) {
    resolveAllConstructs.push(initialize)
  }

  return context
  /** @type {TokenizeContext['write']} */

  function write(slice) {
    chunks = push(chunks, slice)
    main() // Exit if we’re not done, resolve might change stuff.

    if (chunks[chunks.length - 1] !== null) {
      return []
    }

    addResult(initialize, 0) // Otherwise, resolve, and exit.

    context.events = resolveAll(resolveAllConstructs, context.events, context)
    return context.events
  } //
  // Tools.
  //

  /** @type {TokenizeContext['sliceSerialize']} */

  function sliceSerialize(token, expandTabs) {
    return serializeChunks(sliceStream(token), expandTabs)
  }
  /** @type {TokenizeContext['sliceStream']} */

  function sliceStream(token) {
    return sliceChunks(chunks, token)
  }
  /** @type {TokenizeContext['now']} */

  function now() {
    return Object.assign({}, point)
  }
  /** @type {TokenizeContext['defineSkip']} */

  function defineSkip(value) {
    columnStart[value.line] = value.column
    accountForPotentialSkip()
  } //
  // State management.
  //

  /**
   * Main loop (note that `_index` and `_bufferIndex` in `point` are modified by
   * `consume`).
   * Here is where we walk through the chunks, which either include strings of
   * several characters, or numerical character codes.
   * The reason to do this in a loop instead of a call is so the stack can
   * drain.
   *
   * @returns {void}
   */

  function main() {
    /** @type {number} */
    let chunkIndex

    while (point._index < chunks.length) {
      const chunk = chunks[point._index] // If we’re in a buffer chunk, loop through it.

      if (typeof chunk === 'string') {
        chunkIndex = point._index

        if (point._bufferIndex < 0) {
          point._bufferIndex = 0
        }

        while (
          point._index === chunkIndex &&
          point._bufferIndex < chunk.length
        ) {
          go(chunk.charCodeAt(point._bufferIndex))
        }
      } else {
        go(chunk)
      }
    }
  }
  /**
   * Deal with one code.
   *
   * @param {Code} code
   * @returns {void}
   */

  function go(code) {
    consumed = undefined
    expectedCode = code
    state = state(code)
  }
  /** @type {Effects['consume']} */

  function consume(code) {
    if (markdownLineEnding(code)) {
      point.line++
      point.column = 1
      point.offset += code === -3 ? 2 : 1
      accountForPotentialSkip()
    } else if (code !== -1) {
      point.column++
      point.offset++
    } // Not in a string chunk.

    if (point._bufferIndex < 0) {
      point._index++
    } else {
      point._bufferIndex++ // At end of string chunk.
      // @ts-expect-error Points w/ non-negative `_bufferIndex` reference
      // strings.

      if (point._bufferIndex === chunks[point._index].length) {
        point._bufferIndex = -1
        point._index++
      }
    } // Expose the previous character.

    context.previous = code // Mark as consumed.

    consumed = true
  }
  /** @type {Effects['enter']} */

  function enter(type, fields) {
    /** @type {Token} */
    // @ts-expect-error Patch instead of assign required fields to help GC.
    const token = fields || {}
    token.type = type
    token.start = now()
    context.events.push(['enter', token, context])
    stack.push(token)
    return token
  }
  /** @type {Effects['exit']} */

  function exit(type) {
    const token = stack.pop()
    token.end = now()
    context.events.push(['exit', token, context])
    return token
  }
  /**
   * Use results.
   *
   * @type {ReturnHandle}
   */

  function onsuccessfulconstruct(construct, info) {
    addResult(construct, info.from)
  }
  /**
   * Discard results.
   *
   * @type {ReturnHandle}
   */

  function onsuccessfulcheck(_, info) {
    info.restore()
  }
  /**
   * Factory to attempt/check/interrupt.
   *
   * @param {ReturnHandle} onreturn
   * @param {Record<string, unknown>} [fields]
   */

  function constructFactory(onreturn, fields) {
    return hook
    /**
     * Handle either an object mapping codes to constructs, a list of
     * constructs, or a single construct.
     *
     * @param {Construct|Construct[]|ConstructRecord} constructs
     * @param {State} returnState
     * @param {State} [bogusState]
     * @returns {State}
     */

    function hook(constructs, returnState, bogusState) {
      /** @type {Construct[]} */
      let listOfConstructs
      /** @type {number} */

      let constructIndex
      /** @type {Construct} */

      let currentConstruct
      /** @type {Info} */

      let info
      return Array.isArray(constructs)
        ? /* c8 ignore next 1 */
          handleListOfConstructs(constructs)
        : 'tokenize' in constructs // @ts-expect-error Looks like a construct.
        ? handleListOfConstructs([constructs])
        : handleMapOfConstructs(constructs)
      /**
       * Handle a list of construct.
       *
       * @param {ConstructRecord} map
       * @returns {State}
       */

      function handleMapOfConstructs(map) {
        return start
        /** @type {State} */

        function start(code) {
          const def = code !== null && map[code]
          const all = code !== null && map.null
          const list = [
            // To do: add more extension tests.

            /* c8 ignore next 2 */
            ...(Array.isArray(def) ? def : def ? [def] : []),
            ...(Array.isArray(all) ? all : all ? [all] : [])
          ]
          return handleListOfConstructs(list)(code)
        }
      }
      /**
       * Handle a list of construct.
       *
       * @param {Construct[]} list
       * @returns {State}
       */

      function handleListOfConstructs(list) {
        listOfConstructs = list
        constructIndex = 0

        if (list.length === 0) {
          return bogusState
        }

        return handleConstruct(list[constructIndex])
      }
      /**
       * Handle a single construct.
       *
       * @param {Construct} construct
       * @returns {State}
       */

      function handleConstruct(construct) {
        return start
        /** @type {State} */

        function start(code) {
          // To do: not needed to store if there is no bogus state, probably?
          // Currently doesn’t work because `inspect` in document does a check
          // w/o a bogus, which doesn’t make sense. But it does seem to help perf
          // by not storing.
          info = store()
          currentConstruct = construct

          if (!construct.partial) {
            context.currentConstruct = construct
          }

          if (
            construct.name &&
            context.parser.constructs.disable.null.includes(construct.name)
          ) {
            return nok(code)
          }

          return construct.tokenize.call(
            // If we do have fields, create an object w/ `context` as its
            // prototype.
            // This allows a “live binding”, which is needed for `interrupt`.
            fields ? Object.assign(Object.create(context), fields) : context,
            effects,
            ok,
            nok
          )(code)
        }
      }
      /** @type {State} */

      function ok(code) {
        consumed = true
        onreturn(currentConstruct, info)
        return returnState
      }
      /** @type {State} */

      function nok(code) {
        consumed = true
        info.restore()

        if (++constructIndex < listOfConstructs.length) {
          return handleConstruct(listOfConstructs[constructIndex])
        }

        return bogusState
      }
    }
  }
  /**
   * @param {Construct} construct
   * @param {number} from
   * @returns {void}
   */

  function addResult(construct, from) {
    if (construct.resolveAll && !resolveAllConstructs.includes(construct)) {
      resolveAllConstructs.push(construct)
    }

    if (construct.resolve) {
      splice(
        context.events,
        from,
        context.events.length - from,
        construct.resolve(context.events.slice(from), context)
      )
    }

    if (construct.resolveTo) {
      context.events = construct.resolveTo(context.events, context)
    }
  }
  /**
   * Store state.
   *
   * @returns {Info}
   */

  function store() {
    const startPoint = now()
    const startPrevious = context.previous
    const startCurrentConstruct = context.currentConstruct
    const startEventsIndex = context.events.length
    const startStack = Array.from(stack)
    return {
      restore,
      from: startEventsIndex
    }
    /**
     * Restore state.
     *
     * @returns {void}
     */

    function restore() {
      point = startPoint
      context.previous = startPrevious
      context.currentConstruct = startCurrentConstruct
      context.events.length = startEventsIndex
      stack = startStack
      accountForPotentialSkip()
    }
  }
  /**
   * Move the current point a bit forward in the line when it’s on a column
   * skip.
   *
   * @returns {void}
   */

  function accountForPotentialSkip() {
    if (point.line in columnStart && point.column < 2) {
      point.column = columnStart[point.line]
      point.offset += columnStart[point.line] - 1
    }
  }
}
/**
 * Get the chunks from a slice of chunks in the range of a token.
 *
 * @param {Chunk[]} chunks
 * @param {Pick<Token, 'start'|'end'>} token
 * @returns {Chunk[]}
 */

function sliceChunks(chunks, token) {
  const startIndex = token.start._index
  const startBufferIndex = token.start._bufferIndex
  const endIndex = token.end._index
  const endBufferIndex = token.end._bufferIndex
  /** @type {Chunk[]} */

  let view

  if (startIndex === endIndex) {
    // @ts-expect-error `_bufferIndex` is used on string chunks.
    view = [chunks[startIndex].slice(startBufferIndex, endBufferIndex)]
  } else {
    view = chunks.slice(startIndex, endIndex)

    if (startBufferIndex > -1) {
      // @ts-expect-error `_bufferIndex` is used on string chunks.
      view[0] = view[0].slice(startBufferIndex)
    }

    if (endBufferIndex > 0) {
      // @ts-expect-error `_bufferIndex` is used on string chunks.
      view.push(chunks[endIndex].slice(0, endBufferIndex))
    }
  }

  return view
}
/**
 * Get the string value of a slice of chunks.
 *
 * @param {Chunk[]} chunks
 * @param {boolean} [expandTabs=false]
 * @returns {string}
 */

function serializeChunks(chunks, expandTabs) {
  let index = -1
  /** @type {string[]} */

  const result = []
  /** @type {boolean|undefined} */

  let atTab

  while (++index < chunks.length) {
    const chunk = chunks[index]
    /** @type {string} */

    let value

    if (typeof chunk === 'string') {
      value = chunk
    } else
      switch (chunk) {
        case -5: {
          value = '\r'
          break
        }

        case -4: {
          value = '\n'
          break
        }

        case -3: {
          value = '\r' + '\n'
          break
        }

        case -2: {
          value = expandTabs ? ' ' : '\t'
          break
        }

        case -1: {
          if (!expandTabs && atTab) continue
          value = ' '
          break
        }

        default: {
          // Currently only replacement character.
          value = String.fromCharCode(chunk)
        }
      }

    atTab = chunk === -2
    result.push(value)
  }

  return result.join('')
}
