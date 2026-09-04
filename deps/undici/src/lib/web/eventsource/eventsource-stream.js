'use strict'
const { Transform } = require('node:stream')
const { isASCIINumber, isValidLastEventId } = require('./util')

/**
 * @type {number[]} BOM
 */
const BOM = [0xEF, 0xBB, 0xBF]
/**
 * @type {10} LF
 */
const LF = 0x0A
/**
 * @type {13} CR
 */
const CR = 0x0D
/**
 * @type {58} COLON
 */
const COLON = 0x3A
/**
 * @type {32} SPACE
 */
const SPACE = 0x20

const DATA = Buffer.from('data')
const EVENT = Buffer.from('event')
const ID = Buffer.from('id')
const RETRY = Buffer.from('retry')

function isASCIINumberBytes (buffer, start) {
  if (start >= buffer.length) {
    return false
  }

  for (let i = start; i < buffer.length; i++) {
    if (buffer[i] < 0x30 || buffer[i] > 0x39) {
      return false
    }
  }

  return true
}

function isValidLastEventIdBytes (buffer, start) {
  for (let i = start; i < buffer.length; i++) {
    if (buffer[i] === 0x00) {
      return false
    }
  }

  return true
}

function isFieldName (line, length, field) {
  if (length !== field.length) {
    return false
  }

  for (let i = 0; i < length; i++) {
    if (line[i] !== field[i]) {
      return false
    }
  }

  return true
}

/**
 * @typedef {object} EventSourceStreamEvent
 * @type {object}
 * @property {string} [event] The event type.
 * @property {string} [data] The data of the message.
 * @property {string} [id] A unique ID for the event.
 * @property {string} [retry] The reconnection time, in milliseconds.
 */

/**
 * @typedef eventSourceSettings
 * @type {object}
 * @property {string} [lastEventId] The last event ID received from the server.
 * @property {string} [origin] The origin of the event source.
 * @property {number} [reconnectionTime] The reconnection time, in milliseconds.
 */

class EventSourceStream extends Transform {
  /**
   * @type {eventSourceSettings}
   */
  state

  /**
   * Leading byte-order-mark check.
   * @type {boolean}
   */
  checkBOM = true

  /**
   * @type {boolean}
   */
  crlfCheck = false

  /**
   * @type {boolean}
   */
  eventEndCheck = false

  /**
   * @type {Buffer[]}
   */
  chunks = []

  chunkIndex = 0
  pos = 0
  lineChunkIndex = 0
  linePos = 0

  event = {
    data: undefined,
    event: undefined,
    id: undefined,
    retry: undefined
  }

  /**
   * @param {object} options
   * @param {boolean} [options.readableObjectMode]
   * @param {eventSourceSettings} [options.eventSourceSettings]
   * @param {(chunk: any, encoding?: BufferEncoding | undefined) => boolean} [options.push]
   */
  constructor (options = {}) {
    // Enable object mode as EventSourceStream emits objects of shape
    // EventSourceStreamEvent
    options.readableObjectMode = true

    super(options)

    this.state = options.eventSourceSettings || {}
    if (options.push) {
      this.push = options.push
    }
  }

