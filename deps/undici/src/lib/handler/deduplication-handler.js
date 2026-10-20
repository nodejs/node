'use strict'

const { RequestAbortedError } = require('../core/errors')

/**
 * @typedef {import('../../types/dispatcher.d.ts').default.DispatchHandler} DispatchHandler
 */

const DEFAULT_MAX_BUFFER_SIZE = 5 * 1024 * 1024

/**
 * @typedef {Object} WaitingHandler
 * @property {DispatchHandler} handler
 * @property {import('../../types/dispatcher.d.ts').default.DispatchController} controller
 * @property {Buffer[]} bufferedChunks
 * @property {number} bufferedBytes
 * @property {object | null} pendingTrailers
 * @property {boolean} done
 * @property {boolean} [pendingStart] onRequestStart deferred until the primary request's context is known
 */

/**
 * Handler that forwards response events to multiple waiting handlers.
 * Used for request deduplication.
 *
 * @implements {DispatchHandler}
 */
class DeduplicationHandler {
  /**
   * @type {DispatchHandler}
   */
  #primaryHandler

  /**
   * Whether the primary request has started (onRequestStart received). Until
   * then the request context is unknown and cannot be given to waiting handlers.
   * @type {boolean}
   */
  #requestStarted = false

  /**
   * The primary request context, replayed to waiting handlers so they observe
   * the same redirect history as the primary.
   * @type {any}
   */
  #context = null

  /**
   * @type {WaitingHandler[]}
   */
  #waitingHandlers = []

  /**
   * @type {number}
   */
  #maxBufferSize = DEFAULT_MAX_BUFFER_SIZE

  /**
   * @type {number}
   */
  #statusCode = 0

  /**
   * @type {Record<string, string | string[]>}
   */
  #headers = {}

  /**
   * @type {string}
   */
  #statusMessage = ''

  /**
   * @type {boolean}
   */
  #aborted = false

  /**
   * @type {boolean}
   */
  #responseStarted = false

  /**
   * @type {boolean}
   */
  #responseDataStarted = false

  /**
   * @type {boolean}
   */
  #completed = false

  /**
   * @type {import('../../types/dispatcher.d.ts').default.DispatchController | null}
   */
  #controller = null

  /**
   * @type {(() => void) | null}
   */
  #onComplete = null

  /**
   * @param {DispatchHandler} primaryHandler The primary handler
   * @param {() => void} onComplete Callback when request completes
   * @param {number} [maxBufferSize] Maximum paused buffer size per waiting handler
   */
  constructor (primaryHandler, onComplete, maxBufferSize = DEFAULT_MAX_BUFFER_SIZE) {
    this.#primaryHandler = primaryHandler
    this.#onComplete = onComplete
    this.#maxBufferSize = maxBufferSize
  }

