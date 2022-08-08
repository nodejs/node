'use strict'

const util = require('../core/util')
const { ReadableStreamFrom, toUSVString, isBlobLike } = require('./util')
const { FormData } = require('./formdata')
const { kState } = require('./symbols')
const { webidl } = require('./webidl')
const { Blob } = require('buffer')
const { kBodyUsed } = require('../core/symbols')
const assert = require('assert')
const { NotSupportedError } = require('../core/errors')
const { isErrored } = require('../core/util')
const { isUint8Array, isArrayBuffer } = require('util/types')

let ReadableStream

async function * blobGen (blob) {
  yield * blob.stream()
}

// https://fetch.spec.whatwg.org/#concept-bodyinit-extract
function extractBody (object, keepalive = false) {
  if (!ReadableStream) {
    ReadableStream = require('stream/web').ReadableStream
  }

  // 1. Let stream be object if object is a ReadableStream object.
  // Otherwise, let stream be a new ReadableStream, and set up stream.
  let stream = null

  // 2. Let action be null.
  let action = null

  // 3. Let source be null.
  let source = null

  // 4. Let length be null.
  let length = null

  // 5. Let Content-Type be null.
  let contentType = null

  // 6. Switch on object:
  if (object == null) {
    // Note: The IDL processor cannot handle this situation. See
    // https://crbug.com/335871.
  } else if (object instanceof URLSearchParams) {
    // URLSearchParams

    // spec says to run application/x-www-form-urlencoded on body.list
    // this is implemented in Node.js as apart of an URLSearchParams instance toString method
    // See: https://github.com/nodejs/node/blob/e46c680bf2b211bbd52cf959ca17ee98c7f657f5/lib/internal/url.js#L490
    // and https://github.com/nodejs/node/blob/e46c680bf2b211bbd52cf959ca17ee98c7f657f5/lib/internal/url.js#L1100

    // Set source to the result of running the application/x-www-form-urlencoded serializer with object’s list.
    source = object.toString()

    // Set Content-Type to `application/x-www-form-urlencoded;charset=UTF-8`.
    contentType = 'application/x-www-form-urlencoded;charset=UTF-8'
  } else if (isArrayBuffer(object) || ArrayBuffer.isView(object)) {
    // BufferSource

    if (object instanceof DataView) {
      // TODO: Blob doesn't seem to work with DataView?
      object = object.buffer
    }

    // Set source to a copy of the bytes held by object.
    source = new Uint8Array(object)
  } else if (util.isFormDataLike(object)) {
    const boundary = '----formdata-undici-' + Math.random()
    const prefix = `--${boundary}\r\nContent-Disposition: form-data`

    /*! formdata-polyfill. MIT License. Jimmy Wärting <https://jimmy.warting.se/opensource> */
    const escape = (str) =>
      str.replace(/\n/g, '%0A').replace(/\r/g, '%0D').replace(/"/g, '%22')
    const normalizeLinefeeds = (value) => value.replace(/\r?\n|\r/g, '\r\n')

    // Set action to this step: run the multipart/form-data
    // encoding algorithm, with object’s entry list and UTF-8.
    action = async function * (object) {
      const enc = new TextEncoder()

      for (const [name, value] of object) {
        if (typeof value === 'string') {
          yield enc.encode(
            prefix +
              `; name="${escape(normalizeLinefeeds(name))}"` +
              `\r\n\r\n${normalizeLinefeeds(value)}\r\n`
          )
        } else {
          yield enc.encode(
            prefix +
              `; name="${escape(normalizeLinefeeds(name))}"` +
              (value.name ? `; filename="${escape(value.name)}"` : '') +
              '\r\n' +
              `Content-Type: ${
                value.type || 'application/octet-stream'
              }\r\n\r\n`
          )

          yield * blobGen(value)

          yield enc.encode('\r\n')
        }
      }

      yield enc.encode(`--${boundary}--`)
    }

    // Set source to object.
    source = object

    // Set length to unclear, see html/6424 for improving this.
    // TODO

    // Set Content-Type to `multipart/form-data; boundary=`,
    // followed by the multipart/form-data boundary string generated
    // by the multipart/form-data encoding algorithm.
    contentType = 'multipart/form-data; boundary=' + boundary
  } else if (isBlobLike(object)) {
    // Blob

    // Set action to this step: read object.
    action = blobGen

    // Set source to object.
    source = object

    // Set length to object’s size.
    length = object.size

    // If object’s type attribute is not the empty byte sequence, set
    // Content-Type to its value.
    if (object.type) {
      contentType = object.type
    }
  } else if (typeof object[Symbol.asyncIterator] === 'function') {
    // If keepalive is true, then throw a TypeError.
    if (keepalive) {
      throw new TypeError('keepalive')
    }

    // If object is disturbed or locked, then throw a TypeError.
    if (util.isDisturbed(object) || object.locked) {
      throw new TypeError(
        'Response body object should not be disturbed or locked'
      )
    }

    stream =
      object instanceof ReadableStream ? object : ReadableStreamFrom(object)
  } else {
    // TODO: byte sequence?
    // TODO: scalar value string?
    // TODO: else?
    source = toUSVString(object)
    contentType = 'text/plain;charset=UTF-8'
  }

  // 7. If source is a byte sequence, then set action to a
  // step that returns source and length to source’s length.
  // TODO: What is a "byte sequence?"
  if (typeof source === 'string' || util.isBuffer(source)) {
    length = Buffer.byteLength(source)
  }

  // 8. If action is non-null, then run these steps in in parallel:
  if (action != null) {
    // Run action.
    let iterator
    stream = new ReadableStream({
      async start () {
        iterator = action(object)[Symbol.asyncIterator]()
      },
      async pull (controller) {
        const { value, done } = await iterator.next()
        if (done) {
          // When running action is done, close stream.
          queueMicrotask(() => {
            controller.close()
          })
        } else {
          // Whenever one or more bytes are available and stream is not errored,
          // enqueue a Uint8Array wrapping an ArrayBuffer containing the available
          // bytes into stream.
          if (!isErrored(stream)) {
            controller.enqueue(new Uint8Array(value))
          }
        }
        return controller.desiredSize > 0
      },
      async cancel (reason) {
        await iterator.return()
      }
    })
  } else if (!stream) {
    // TODO: Spec doesn't say anything about this?
    stream = new ReadableStream({
      async pull (controller) {
        controller.enqueue(
          typeof source === 'string' ? new TextEncoder().encode(source) : source
        )
        queueMicrotask(() => {
          controller.close()
        })
      }
    })
  }

  // 9. Let body be a body whose stream is stream, source is source,
  // and length is length.
  const body = { stream, source, length }

  // 10. Return body and Content-Type.
  return [body, contentType]
}

// https://fetch.spec.whatwg.org/#bodyinit-safely-extract
function safelyExtractBody (object, keepalive = false) {
  if (!ReadableStream) {
    // istanbul ignore next
    ReadableStream = require('stream/web').ReadableStream
  }

  // To safely extract a body and a `Content-Type` value from
  // a byte sequence or BodyInit object object, run these steps:

  // 1. If object is a ReadableStream object, then:
  if (object instanceof ReadableStream) {
    // Assert: object is neither disturbed nor locked.
    // istanbul ignore next
    assert(!util.isDisturbed(object), 'disturbed')
    // istanbul ignore next
    assert(!object.locked, 'locked')
  }

  // 2. Return the results of extracting object.
  return extractBody(object, keepalive)
}

function cloneBody (body) {
  // To clone a body body, run these steps:

  // https://fetch.spec.whatwg.org/#concept-body-clone

  // 1. Let « out1, out2 » be the result of teeing body’s stream.
  const [out1, out2] = body.stream.tee()

  // 2. Set body’s stream to out1.
  body.stream = out1

  // 3. Return a body whose stream is out2 and other members are copied from body.
  return {
    stream: out2,
    length: body.length,
    source: body.source
  }
}

async function * consumeBody (body) {
  if (body) {
    if (isUint8Array(body)) {
      yield body
    } else {
      const stream = body.stream

      if (util.isDisturbed(stream)) {
        throw new TypeError('disturbed')
      }

      if (stream.locked) {
        throw new TypeError('locked')
      }

      // Compat.
      stream[kBodyUsed] = true

      yield * stream
    }
  }
}

function bodyMixinMethods (instance) {
  const methods = {
    async blob () {
      if (!(this instanceof instance)) {
        throw new TypeError('Illegal invocation')
      }

      const chunks = []

      for await (const chunk of consumeBody(this[kState].body)) {
        if (!isUint8Array(chunk)) {
          throw new TypeError('Expected Uint8Array chunk')
        }

        // Assemble one final large blob with Uint8Array's can exhaust memory.
        // That's why we create create multiple blob's and using references
        chunks.push(new Blob([chunk]))
      }

      return new Blob(chunks, { type: this.headers.get('Content-Type') || '' })
    },

    async arrayBuffer () {
      if (!(this instanceof instance)) {
        throw new TypeError('Illegal invocation')
      }

      const contentLength = this.headers.get('content-length')
      const encoded = this.headers.has('content-encoding')

      // if we have content length and no encoding, then we can
      // pre allocate the buffer and just read the data into it
      if (!encoded && contentLength) {
        const buffer = new Uint8Array(contentLength)
        let offset = 0

        for await (const chunk of consumeBody(this[kState].body)) {
          if (!isUint8Array(chunk)) {
            throw new TypeError('Expected Uint8Array chunk')
          }

          buffer.set(chunk, offset)
          offset += chunk.length
        }

        return buffer.buffer
      }

      // if we don't have content length, then we have to allocate 2x the
      // size of the body, once for consumed data, and once for the final buffer

      // This could be optimized by using growable ArrayBuffer, but it's not
      // implemented yet. https://github.com/tc39/proposal-resizablearraybuffer

      const chunks = []
      let size = 0

      for await (const chunk of consumeBody(this[kState].body)) {
        if (!isUint8Array(chunk)) {
          throw new TypeError('Expected Uint8Array chunk')
        }

        chunks.push(chunk)
        size += chunk.byteLength
      }

      const buffer = new Uint8Array(size)
      let offset = 0

      for (const chunk of chunks) {
        buffer.set(chunk, offset)
        offset += chunk.byteLength
      }

      return buffer.buffer
    },

    async text () {
      if (!(this instanceof instance)) {
        throw new TypeError('Illegal invocation')
      }

      let result = ''
      const textDecoder = new TextDecoder()

      for await (const chunk of consumeBody(this[kState].body)) {
        if (!isUint8Array(chunk)) {
          throw new TypeError('Expected Uint8Array chunk')
        }

        result += textDecoder.decode(chunk, { stream: true })
      }

      // flush
      result += textDecoder.decode()

      return result
    },

    async json () {
      if (!(this instanceof instance)) {
        throw new TypeError('Illegal invocation')
      }

      return JSON.parse(await this.text())
    },

    async formData () {
      if (!(this instanceof instance)) {
        throw new TypeError('Illegal invocation')
      }

      const contentType = this.headers.get('Content-Type')

      // If mimeType’s essence is "multipart/form-data", then:
      if (/multipart\/form-data/.test(contentType)) {
        throw new NotSupportedError('multipart/form-data not supported')
      } else if (/application\/x-www-form-urlencoded/.test(contentType)) {
        // Otherwise, if mimeType’s essence is "application/x-www-form-urlencoded", then:

        // 1. Let entries be the result of parsing bytes.
        let entries
        try {
          let text = ''
          // application/x-www-form-urlencoded parser will keep the BOM.
          // https://url.spec.whatwg.org/#concept-urlencoded-parser
          const textDecoder = new TextDecoder('utf-8', { ignoreBOM: true })
          for await (const chunk of consumeBody(this[kState].body)) {
            if (!isUint8Array(chunk)) {
              throw new TypeError('Expected Uint8Array chunk')
            }
            text += textDecoder.decode(chunk, { stream: true })
          }
          text += textDecoder.decode()
          entries = new URLSearchParams(text)
        } catch (err) {
          // istanbul ignore next: Unclear when new URLSearchParams can fail on a string.
          // 2. If entries is failure, then throw a TypeError.
          throw Object.assign(new TypeError(), { cause: err })
        }

        // 3. Return a new FormData object whose entries are entries.
        const formData = new FormData()
        for (const [name, value] of entries) {
          formData.append(name, value)
        }
        return formData
      } else {
        // Otherwise, throw a TypeError.
        webidl.errors.exception({
          header: `${instance.name}.formData`,
          value: 'Could not parse content as FormData.'
        })
      }
    }
  }

  return methods
}

const properties = {
  body: {
    enumerable: true,
    get () {
      if (!this || !this[kState]) {
        throw new TypeError('Illegal invocation')
      }

      return this[kState].body ? this[kState].body.stream : null
    }
  },
  bodyUsed: {
    enumerable: true,
    get () {
      if (!this || !this[kState]) {
        throw new TypeError('Illegal invocation')
      }

      return !!this[kState].body && util.isDisturbed(this[kState].body.stream)
    }
  }
}

function mixinBody (prototype) {
  Object.assign(prototype.prototype, bodyMixinMethods(prototype))
  Object.defineProperties(prototype.prototype, properties)
}

module.exports = {
  extractBody,
  safelyExtractBody,
  cloneBody,
  mixinBody
}