  /**
   * @param {Buffer} chunk
   * @param {string} _encoding
   * @param {Function} callback
   * @returns {void}
   */
  _transform (chunk, _encoding, callback) {
    if (chunk.length === 0) {
      callback()
      return
    }

    this.chunks.push(chunk)

    // Strip leading byte-order-mark if we opened the stream and started
    // the processing of the incoming data
    if (this.checkBOM) {
      if (this.handleBOM()) {
        callback()
        return
      }
    }

    while (this.hasCurrentByte()) {
      const byte = this.currentByte()

      // If the previous line ended with an end-of-line, we need to check
      // if the next character is also an end-of-line.
      if (this.eventEndCheck) {
        // If the the current character is an end-of-line, then the event
        // is finished and we can process it

        // If the previous line ended with a carriage return, we need to
        // check if the current character is a line feed and remove it
        // from the buffer.
        if (this.crlfCheck) {
          // If the current character is a line feed, we can remove it
          // from the buffer and reset the crlfCheck flag
          if (byte === LF) {
            this.crlfCheck = false
            this.consumeCurrentByte()

            // It is possible that the line feed is not the end of the
            // event. We need to check if the next character is an
            // end-of-line character to determine if the event is
            // finished. We simply continue the loop to check the next
            // character.

            // As we removed the line feed from the buffer and set the
            // crlfCheck flag to false, we basically don't make any
            // distinction between a line feed and a carriage return.
            continue
          }
          this.crlfCheck = false
        }

        if (byte === LF || byte === CR) {
          // If the current character is a carriage return, we need to
          // set the crlfCheck flag to true, as we need to check if the
          // next character is a line feed so we can remove it from the
          // buffer
          if (byte === CR) {
            this.crlfCheck = true
          }

          this.consumeCurrentByte()
          if (this.hasPendingEvent()) {
            this.processEvent(this.event)
          }
          this.clearEvent()
          continue
        }
        // If the current character is not an end-of-line, then the event
        // is not finished and we have to reset the eventEndCheck flag
        this.eventEndCheck = false
        continue
      }

      // If the current character is an end-of-line, we can process the
      // line
      if (byte === LF || byte === CR) {
        // If the current character is a carriage return, we need to
        // set the crlfCheck flag to true, as we need to check if the
        // next character is a line feed
        if (byte === CR) {
          this.crlfCheck = true
        }

        // In any case, we can process the line as we reached an
        // end-of-line character
        this.parseLine(this.readLine(), this.event)
        this.consumeCurrentByte()
        // A line was processed and this could be the end of the event. We need
        // to check if the next line is empty to determine if the event is
        // finished.
        this.eventEndCheck = true
        continue
      }

      this.advanceCursor()
    }

    callback()
  }

  /**
   * @param {Buffer} line
   * @param {EventSourceStreamEvent} event
   */
  parseLine (line, event) {
    // If the line is empty (a blank line)
    // Dispatch the event, as defined below.
    // This will be handled in the _transform method
    if (line.length === 0) {
      return
    }

    // If the line starts with a U+003A COLON character (:)
    // Ignore the line.
    const colonPosition = line.indexOf(COLON)
    if (colonPosition === 0) {
      return
    }

    let fieldLength = line.length
    let valueStart = line.length

    // If the line contains a U+003A COLON character (:)
    if (colonPosition !== -1) {
      fieldLength = colonPosition

      // Collect the characters on the line after the first U+003A COLON
      // character (:), and let value be that string.
      // If value starts with a U+0020 SPACE character, remove it from value.
      valueStart = colonPosition + 1
      if (line[valueStart] === SPACE) {
        ++valueStart
      }
    }

    if (isFieldName(line, fieldLength, DATA)) {
      const value = line.toString('utf8', valueStart)

      if (event.data === undefined) {
        event.data = value
      } else {
        event.data += `\n${value}`
      }
      return
    }

    if (isFieldName(line, fieldLength, RETRY)) {
      if (isASCIINumberBytes(line, valueStart)) {
        event.retry = line.toString('utf8', valueStart)
      }
      return
    }

    if (isFieldName(line, fieldLength, ID)) {
      if (isValidLastEventIdBytes(line, valueStart)) {
        event.id = line.toString('utf8', valueStart)
      }
      return
    }

    if (isFieldName(line, fieldLength, EVENT)) {
      const value = line.toString('utf8', valueStart)

      if (value.length > 0) {
        event.event = value
      }
    }
  }

  /**
   * @param {EventSourceStreamEvent} event
   */
  processEvent (event) {
    if (event.retry && isASCIINumber(event.retry)) {
      this.state.reconnectionTime = parseInt(event.retry, 10)
    }

    if (event.id !== undefined && isValidLastEventId(event.id)) {
      this.state.lastEventId = event.id
    }

    // only dispatch event, when data is provided
    if (event.data !== undefined) {
      this.push({
        type: event.event || 'message',
        options: {
          data: event.data,
          lastEventId: this.state.lastEventId,
          origin: this.state.origin
        }
      })
    }
  }

