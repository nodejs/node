'use strict'

const { WebsocketFrameSend } = require('./frame')
const { opcodes, sendHints } = require('./constants')

/** @type {Uint8Array} */
const FastBuffer = Buffer[Symbol.species]

class SendQueue {
  #queued = new Set()
  #size = 0

  /** @type {import('net').Socket} */
  #socket

  constructor (socket) {
    this.#socket = socket
  }

  add (item, cb, hint) {
    if (hint !== sendHints.blob) {
      const data = clone(item, hint)

      if (this.#size === 0) {
        this.#dispatch(data, cb, hint)
      } else {
        this.#queued.add([data, cb, true, hint])
        this.#size++

        this.#run()
      }

      return
    }

    const promise = item.arrayBuffer()
    const queue = [null, cb, false, hint]
    promise.then((ab) => {
      queue[0] = clone(ab, hint)
      queue[2] = true

      this.#run()
    })

    this.#queued.add(queue)
    this.#size++
  }

  #run () {
    for (const queued of this.#queued) {
      const [data, cb, done, hint] = queued

      if (!done) return

      this.#queued.delete(queued)
      this.#size--

      this.#dispatch(data, cb, hint)
    }
  }

  #dispatch (data, cb, hint) {
    const frame = new WebsocketFrameSend()
    const opcode = hint === sendHints.string ? opcodes.TEXT : opcodes.BINARY

    frame.frameData = data
    const buffer = frame.createFrame(opcode)

    this.#socket.write(buffer, cb)
  }
}

function clone (data, hint) {
  switch (hint) {
    case sendHints.string:
      return Buffer.from(data)
    case sendHints.arrayBuffer:
    case sendHints.blob:
      return new FastBuffer(data)
    case sendHints.typedArray:
      return Buffer.copyBytesFrom(data)
  }
}

module.exports = { SendQueue }
