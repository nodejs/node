'use strict'

const { writeFile, readFile, mkdir } = require('node:fs/promises')
const { dirname, resolve } = require('node:path')
const { InvalidArgumentError, UndiciError } = require('../core/errors')

/**
 * Formats a request for consistent snapshot storage
 * Caches normalized headers to avoid repeated processing
 */
function formatRequestKey (opts, cachedSets, matchOptions = {}) {
  const url = new URL(opts.path, opts.origin)

  // Cache normalized headers if not already done
  const normalized = opts._normalizedHeaders || normalizeHeaders(opts.headers)
  if (!opts._normalizedHeaders) {
    opts._normalizedHeaders = normalized
  }

  return {
    method: opts.method || 'GET',
    url: matchOptions.matchQuery !== false ? url.toString() : `${url.origin}${url.pathname}`,
    headers: filterHeadersForMatching(normalized, cachedSets, matchOptions),
    body: matchOptions.matchBody !== false && opts.body ? String(opts.body) : undefined
  }
}

/**
 * Filters headers based on matching configuration
 */
function filterHeadersForMatching (headers, cachedSets, matchOptions = {}) {
  if (!headers || typeof headers !== 'object') return {}

  const {
    matchHeaders = null,
    caseSensitive = false
  } = matchOptions

  const filtered = {}
  const { ignoreSet, excludeSet, matchSet } = cachedSets

  for (const [key, value] of Object.entries(headers)) {
    const headerKey = caseSensitive ? key : key.toLowerCase()

    // Skip if in exclude list (for security)
    if (excludeSet.has(headerKey)) continue

    // Skip if in ignore list (for matching)
    if (ignoreSet.has(headerKey)) continue

    // If matchHeaders is specified, only include those headers
    if (matchHeaders && Array.isArray(matchHeaders)) {
      if (!matchSet.has(headerKey)) continue
    }

    filtered[headerKey] = value
  }

  return filtered
}

/**
 * Filters headers for storage (only excludes sensitive headers)
 */
function filterHeadersForStorage (headers, matchOptions = {}) {
  if (!headers || typeof headers !== 'object') return {}

  const {
    excludeHeaders = [],
    caseSensitive = false
  } = matchOptions

  const filtered = {}
  const excludeSet = new Set(excludeHeaders.map(h => caseSensitive ? h : h.toLowerCase()))

  for (const [key, value] of Object.entries(headers)) {
    const headerKey = caseSensitive ? key : key.toLowerCase()

    // Skip if in exclude list (for security)
    if (excludeSet.has(headerKey)) continue

    filtered[headerKey] = value
  }

  return filtered
}

/**
 * Creates cached header sets for performance
 */
function createHeaderSetsCache (matchOptions = {}) {
  const { ignoreHeaders = [], excludeHeaders = [], matchHeaders = null, caseSensitive = false } = matchOptions

  return {
    ignoreSet: new Set(ignoreHeaders.map(h => caseSensitive ? h : h.toLowerCase())),
    excludeSet: new Set(excludeHeaders.map(h => caseSensitive ? h : h.toLowerCase())),
    matchSet: matchHeaders && Array.isArray(matchHeaders)
      ? new Set(matchHeaders.map(h => caseSensitive ? h : h.toLowerCase()))
      : null
  }
}

/**
 * Normalizes headers for consistent comparison
 */
function normalizeHeaders (headers) {
  if (!headers) return {}

  const normalized = {}

  // Handle array format (undici internal format: [name, value, name, value, ...])
  if (Array.isArray(headers)) {
    for (let i = 0; i < headers.length; i += 2) {
      const key = headers[i]
      const value = headers[i + 1]
      if (key && value !== undefined) {
        // Convert Buffers to strings if needed
        const keyStr = Buffer.isBuffer(key) ? key.toString() : String(key)
        const valueStr = Buffer.isBuffer(value) ? value.toString() : String(value)
        normalized[keyStr.toLowerCase()] = valueStr
      }
    }
    return normalized
  }

  // Handle object format
  if (headers && typeof headers === 'object') {
    for (const [key, value] of Object.entries(headers)) {
      if (key && typeof key === 'string') {
        normalized[key.toLowerCase()] = Array.isArray(value) ? value.join(', ') : String(value)
      }
    }
  }

  return normalized
}

/**
 * Creates a hash key for request matching
 */
function createRequestHash (request) {
  const parts = [
    request.method,
    request.url,
    JSON.stringify(request.headers, Object.keys(request.headers).sort()),
    request.body || ''
  ]
  return Buffer.from(parts.join('|')).toString('base64url')
}

/**
 * Checks if a URL matches any of the exclude patterns
 */
function isUrlExcluded (url, excludePatterns = []) {
  if (!excludePatterns.length) return false

  for (const pattern of excludePatterns) {
    if (typeof pattern === 'string') {
      // Simple string match (case-insensitive)
      if (url.toLowerCase().includes(pattern.toLowerCase())) {
        return true
      }
    } else if (pattern instanceof RegExp) {
      // Regex pattern match
      if (pattern.test(url)) {
        return true
      }
    }
  }

  return false
}