  /**
   * Add a waiting handler that will receive response events.
   * Returns false if deduplication can no longer safely attach this handler.
   *
   * @param {DispatchHandler} handler
   * @returns {boolean}
   */
  addWaitingHandler (handler) {
    if (this.#completed || this.#responseDataStarted) {
      return false
    }

    const waitingHandler = this.#createWaitingHandler(handler)
    const waitingController = waitingHandler.controller

    try {
      if (this.#requestStarted) {
        // The primary request has started, so its context (including the
        // redirect history) is known and can be delivered immediately.
        handler.onRequestStart?.(waitingController, this.#context)

        if (waitingController.aborted) {
          waitingHandler.done = true
          return true
        }

        if (this.#responseStarted) {
          handler.onResponseStart?.(
            waitingController,
            this.#statusCode,
            this.#headers,
            this.#statusMessage
          )
        }
      } else {
        // The primary request has not started yet. Defer onRequestStart until
        // its context is known, so the waiting handler observes the same
        // redirect history as the primary (a cache handler relies on it to
        // reject cross-origin redirect responses).
        waitingHandler.pendingStart = true
      }
    } catch {
      // Ignore errors from waiting handlers
      waitingHandler.done = true
      return true
    }

    if (!waitingController.aborted) {
      this.#waitingHandlers.push(waitingHandler)
    }

    return true
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {any} context
   */
  onRequestStart (controller, context) {
    this.#controller = controller
    if (!this.#requestStarted) {
      this.#requestStarted = true
      this.#context = context
      // Deliver the now-known context to handlers that joined before the
      // primary request started.
      for (const waitingHandler of this.#waitingHandlers) {
        if (waitingHandler.pendingStart && !waitingHandler.done && !waitingHandler.controller.aborted) {
          waitingHandler.pendingStart = false
          try {
            waitingHandler.handler.onRequestStart?.(waitingHandler.controller, context)
          } catch {
            // Ignore errors from waiting handlers
            waitingHandler.done = true
          }
        }
      }
      this.#pruneDoneWaitingHandlers()
    }
    this.#primaryHandler.onRequestStart?.(controller, context)
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {number} statusCode
   * @param {import('../../types/header.d.ts').IncomingHttpHeaders} headers
   * @param {Socket} socket
   */
  onRequestUpgrade (controller, statusCode, headers, socket) {
    this.#primaryHandler.onRequestUpgrade?.(controller, statusCode, headers, socket)
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {number} statusCode
   * @param {Record<string, string | string[]>} headers
   * @param {string} statusMessage
   */
  onResponseStart (controller, statusCode, headers, statusMessage) {
    this.#responseStarted = true
    this.#statusCode = statusCode
    this.#headers = headers
    this.#statusMessage = statusMessage

    this.#primaryHandler.onResponseStart?.(controller, statusCode, headers, statusMessage)

    for (const waitingHandler of this.#waitingHandlers) {
      const { handler, controller: waitingController } = waitingHandler

      if (waitingHandler.done || waitingController.aborted) {
        waitingHandler.done = true
        continue
      }

      try {
        handler.onResponseStart?.(
          waitingController,
          statusCode,
          headers,
          statusMessage
        )
      } catch {
        // Ignore errors from waiting handlers
      }

      if (waitingController.aborted) {
        waitingHandler.done = true
      }
    }

    this.#pruneDoneWaitingHandlers()
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {Buffer} chunk
   */
  onResponseData (controller, chunk) {
    if (this.#aborted || this.#completed) {
      return
    }

    this.#responseDataStarted = true

    this.#primaryHandler.onResponseData?.(controller, chunk)

    for (const waitingHandler of this.#waitingHandlers) {
      const { handler, controller: waitingController } = waitingHandler

      if (waitingHandler.done || waitingController.aborted) {
        waitingHandler.done = true
        continue
      }

      if (waitingController.paused) {
        this.#bufferWaitingChunk(waitingHandler, chunk)
        continue
      }

      try {
        handler.onResponseData?.(waitingController, chunk)
      } catch {
        // Ignore errors from waiting handlers
      }

      if (waitingController.aborted) {
        waitingHandler.done = true
        waitingHandler.bufferedChunks = []
        waitingHandler.bufferedBytes = 0
      }
    }

    this.#pruneDoneWaitingHandlers()
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {object} trailers
   */
  onResponseEnd (controller, trailers) {
    if (this.#aborted || this.#completed) {
      return
    }

    this.#completed = true
    // Remove the completed request before callbacks can redispatch into this
    // interceptor. Cleanup must not depend on downstream handlers succeeding.
    this.#cleanup()

    try {
      this.#primaryHandler.onResponseEnd?.(controller, trailers)
    } catch (err) {
      // Completion callbacks may throw to initiate a retry. The transport's
      // error callback would be ignored now that this handler is completed.
      this.#primaryHandler.onResponseError?.(controller, err)
    } finally {
      for (const waitingHandler of this.#waitingHandlers) {
        if (waitingHandler.done || waitingHandler.controller.aborted) {
          waitingHandler.done = true
          continue
        }

        this.#flushWaitingHandler(waitingHandler)

        if (waitingHandler.done || waitingHandler.controller.aborted) {
          waitingHandler.done = true
          continue
        }

        if (waitingHandler.controller.paused) {
          waitingHandler.pendingTrailers = trailers
          continue
        }

        this.#endWaitingHandler(waitingHandler, trailers)
      }

      this.#pruneDoneWaitingHandlers()
    }
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {Error} err
   */
  onResponseError (controller, err) {
    if (this.#completed) {
      return
    }

    this.#aborted = true
    this.#completed = true
    this.#cleanup()

    try {
      this.#primaryHandler.onResponseError?.(controller, err)
    } finally {
      for (const waitingHandler of this.#waitingHandlers) {
        this.#errorWaitingHandler(waitingHandler, err)
      }

      this.#waitingHandlers = []
    }
  }

  #cleanup () {
    const onComplete = this.#onComplete
    this.#onComplete = null
    onComplete?.()
  }

  /**
   * @param {DispatchHandler} handler
   * @returns {WaitingHandler}
   */
  #createWaitingHandler (handler) {
    /** @type {WaitingHandler} */
    const waitingHandler = {
      handler,
      controller: null,
      bufferedChunks: [],
      bufferedBytes: 0,
      pendingTrailers: null,
      done: false,
      pendingStart: false
    }

    const state = {
      aborted: false,
      paused: false,
      reason: null
    }

    waitingHandler.controller = {
      resume: () => {
        if (state.aborted) {
          return
        }

        state.paused = false
        this.#flushWaitingHandler(waitingHandler)

        if (
          this.#completed &&
          waitingHandler.pendingTrailers &&
          waitingHandler.bufferedChunks.length === 0 &&
          !state.paused &&
          !state.aborted
        ) {
          this.#endWaitingHandler(waitingHandler, waitingHandler.pendingTrailers)
        }

        this.#pruneDoneWaitingHandlers()
      },
      pause: () => {
        if (!state.aborted) {
          state.paused = true
        }
      },
      get paused () { return state.paused },
      get aborted () { return state.aborted },
      get reason () { return state.reason },
      abort: (reason) => {
        if (state.aborted) {
          return
        }

        state.aborted = true
        state.reason = reason ?? null
        waitingHandler.done = true
        waitingHandler.pendingTrailers = null
        waitingHandler.bufferedChunks = []
        waitingHandler.bufferedBytes = 0

        try {
          handler.onResponseError?.(waitingHandler.controller, state.reason ?? new RequestAbortedError())
        } catch {
          // Ignore errors from waiting handlers
        }
      }
    }

    return waitingHandler
  }

  /**
   * @param {WaitingHandler} waitingHandler
   * @param {Buffer} chunk
   */
  #bufferWaitingChunk (waitingHandler, chunk) {
    if (waitingHandler.done || waitingHandler.controller.aborted) {
      waitingHandler.done = true
      waitingHandler.bufferedChunks = []
      waitingHandler.bufferedBytes = 0
      return
    }

    const bufferedChunk = Buffer.from(chunk)
    waitingHandler.bufferedChunks.push(bufferedChunk)
    waitingHandler.bufferedBytes += bufferedChunk.length

    if (waitingHandler.bufferedBytes > this.#maxBufferSize) {
      const err = new RequestAbortedError(`Deduplicated waiting handler exceeded maxBufferSize (${this.#maxBufferSize} bytes) while paused`)
      waitingHandler.controller.abort(err)
    }
  }

  /**
   * @param {WaitingHandler} waitingHandler
   */
  #flushWaitingHandler (waitingHandler) {
    const { handler, controller } = waitingHandler

    while (
      !waitingHandler.done &&
      !controller.aborted &&
      !controller.paused &&
      waitingHandler.bufferedChunks.length > 0
    ) {
      const bufferedChunk = waitingHandler.bufferedChunks.shift()
      waitingHandler.bufferedBytes -= bufferedChunk.length

      try {
        handler.onResponseData?.(controller, bufferedChunk)
      } catch {
        // Ignore errors from waiting handlers
      }

      if (controller.aborted) {
        waitingHandler.done = true
        waitingHandler.pendingTrailers = null
        waitingHandler.bufferedChunks = []
        waitingHandler.bufferedBytes = 0
        break
      }
    }
  }

  /**
   * @param {WaitingHandler} waitingHandler
   * @param {object} trailers
   */
  #endWaitingHandler (waitingHandler, trailers) {
    waitingHandler.done = true
    waitingHandler.pendingTrailers = null

    try {
      waitingHandler.handler.onResponseEnd?.(waitingHandler.controller, trailers)
    } catch (err) {
      // This is a response failure, not a consumer abort. Aborting the waiting
      // controller here would prevent downstream handlers from retrying.
      try {
        waitingHandler.handler.onResponseError?.(waitingHandler.controller, err)
      } catch {
        // An error callback must not prevent other waiting handlers completing.
      }
    }
  }

  /**
   * @param {WaitingHandler} waitingHandler
   * @param {Error} err
   */
  #errorWaitingHandler (waitingHandler, err) {
    if (waitingHandler.done) {
      return
    }

    waitingHandler.done = true
    waitingHandler.pendingTrailers = null
    waitingHandler.bufferedChunks = []
    waitingHandler.bufferedBytes = 0

    // A response failure is not a consumer abort: retry handlers must be able
    // to retry it just as they would a failure of the primary request.
    try {
      waitingHandler.handler.onResponseError?.(waitingHandler.controller, err)
    } catch {
      // Ignore errors from waiting handlers
    }
  }

  #pruneDoneWaitingHandlers () {
    this.#waitingHandlers = this.#waitingHandlers.filter(waitingHandler => waitingHandler.done === false)
  }
}

module.exports = DeduplicationHandler
