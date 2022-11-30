'use strict'

const Busboy = require('busboy')
const util = require('../core/util')
const { ReadableStreamFrom, isBlobLike, isReadableStreamLike, readableStreamClose } = require('./util')
const { FormData } = require('./formdata')
const { kState } = require('./symbols')
const { webidl } = require('./webidl')
const { DOMException, structuredClone } = require('./constants')
const { Blob } = require('buffer')
const { kBodyUsed } = require('../core/symbols')
const assert = require('assert')
const { isErrored } = require('../core/util')
const { isUint8Array, isArrayBuffer } = require('util/types')
const { File } = require('./file')
const { StringDecoder } = require('string_decoder')
const { parseMIMEType, serializeAMimeType } = require('./dataURL')

/** @type {globalThis['ReadableStream']} */
let ReadableStream

// https://fetch.spec.whatwg.org/#concept-bodyinit-extract
function extractBody (object, keepalive = false) {
  if (!ReadableStream) {
    ReadableStream = require('stream/web').ReadableStream
  }

  // 1. Let stream be null.
  let stream = null

  // 2. If object is a ReadableStream object, then set stream to object.
  if (object instanceof ReadableStream) {
    stream = object
  } else if (isBlobLike(object)) {
    // 3. Otherwise, if object is a Blob object, set stream to the
    //    result of running object’s get stream.
    stream = object.stream()
  } else {
    // 4. Otherwise, set stream to a new ReadableStream object, and set
    //    up stream.
    stream = new ReadableStream({
      async pull (controller) {
        controller.enqueue(
          typeof source === 'string' ? new TextEncoder().encode(source) : source
        )
        queueMicrotask(() => readableStreamClose(controller))
      },
      start () {},
      type: undefined
    })
  }

  // 5. Assert: stream is a ReadableStream object.
  assert(isReadableStreamLike(stream))

  // 6. Let action be null.
  let action = null

  // 7. Let source be null.
  let source = null

  // 8. Let length be null.
  let length = null

  // 9. Let type be null.
  let type = null

  // 10. Switch on object:
  if (typeof object === 'string') {
    // Set source to the UTF-8 encoding of object.
    // Note: setting source to a Uint8Array here breaks some mocking assumptions.
    source = object

    // Set type to `text/plain;charset=UTF-8`.
    type = 'text/plain;charset=UTF-8'
  } else if (object instanceof URLSearchParams) {
    // URLSearchParams

    // spec says to run application/x-www-form-urlencoded on body.list
    // this is implemented in Node.js as apart of an URLSearchParams instance toString method
    // See: https://github.com/nodejs/node/blob/e46c680bf2b211bbd52cf959ca17ee98c7f657f5/lib/internal/url.js#L490
    // and https://github.com/nodejs/node/blob/e46c680bf2b211bbd52cf959ca17ee98c7f657f5/lib/internal/url.js#L1100

    // Set source to the result of running the application/x-www-form-urlencoded serializer with object’s list.
    source = object.toString()

    // Set type to `application/x-www-form-urlencoded;charset=UTF-8`.
    type = 'application/x-www-form-urlencoded;charset=UTF-8'
  } else if (isArrayBuffer(object)) {
    // BufferSource/ArrayBuffer

    // Set source to a copy of the bytes held by object.
    source = new Uint8Array(object.slice())
  } else if (ArrayBuffer.isView(object)) {
    // BufferSource/ArrayBufferView

    // Set source to a copy of the bytes held by object.
    source = new Uint8Array(object.buffer.slice(object.byteOffset, object.byteOffset + object.byteLength))
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

          yield * value.stream()

          // '\r\n' encoded
          yield new Uint8Array([13, 10])
        }
      }

      yield enc.encode(`--${boundary}--`)
    }

    // Set source to object.
    source = object

    // Set length to unclear, see html/6424 for improving this.
    // TODO

    // Set type to `multipart/form-data; boundary=`,
    // followed by the multipart/form-data boundary string generated
    // by the multipart/form-data encoding algorithm.
    type = 'multipart/form-data; boundary=' + boundary
  } else if (isBlobLike(object)) {
    // Blob

    // Set source to object.
    source = object

    // Set length to object’s size.
    length = object.size

    // If object’s type attribute is not the empty byte sequence, set
    // type to its value.
    if (object.type) {
      type = object.type
    }
  } else if (object instanceof Uint8Array) {
    // byte sequence

    // Set source to object.
    source = object
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
  }

  // 11. If source is a byte sequence, then set action to a
  // step that returns source and length to source’s length.
  if (typeof source === 'string' || util.isBuffer(source)) {
    length = Buffer.byteLength(source)
  }

  // 12. If action is non-null, then run these steps in in parallel:
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
      },
      type: undefined
    })
  }

  // 13. Let body be a body whose stream is stream, source is source,
  // and length is length.
  const body = { stream, source, length }

  // 14. Return (body, type).
  return [body, type]
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
    assert(!util.isDisturbed(object), 'The body has already been consumed.')
    // istanbul ignore next
    assert(!object.locked, 'The stream is locked.')
  }

  // 2. Return the results of extracting object.
  return extractBody(object, keepalive)
}