class SnapshotRecorder {
  constructor (options = {}) {
    this.snapshots = new Map()
    this.snapshotPath = options.snapshotPath
    this.mode = options.mode || 'record'
    this.loaded = false
    this.maxSnapshots = options.maxSnapshots || Infinity
    this.autoFlush = options.autoFlush || false
    this.flushInterval = options.flushInterval || 30000 // 30 seconds default
    this._flushTimer = null
    this._flushTimeout = null

    // Matching configuration
    this.matchOptions = {
      matchHeaders: options.matchHeaders || null, // null means match all headers
      ignoreHeaders: options.ignoreHeaders || [],
      excludeHeaders: options.excludeHeaders || [],
      matchBody: options.matchBody !== false, // default: true
      matchQuery: options.matchQuery !== false, // default: true
      caseSensitive: options.caseSensitive || false
    }

    // Cache processed header sets to avoid recreating them on every request
    this._headerSetsCache = createHeaderSetsCache(this.matchOptions)

    // Request filtering callbacks
    this.shouldRecord = options.shouldRecord || null // function(requestOpts) -> boolean
    this.shouldPlayback = options.shouldPlayback || null // function(requestOpts) -> boolean

    // URL pattern filtering
    this.excludeUrls = options.excludeUrls || [] // Array of regex patterns or strings

    // Start auto-flush timer if enabled
    if (this.autoFlush && this.snapshotPath) {
      this._startAutoFlush()
    }
  }

  /**
   * Records a request-response interaction
   */
  async record (requestOpts, response) {
    // Check if recording should be filtered out
    if (this.shouldRecord && typeof this.shouldRecord === 'function') {
      if (!this.shouldRecord(requestOpts)) {
        return // Skip recording
      }
    }

    // Check URL exclusion patterns
    const url = new URL(requestOpts.path, requestOpts.origin).toString()
    if (isUrlExcluded(url, this.excludeUrls)) {
      return // Skip recording
    }

    const request = formatRequestKey(requestOpts, this._headerSetsCache, this.matchOptions)
    const hash = createRequestHash(request)

    // Extract response data - always store body as base64
    const normalizedHeaders = normalizeHeaders(response.headers)
    const responseData = {
      statusCode: response.statusCode,
      headers: filterHeadersForStorage(normalizedHeaders, this.matchOptions),
      body: Buffer.isBuffer(response.body)
        ? response.body.toString('base64')
        : Buffer.from(String(response.body || '')).toString('base64'),
      trailers: response.trailers
    }

    // Remove oldest snapshot if we exceed maxSnapshots limit
    if (this.snapshots.size >= this.maxSnapshots && !this.snapshots.has(hash)) {
      const oldestKey = this.snapshots.keys().next().value
      this.snapshots.delete(oldestKey)
    }

    // Support sequential responses - if snapshot exists, add to responses array
    const existingSnapshot = this.snapshots.get(hash)
    if (existingSnapshot && existingSnapshot.responses) {
      existingSnapshot.responses.push(responseData)
      existingSnapshot.timestamp = new Date().toISOString()
    } else {
      this.snapshots.set(hash, {
        request,
        responses: [responseData], // Always store as array for consistency
        callCount: 0,
        timestamp: new Date().toISOString()
      })
    }

    // Auto-flush if enabled
    if (this.autoFlush && this.snapshotPath) {
      this._scheduleFlush()
    }
  }

  /**
   * Finds a matching snapshot for the given request
   * Returns the appropriate response based on call count for sequential responses
   */
  findSnapshot (requestOpts) {
    // Check if playback should be filtered out
    if (this.shouldPlayback && typeof this.shouldPlayback === 'function') {
      if (!this.shouldPlayback(requestOpts)) {
        return undefined // Skip playback
      }
    }

    // Check URL exclusion patterns
    const url = new URL(requestOpts.path, requestOpts.origin).toString()
    if (isUrlExcluded(url, this.excludeUrls)) {
      return undefined // Skip playback
    }

    const request = formatRequestKey(requestOpts, this._headerSetsCache, this.matchOptions)
    const hash = createRequestHash(request)
    const snapshot = this.snapshots.get(hash)

    if (!snapshot) return undefined

    // Handle sequential responses
    if (snapshot.responses && Array.isArray(snapshot.responses)) {
      const currentCallCount = snapshot.callCount || 0
      const responseIndex = Math.min(currentCallCount, snapshot.responses.length - 1)
      snapshot.callCount = currentCallCount + 1

      return {
        ...snapshot,
        response: snapshot.responses[responseIndex]
      }
    }

    // Legacy format compatibility - convert single response to array format
    if (snapshot.response && !snapshot.responses) {
      snapshot.responses = [snapshot.response]
      snapshot.callCount = 1
      delete snapshot.response

      return {
        ...snapshot,
        response: snapshot.responses[0]
      }
    }

    return snapshot
  }

