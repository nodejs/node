'use strict'

const { InvalidArgumentError } = require('../core/errors')
const Client = require('./client')

class H2CClient extends Client {
  constructor (origin, clientOpts) {
    if (typeof origin === 'string') {
      origin = new URL(origin)
    }

    if (origin.protocol !== 'http:') {
      throw new InvalidArgumentError(
        'h2c-client: Only h2c protocol is supported'
      )
    }

    const { maxConcurrentStreams, pipelining, ...opts } =
            clientOpts ?? {}
    const defaultMaxConcurrentStreams = maxConcurrentStreams ?? 100
    let defaultPipelining = 100

    if (
      maxConcurrentStreams != null &&
            (!Number.isInteger(maxConcurrentStreams) ||
            maxConcurrentStreams < 1)
    ) {
      throw new InvalidArgumentError('maxConcurrentStreams must be a positive integer, greater than 0')
    }

    if (pipelining != null && Number.isInteger(pipelining) && pipelining > 0) {
      defaultPipelining = pipelining
    }

    if (defaultPipelining > defaultMaxConcurrentStreams) {
      throw new InvalidArgumentError(
        'h2c-client: pipelining cannot be greater than maxConcurrentStreams'
      )
    }

    super(origin, {
      ...opts,
      maxConcurrentStreams: defaultMaxConcurrentStreams,
      pipelining: defaultPipelining,
      allowH2: true,
      useH2c: true
    })
  }
}

module.exports = H2CClient