function cloneBody (body) {
  // To clone a body body, run these steps:

  // https://fetch.spec.whatwg.org/#concept-body-clone

  // 1. Let « out1, out2 » be the result of teeing body’s stream.
  const [out1, out2] = body.stream.tee()
  const out2Clone = structuredClone(out2, { transfer: [out2] })
  // This, for whatever reasons, unrefs out2Clone which allows
  // the process to exit by itself.
  const [, finalClone] = out2Clone.tee()

  // 2. Set body’s stream to out1.
  body.stream = out1

  // 3. Return a body whose stream is out2 and other members are copied from body.
  return {
    stream: finalClone,
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
        throw new TypeError('The body has already been consumed.')
      }

      if (stream.locked) {
        throw new TypeError('The stream is locked.')
      }

      // Compat.
      stream[kBodyUsed] = true

      yield * stream
    }
  }
}

function throwIfAborted (state) {
  if (state.aborted) {
    throw new DOMException('The operation was aborted.', 'AbortError')
  }
}

function bodyMixinMethods (instance) {
  const methods = {
    blob () {
      // The blob() method steps are to return the result of
      // running consume body with this and Blob.
      return specConsumeBody(this, 'Blob', instance)
    },

    arrayBuffer () {
      // The arrayBuffer() method steps are to return the
      // result of running consume body with this and ArrayBuffer.
      return specConsumeBody(this, 'ArrayBuffer', instance)
    },

    text () {
      // The text() method steps are to return the result of
      // running consume body with this and text.
      return specConsumeBody(this, 'text', instance)
    },

    json () {
      // The json() method steps are to return the result of
      // running consume body with this and JSON.
      return specConsumeBody(this, 'JSON', instance)
    },

    async formData () {
      webidl.brandCheck(this, instance)

      throwIfAborted(this[kState])

      const contentType = this.headers.get('Content-Type')

      // If mimeType’s essence is "multipart/form-data", then:
      if (/multipart\/form-data/.test(contentType)) {
        const headers = {}
        for (const [key, value] of this.headers) headers[key.toLowerCase()] = value

        const responseFormData = new FormData()

        let busboy

        try {
          busboy = Busboy({ headers })
        } catch (err) {
          // Error due to headers:
          throw Object.assign(new TypeError(), { cause: err })
        }

        busboy.on('field', (name, value) => {
          responseFormData.append(name, value)
        })
        busboy.on('file', (name, value, info) => {
          const { filename, encoding, mimeType } = info
          const chunks = []

          if (encoding.toLowerCase() === 'base64') {
            let base64chunk = ''

            value.on('data', (chunk) => {
              base64chunk += chunk.toString().replace(/[\r\n]/gm, '')

              const end = base64chunk.length - base64chunk.length % 4
              chunks.push(Buffer.from(base64chunk.slice(0, end), 'base64'))

              base64chunk = base64chunk.slice(end)
            })
            value.on('end', () => {
              chunks.push(Buffer.from(base64chunk, 'base64'))
              responseFormData.append(name, new File(chunks, filename, { type: mimeType }))
            })
          } else {
            value.on('data', (chunk) => {
              chunks.push(chunk)
            })
            value.on('end', () => {
              responseFormData.append(name, new File(chunks, filename, { type: mimeType }))
            })
          }
        })

        const busboyResolve = new Promise((resolve, reject) => {
          busboy.on('finish', resolve)
          busboy.on('error', (err) => reject(new TypeError(err)))
        })

        if (this.body !== null) for await (const chunk of consumeBody(this[kState].body)) busboy.write(chunk)
        busboy.end()
        await busboyResolve

        return responseFormData
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
        // Wait a tick before checking if the request has been aborted.
        // Otherwise, a TypeError can be thrown when an AbortError should.
        await Promise.resolve()

        throwIfAborted(this[kState])

        // Otherwise, throw a TypeError.
        throw webidl.errors.exception({
          header: `${instance.name}.formData`,
          message: 'Could not parse content as FormData.'
        })
      }
    }
  }

  return methods
}

function mixinBody (prototype) {
  Object.assign(prototype.prototype, bodyMixinMethods(prototype))
}