  /**
   * Loads snapshots from file
   */
  async loadSnapshots (filePath) {
    const path = filePath || this.snapshotPath
    if (!path) {
      throw new InvalidArgumentError('Snapshot path is required')
    }

    try {
      const data = await readFile(resolve(path), 'utf8')
      const parsed = JSON.parse(data)

      // Convert array format back to Map
      if (Array.isArray(parsed)) {
        this.snapshots.clear()
        for (const { hash, snapshot } of parsed) {
          this.snapshots.set(hash, snapshot)
        }
      } else {
        // Legacy object format
        this.snapshots = new Map(Object.entries(parsed))
      }

      this.loaded = true
    } catch (error) {
      if (error.code === 'ENOENT') {
        // File doesn't exist yet - that's ok for recording mode
        this.snapshots.clear()
        this.loaded = true
      } else {
        throw new UndiciError(`Failed to load snapshots from ${path}`, { cause: error })
      }
    }
  }

  /**
   * Saves snapshots to file
   */
  async saveSnapshots (filePath) {
    const path = filePath || this.snapshotPath
    if (!path) {
      throw new InvalidArgumentError('Snapshot path is required')
    }

    const resolvedPath = resolve(path)

    // Ensure directory exists
    await mkdir(dirname(resolvedPath), { recursive: true })

    // Convert Map to serializable format
    const data = Array.from(this.snapshots.entries()).map(([hash, snapshot]) => ({
      hash,
      snapshot
    }))

    await writeFile(resolvedPath, JSON.stringify(data, null, 2), 'utf8', { flush: true })
  }

  /**
   * Clears all recorded snapshots
   */
  clear () {
    this.snapshots.clear()
  }

  /**
   * Gets all recorded snapshots
   */
  getSnapshots () {
    return Array.from(this.snapshots.values())
  }

  /**
   * Gets snapshot count
   */
  size () {
    return this.snapshots.size
  }

  /**
   * Resets call counts for all snapshots (useful for test cleanup)
   */
  resetCallCounts () {
    for (const snapshot of this.snapshots.values()) {
      snapshot.callCount = 0
    }
  }

  /**
   * Deletes a specific snapshot by request options
   */
  deleteSnapshot (requestOpts) {
    const request = formatRequestKey(requestOpts, this._headerSetsCache, this.matchOptions)
    const hash = createRequestHash(request)
    return this.snapshots.delete(hash)
  }

  /**
   * Gets information about a specific snapshot
   */
  getSnapshotInfo (requestOpts) {
    const request = formatRequestKey(requestOpts, this._headerSetsCache, this.matchOptions)
    const hash = createRequestHash(request)
    const snapshot = this.snapshots.get(hash)

    if (!snapshot) return null

    return {
      hash,
      request: snapshot.request,
      responseCount: snapshot.responses ? snapshot.responses.length : (snapshot.response ? 1 : 0),
      callCount: snapshot.callCount || 0,
      timestamp: snapshot.timestamp
    }
  }

  /**
   * Replaces all snapshots with new data (full replacement)
   */
  replaceSnapshots (snapshotData) {
    this.snapshots.clear()

    if (Array.isArray(snapshotData)) {
      for (const { hash, snapshot } of snapshotData) {
        this.snapshots.set(hash, snapshot)
      }
    } else if (snapshotData && typeof snapshotData === 'object') {
      // Legacy object format
      this.snapshots = new Map(Object.entries(snapshotData))
    }
  }

  /**
   * Starts the auto-flush timer
   */
  _startAutoFlush () {
    if (!this._flushTimer) {
      this._flushTimer = setInterval(() => {
        this.saveSnapshots().catch(() => {
          // Ignore flush errors - they shouldn't interrupt normal operation
        })
      }, this.flushInterval)
    }
  }

  /**
   * Stops the auto-flush timer
   */
  _stopAutoFlush () {
    if (this._flushTimer) {
      clearInterval(this._flushTimer)
      this._flushTimer = null
    }
  }

  /**
   * Schedules a flush (debounced to avoid excessive writes)
   */
  _scheduleFlush () {
    // Simple debouncing - clear existing timeout and set new one
    if (this._flushTimeout) {
      clearTimeout(this._flushTimeout)
    }
    this._flushTimeout = setTimeout(() => {
      this.saveSnapshots().catch(() => {
        // Ignore flush errors
      })
      this._flushTimeout = null
    }, 1000) // 1 second debounce
  }

  /**
   * Cleanup method to stop timers
   */
  destroy () {
    this._stopAutoFlush()
    if (this._flushTimeout) {
      clearTimeout(this._flushTimeout)
      this._flushTimeout = null
    }
  }

  /**
   * Async close method that saves all recordings and performs cleanup
   */
  async close () {
    // Save any pending recordings if we have a snapshot path
    if (this.snapshotPath && this.snapshots.size > 0) {
      await this.saveSnapshots()
    }

    // Perform cleanup
    this.destroy()
  }
}

module.exports = { SnapshotRecorder, formatRequestKey, createRequestHash, filterHeadersForMatching, filterHeadersForStorage, isUrlExcluded, createHeaderSetsCache }
