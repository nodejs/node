'use strict'

const Agent = require('../dispatcher/agent')
const MockAgent = require('./mock-agent')
const { SnapshotRecorder } = require('./snapshot-recorder')
const WrapHandler = require('../handler/wrap-handler')
const { InvalidArgumentError, UndiciError } = require('../core/errors')

const kSnapshotRecorder = Symbol('kSnapshotRecorder')
const kSnapshotMode = Symbol('kSnapshotMode')
const kSnapshotPath = Symbol('kSnapshotPath')
const kSnapshotLoaded = Symbol('kSnapshotLoaded')
const kRealAgent = Symbol('kRealAgent')

// Static flag to ensure warning is only emitted once
let warningEmitted = false

class SnapshotAgent extends MockAgent {
  constructor (opts = {}) {
    // Emit experimental warning only once
    if (!warningEmitted) {
      process.emitWarning(
        'SnapshotAgent is experimental and subject to change',
        'ExperimentalWarning'
      )
      warningEmitted = true
    }

    const mockOptions = { ...opts }
    delete mockOptions.mode
    delete mockOptions.snapshotPath

    super(mockOptions)

    // Validate mode option
    const validModes = ['record', 'playback', 'update']
    const mode = opts.mode || 'record'
    if (!validModes.includes(mode)) {
      throw new InvalidArgumentError(`Invalid snapshot mode: ${mode}. Must be one of: ${validModes.join(', ')}`)
    }

    // Validate snapshotPath is provided when required
    if ((mode === 'playback' || mode === 'update') && !opts.snapshotPath) {
      throw new InvalidArgumentError(`snapshotPath is required when mode is '${mode}'`)
    }

    this[kSnapshotMode] = mode
    this[kSnapshotPath] = opts.snapshotPath
    this[kSnapshotRecorder] = new SnapshotRecorder({
      snapshotPath: this[kSnapshotPath],
      mode: this[kSnapshotMode],
      maxSnapshots: opts.maxSnapshots,
      autoFlush: opts.autoFlush,
      flushInterval: opts.flushInterval,
      matchHeaders: opts.matchHeaders,
      ignoreHeaders: opts.ignoreHeaders,
      excludeHeaders: opts.excludeHeaders,
      matchBody: opts.matchBody,
      matchQuery: opts.matchQuery,
      caseSensitive: opts.caseSensitive,
      shouldRecord: opts.shouldRecord,
      shouldPlayback: opts.shouldPlayback,
      excludeUrls: opts.excludeUrls
    })
    this[kSnapshotLoaded] = false

    // For recording/update mode, we need a real agent to make actual requests
    if (this[kSnapshotMode] === 'record' || this[kSnapshotMode] === 'update') {
      this[kRealAgent] = new Agent(opts)
    }

    // Auto-load snapshots in playback/update mode
    if ((this[kSnapshotMode] === 'playback' || this[kSnapshotMode] === 'update') && this[kSnapshotPath]) {
      this.loadSnapshots().catch(() => {
        // Ignore load errors - file might not exist yet
      })
    }
  }

  dispatch (opts, handler) {
    handler = WrapHandler.wrap(handler)
    const mode = this[kSnapshotMode]

    if (mode === 'playback' || mode === 'update') {
      // Ensure snapshots are loaded
      if (!this[kSnapshotLoaded]) {
        // Need to load asynchronously, delegate to async version
        return this._asyncDispatch(opts, handler)
      }

      // Try to find existing snapshot (synchronous)
      const snapshot = this[kSnapshotRecorder].findSnapshot(opts)

      if (snapshot) {
        // Use recorded response (synchronous)
        return this._replaySnapshot(snapshot, handler)
      } else if (mode === 'update') {
        // Make real request and record it (async required)
        return this._recordAndReplay(opts, handler)
      } else {
        // Playback mode but no snapshot found
        const error = new UndiciError(`No snapshot found for ${opts.method || 'GET'} ${opts.path}`)
        if (handler.onError) {
          handler.onError(error)
          return
        }
        throw error
      }
    } else if (mode === 'record') {
      // Record mode - make real request and save response (async required)
      return this._recordAndReplay(opts, handler)
    } else {
      throw new InvalidArgumentError(`Invalid snapshot mode: ${mode}. Must be 'record', 'playback', or 'update'`)
    }
  }

  /**
   * Async version of dispatch for when we need to load snapshots first
   */
  async _asyncDispatch (opts, handler) {
    await this.loadSnapshots()
    return this.dispatch(opts, handler)
  }