// https://fetch.spec.whatwg.org/#concept-body-consume-body
async function specConsumeBody (object, type, instance) {
  webidl.brandCheck(object, instance)

  throwIfAborted(object[kState])

  // 1. If object is unusable, then return a promise rejected
  //    with a TypeError.
  if (bodyUnusable(object[kState].body)) {
    throw new TypeError('Body is unusable')
  }

  // 2. Let promise be a promise resolved with an empty byte
  //    sequence.
  let promise

  // 3. If object’s body is non-null, then set promise to the
  //    result of fully reading body as promise given object’s
  //    body.
  if (object[kState].body != null) {
    promise = await fullyReadBodyAsPromise(object[kState].body)
  } else {
    // step #2
    promise = { size: 0, bytes: [new Uint8Array()] }
  }

  // 4. Let steps be to return the result of package data with
  //    the first argument given, type, and object’s MIME type.
  const mimeType = type === 'Blob' || type === 'FormData'
    ? bodyMimeType(object)
    : undefined

  // 5. Return the result of upon fulfillment of promise given
  //    steps.
  return packageData(promise, type, mimeType)
}

/**
 * @see https://fetch.spec.whatwg.org/#concept-body-package-data
 * @param {{ size: number, bytes: Uint8Array[] }} bytes
 * @param {string} type
 * @param {ReturnType<typeof parseMIMEType>|undefined} mimeType
 */
function packageData ({ bytes, size }, type, mimeType) {
  switch (type) {
    case 'ArrayBuffer': {
      // Return a new ArrayBuffer whose contents are bytes.
      const uint8 = new Uint8Array(size)
      let offset = 0

      for (const chunk of bytes) {
        uint8.set(chunk, offset)
        offset += chunk.byteLength
      }

      return uint8.buffer
    }
    case 'Blob': {
      if (mimeType === 'failure') {
        mimeType = ''
      } else if (mimeType) {
        mimeType = serializeAMimeType(mimeType)
      }

      // Return a Blob whose contents are bytes and type attribute
      // is mimeType.
      return new Blob(bytes, { type: mimeType })
    }
    case 'JSON': {
      // Return the result of running parse JSON from bytes on bytes.
      return JSON.parse(utf8DecodeBytes(bytes))
    }
    case 'text': {
      // 1. Return the result of running UTF-8 decode on bytes.
      return utf8DecodeBytes(bytes)
    }
  }
}

// https://fetch.spec.whatwg.org/#body-unusable
function bodyUnusable (body) {
  // An object including the Body interface mixin is
  // said to be unusable if its body is non-null and
  // its body’s stream is disturbed or locked.
  return body != null && (body.stream.locked || util.isDisturbed(body.stream))
}

// https://fetch.spec.whatwg.org/#fully-reading-body-as-promise
async function fullyReadBodyAsPromise (body) {
  // 1. Let reader be the result of getting a reader for body’s
  //    stream. If that threw an exception, then return a promise
  //    rejected with that exception.
  const reader = body.stream.getReader()

  // 2. Return the result of reading all bytes from reader.
  /** @type {Uint8Array[]} */
  const bytes = []
  let size = 0

  while (true) {
    const { done, value } = await reader.read()

    if (done) {
      break
    }

    // https://streams.spec.whatwg.org/#read-loop
    // If chunk is not a Uint8Array object, reject promise with
    // a TypeError and abort these steps.
    if (!isUint8Array(value)) {
      throw new TypeError('Value is not a Uint8Array.')
    }

    bytes.push(value)
    size += value.byteLength
  }

  return { size, bytes }
}

/**
 * @see https://encoding.spec.whatwg.org/#utf-8-decode
 * @param {Uint8Array[]} ioQueue
 */
function utf8DecodeBytes (ioQueue) {
  if (ioQueue.length === 0) {
    return ''
  }

  // 1. Let buffer be the result of peeking three bytes
  //    from ioQueue, converted to a byte sequence.
  const buffer = ioQueue[0]

  // 2. If buffer is 0xEF 0xBB 0xBF, then read three
  //    bytes from ioQueue. (Do nothing with those bytes.)
  if (buffer[0] === 0xEF && buffer[1] === 0xBB && buffer[2] === 0xBF) {
    ioQueue[0] = ioQueue[0].subarray(3)
  }

  // 3. Process a queue with an instance of UTF-8’s
  //    decoder, ioQueue, output, and "replacement".
  const decoder = new StringDecoder('utf-8')
  let output = ''

  for (const chunk of ioQueue) {
    output += decoder.write(chunk)
  }

  output += decoder.end()

  // 4. Return output.
  return output
}

/**
 * @see https://fetch.spec.whatwg.org/#concept-body-mime-type
 * @param {import('./response').Response|import('./request').Request} object
 */
function bodyMimeType (object) {
  const { headersList } = object[kState]
  const contentType = headersList.get('content-type')

  if (contentType === null) {
    return 'failure'
  }

  return parseMIMEType(contentType)
}

module.exports = {
  extractBody,
  safelyExtractBody,
  cloneBody,
  mixinBody
}
