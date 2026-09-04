'use strict'

const Dispatcher = require('./dispatcher')
const RetryHandler = require('../handler/retry-handler')
const { kOriginless, kUrl } = require('../core/symbols')

class RetryAgent extends Dispatcher {
  #agent = null
  #options = null
  constructor (agent, options = {}) {
    super(options)
    this.#agent = agent
    this.#options = options
    this[kUrl] = agent[kUrl]
    this[kOriginless] = agent[kOriginless]
  }

  dispatch (opts, handler) {
    const retry = new RetryHandler({
      ...opts,
      retryOptions: this.#options
    }, {
      dispatch: this.#agent.dispatch.bind(this.#agent),
      handler
    })
    return this.#agent.dispatch(opts, retry)
  }

  close () {
    return this.#agent.close()
  }

  destroy () {
    return this.#agent.destroy()
  }
}

module.exports = RetryAgent
