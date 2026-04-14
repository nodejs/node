'use strict'

const { createInflateRaw, Z_DEFAULT_WINDOWBITS } = require('node:zlib')
const { isValidClientWindowBits } = require('./util')
const { MessageSizeExceededError } = require('../../core/errors')

const tail = Buffer.from([0x00, 0x00, 0xff, 0xff])
const kBuffer = Symbol('kBuffer')
const kLength = Symbol('kLength')

class PerMessageDeflate {
  /** @type {import('node:zlib').InflateRaw} */
  #inflate

  #options = {}

  #maxPayloadSize = 0

  /**
   * @param {Map<string, string>} extensions
   */
  constructor (extensions, options) {
    this.#options.serverNoContextTakeover = extensions.has('server_no_context_takeover')
    this.#options.serverMaxWindowBits = extensions.get('server_max_window_bits')

    this.#maxPayloadSize = options.maxPayloadSize
  }

  /**
   * Decompress a compressed payload.
   * @param {Buffer} chunk Compressed data
   * @param {boolean} fin Final fragment flag
   * @param {Function} callback Callback function
   */
  decompress (chunk, fin, callback) {
    // An endpoint uses the following algorithm to decompress a message.
    // 1.  Append 4 octets of 0x00 0x00 0xff 0xff to the tail end of the
    //     payload of the message.
    // 2.  Decompress the resulting data using DEFLATE.
    if (!this.#inflate) {
      let windowBits = Z_DEFAULT_WINDOWBITS

      if (this.#options.serverMaxWindowBits) { // empty values default to Z_DEFAULT_WINDOWBITS
        if (!isValidClientWindowBits(this.#options.serverMaxWindowBits)) {
          callback(new Error('Invalid server_max_window_bits'))
          return
        }

        windowBits = Number.parseInt(this.#options.serverMaxWindowBits)
      }

      try {
        this.#inflate = createInflateRaw({ windowBits })
      } catch (err) {
        callback(err)
        return
      }
      this.#inflate[kBuffer] = []
      this.#inflate[kLength] = 0

      this.#inflate.on('data', (data) => {
        this.#inflate[kLength] += data.length

        if (this.#maxPayloadSize > 0 && this.#inflate[kLength] > this.#maxPayloadSize) {
          callback(new MessageSizeExceededError())
          this.#inflate.removeAllListeners()
          this.#inflate = null
          return
        }

        this.#inflate[kBuffer].push(data)
      })

      this.#inflate.on('error', (err) => {
        this.#inflate = null
        callback(err)
      })
    }

    this.#inflate.write(chunk)
    if (fin) {
      this.#inflate.write(tail)
    }

    this.#inflate.flush(() => {
      if (!this.#inflate) {
        return
      }

      const full = Buffer.concat(this.#inflate[kBuffer], this.#inflate[kLength])

      this.#inflate[kBuffer].length = 0
      this.#inflate[kLength] = 0

      callback(null, full)
    })
  }
}

module.exports = { PerMessageDeflate }
