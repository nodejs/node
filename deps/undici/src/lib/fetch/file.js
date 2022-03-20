'use strict'

const { Blob } = require('buffer')
const { kState } = require('./symbols')

class File extends Blob {
  constructor (fileBits, fileName, options = {}) {
    // TODO: argument idl type check

    // The File constructor is invoked with two or three parameters, depending
    // on whether the optional dictionary parameter is used. When the File()
    // constructor is invoked, user agents must run the following steps:

    // 1. Let bytes be the result of processing blob parts given fileBits and
    // options.
    // TODO

    // 2. Let n be the fileName argument to the constructor.
    const n = fileName

    // 3. Process FilePropertyBag dictionary argument by running the following
    // substeps:

    //    1. If the type member is provided and is not the empty string, let t
    //    be set to the type dictionary member. If t contains any characters
    //    outside the range U+0020 to U+007E, then set t to the empty string
    //    and return from these substeps.
    //    TODO
    const t = options.type

    //    2. Convert every character in t to ASCII lowercase.
    //    TODO

    //    3. If the lastModified member is provided, let d be set to the
    //    lastModified dictionary member. If it is not provided, set d to the
    //    current date and time represented as the number of milliseconds since
    //    the Unix Epoch (which is the equivalent of Date.now() [ECMA-262]).
    const d = options.lastModified ?? Date.now()

    // 4. Return a new File object F such that:
    // F refers to the bytes byte sequence.
    // F.size is set to the number of total bytes in bytes.
    // F.name is set to n.
    // F.type is set to t.
    // F.lastModified is set to d.
    // TODO

    super(fileBits, { type: t })
    this[kState] = {
      name: n,
      lastModified: d
    }
  }

  get name () {
    if (!(this instanceof File)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].name
  }

  get lastModified () {
    if (!(this instanceof File)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].lastModified
  }

  get [Symbol.toStringTag] () {
    if (!(this instanceof File)) {
      throw new TypeError('Illegal invocation')
    }

    return this.constructor.name
  }
}

class FileLike {
  constructor (blobLike, fileName, options = {}) {
    // TODO: argument idl type check

    // The File constructor is invoked with two or three parameters, depending
    // on whether the optional dictionary parameter is used. When the File()
    // constructor is invoked, user agents must run the following steps:

    // 1. Let bytes be the result of processing blob parts given fileBits and
    // options.

    // 2. Let n be the fileName argument to the constructor.
    const n = fileName

    // 3. Process FilePropertyBag dictionary argument by running the following
    // substeps:

    //    1. If the type member is provided and is not the empty string, let t
    //    be set to the type dictionary member. If t contains any characters
    //    outside the range U+0020 to U+007E, then set t to the empty string
    //    and return from these substeps.
    //    TODO
    const t = options.type

    //    2. Convert every character in t to ASCII lowercase.
    //    TODO

    //    3. If the lastModified member is provided, let d be set to the
    //    lastModified dictionary member. If it is not provided, set d to the
    //    current date and time represented as the number of milliseconds since
    //    the Unix Epoch (which is the equivalent of Date.now() [ECMA-262]).
    const d = options.lastModified ?? Date.now()

    // 4. Return a new File object F such that:
    // F refers to the bytes byte sequence.
    // F.size is set to the number of total bytes in bytes.
    // F.name is set to n.
    // F.type is set to t.
    // F.lastModified is set to d.

    this[kState] = {
      blobLike,
      name: n,
      type: t,
      lastModified: d
    }
  }

  stream (...args) {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].blobLike.stream(...args)
  }

  arrayBuffer (...args) {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].blobLike.arrayBuffer(...args)
  }

  slice (...args) {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].blobLike.slice(...args)
  }

  text (...args) {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].blobLike.text(...args)
  }

  get size () {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].blobLike.size
  }

  get type () {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].blobLike.type
  }

  get name () {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].name
  }

  get lastModified () {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].lastModified
  }

  get [Symbol.toStringTag] () {
    if (!(this instanceof FileLike)) {
      throw new TypeError('Illegal invocation')
    }

    return 'File'
  }
}

module.exports = { File: globalThis.File ?? File, FileLike }