  /**
   * Records a real request and replays the response
   */
  _recordAndReplay (opts, handler) {
    const responseData = {
      statusCode: null,
      headers: {},
      trailers: {},
      body: []
    }

    const self = this // Capture 'this' context for use within nested handler callbacks

    const recordingHandler = {
      onRequestStart (controller, context) {
        return handler.onRequestStart(controller, { ...context, history: this.history })
      },

      onRequestUpgrade (controller, statusCode, headers, socket) {
        return handler.onRequestUpgrade(controller, statusCode, headers, socket)
      },

      onResponseStart (controller, statusCode, headers, statusMessage) {
        responseData.statusCode = statusCode
        responseData.headers = headers
        return handler.onResponseStart(controller, statusCode, headers, statusMessage)
      },

      onResponseData (controller, chunk) {
        responseData.body.push(chunk)
        return handler.onResponseData(controller, chunk)
      },

      onResponseEnd (controller, trailers) {
        responseData.trailers = trailers

        // Record the interaction using captured 'self' context (fire and forget)
        const responseBody = Buffer.concat(responseData.body)
        self[kSnapshotRecorder].record(opts, {
          statusCode: responseData.statusCode,
          headers: responseData.headers,
          body: responseBody,
          trailers: responseData.trailers
        }).then(() => {
          handler.onResponseEnd(controller, trailers)
        }).catch((error) => {
          handler.onResponseError(controller, error)
        })
      }
    }

    // Use composed agent if available (includes interceptors), otherwise use real agent
    const agent = this[kRealAgent]
    return agent.dispatch(opts, recordingHandler)
  }

  /**
   * Replays a recorded response
   */
  _replaySnapshot (snapshot, handler) {
    return new Promise((resolve) => {
      // Simulate the response
      setImmediate(() => {
        try {
          const { response } = snapshot

          const controller = {
            pause () {},
            resume () {},
            abort (reason) {
              this.aborted = true
              this.reason = reason
            },

            aborted: false,
            paused: false
          }

          handler.onRequestStart(controller)

          handler.onResponseStart(controller, response.statusCode, response.headers)

          // Body is always stored as base64 string
          const body = Buffer.from(response.body, 'base64')
          handler.onResponseData(controller, body)

          handler.onResponseEnd(controller, response.trailers)
          resolve()
        } catch (error) {
          handler.onError?.(error)
        }
      })
    })
  }

  /**
   * Loads snapshots from file
   */
  async loadSnapshots (filePath) {
    await this[kSnapshotRecorder].loadSnapshots(filePath || this[kSnapshotPath])
    this[kSnapshotLoaded] = true

    // In playback mode, set up MockAgent interceptors for all snapshots
    if (this[kSnapshotMode] === 'playback') {
      this._setupMockInterceptors()
    }
  }

  /**
   * Saves snapshots to file
   */
  async saveSnapshots (filePath) {
    return this[kSnapshotRecorder].saveSnapshots(filePath || this[kSnapshotPath])
  }

  /**
   * Sets up MockAgent interceptors based on recorded snapshots.
   *
   * This method creates MockAgent interceptors for each recorded snapshot,
   * allowing the SnapshotAgent to fall back to MockAgent's standard intercept
   * mechanism in playback mode. Each interceptor is configured to persist
   * (remain active for multiple requests) and responds with the recorded
   * response data.
   *
   * Called automatically when loading snapshots in playback mode.
   *
   * @private
   */
  _setupMockInterceptors () {
    for (const snapshot of this[kSnapshotRecorder].getSnapshots()) {
      const { request, responses, response } = snapshot
      const url = new URL(request.url)

      const mockPool = this.get(url.origin)

      // Handle both new format (responses array) and legacy format (response object)
      const responseData = responses ? responses[0] : response
      if (!responseData) continue

      mockPool.intercept({
        path: url.pathname + url.search,
        method: request.method,
        headers: request.headers,
        body: request.body
      }).reply(responseData.statusCode, responseData.body, {
        headers: responseData.headers,
        trailers: responseData.trailers
      }).persist()
    }
  }

  /**
   * Gets the snapshot recorder
   */
  getRecorder () {
    return this[kSnapshotRecorder]
  }

  /**
   * Gets the current mode
   */
  getMode () {
    return this[kSnapshotMode]
  }

  /**
   * Clears all snapshots
   */
  clearSnapshots () {
    this[kSnapshotRecorder].clear()
  }

  /**
   * Resets call counts for all snapshots (useful for test cleanup)
   */
  resetCallCounts () {
    this[kSnapshotRecorder].resetCallCounts()
  }

  /**
   * Deletes a specific snapshot by request options
   */
  deleteSnapshot (requestOpts) {
    return this[kSnapshotRecorder].deleteSnapshot(requestOpts)
  }

  /**
   * Gets information about a specific snapshot
   */
  getSnapshotInfo (requestOpts) {
    return this[kSnapshotRecorder].getSnapshotInfo(requestOpts)
  }

  /**
   * Replaces all snapshots with new data (full replacement)
   */
  replaceSnapshots (snapshotData) {
    this[kSnapshotRecorder].replaceSnapshots(snapshotData)
  }

  async close () {
    // Close recorder (saves snapshots and cleans up timers)
    await this[kSnapshotRecorder].close()
    await this[kRealAgent]?.close()
    await super.close()
  }
}

module.exports = SnapshotAgent