  clearEvent () {
    this.event.data = undefined
    this.event.event = undefined
    this.event.id = undefined
    this.event.retry = undefined
  }

  hasPendingEvent () {
    return this.event.data !== undefined ||
      this.event.event !== undefined ||
      this.event.id !== undefined ||
      this.event.retry !== undefined
  }

  hasCurrentByte () {
    return this.chunkIndex < this.chunks.length &&
      this.pos < this.chunks[this.chunkIndex].length
  }

  currentByte () {
    return this.chunks[this.chunkIndex][this.pos]
  }

  consumeCurrentByte () {
    this.advanceCursor()
    this.syncLineStartToCursor()
  }

  advanceCursor () {
    this.pos++

    while (this.chunkIndex < this.chunks.length && this.pos >= this.chunks[this.chunkIndex].length) {
      this.chunkIndex++
      this.pos = 0
    }
  }

  syncLineStartToCursor () {
    this.lineChunkIndex = this.chunkIndex
    this.linePos = this.pos
    this.dropConsumedChunks()
  }

  dropConsumedChunks () {
    while (this.lineChunkIndex > 0) {
      this.chunks.shift()
      this.lineChunkIndex--
      this.chunkIndex--
    }

    if (this.chunkIndex === this.chunks.length) {
      this.chunks.length = 0
      this.chunkIndex = 0
      this.pos = 0
      this.lineChunkIndex = 0
      this.linePos = 0
    }
  }

  readLine () {
    if (this.lineChunkIndex === this.chunkIndex) {
      return this.chunks[this.chunkIndex].subarray(this.linePos, this.pos)
    }

    const chunks = []
    let length = 0

    for (let i = this.lineChunkIndex; i <= this.chunkIndex; i++) {
      const chunk = this.chunks[i]
      const start = i === this.lineChunkIndex ? this.linePos : 0
      const end = i === this.chunkIndex ? this.pos : chunk.length
      const slice = chunk.subarray(start, end)
      length += slice.length
      chunks.push(slice)
    }

    return Buffer.concat(chunks, length)
  }

  peekBufferedByte (offset) {
    let chunkIndex = this.lineChunkIndex
    let pos = this.linePos

    while (chunkIndex < this.chunks.length) {
      const chunk = this.chunks[chunkIndex]
      const remaining = chunk.length - pos

      if (offset < remaining) {
        return chunk[pos + offset]
      }

      offset -= remaining
      chunkIndex++
      pos = 0
    }
  }

  discardLeadingBytes (count) {
    while (count > 0 && this.lineChunkIndex < this.chunks.length) {
      const chunk = this.chunks[this.lineChunkIndex]
      const remaining = chunk.length - this.linePos

      if (count < remaining) {
        this.linePos += count
        count = 0
      } else {
        count -= remaining
        this.lineChunkIndex++
        this.linePos = 0
      }
    }

    this.chunkIndex = this.lineChunkIndex
    this.pos = this.linePos
    this.dropConsumedChunks()
  }

  handleBOM () {
    const first = this.peekBufferedByte(0)
    const second = this.peekBufferedByte(1)
    const third = this.peekBufferedByte(2)

    if (second === undefined) {
      if (first === BOM[0]) {
        return true
      }

      this.checkBOM = false
      return true
    }

    if (third === undefined) {
      if (first === BOM[0] && second === BOM[1]) {
        return true
      }

      this.checkBOM = false
      return false
    }

    if (first === BOM[0] && second === BOM[1] && third === BOM[2]) {
      this.discardLeadingBytes(3)
    }

    this.checkBOM = false
    return !this.hasCurrentByte()
  }
}

module.exports = {
  EventSourceStream
}
