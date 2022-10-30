'use strict'

const { webidl } = require('../fetch/webidl')

const kState = Symbol('ProgressEvent state')

/**
 * @see https://xhr.spec.whatwg.org/#progressevent
 */
class ProgressEvent extends Event {
  constructor (type, eventInitDict = {}) {
    type = webidl.converters.DOMString(type)
    eventInitDict = webidl.converters.ProgressEventInit(eventInitDict ?? {})

    super(type, eventInitDict)

    this[kState] = {
      lengthComputable: eventInitDict.lengthComputable,
      loaded: eventInitDict.loaded,
      total: eventInitDict.total
    }
  }

  get lengthComputable () {
    if (!(this instanceof ProgressEvent)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].lengthComputable
  }

  get loaded () {
    if (!(this instanceof ProgressEvent)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].loaded
  }

  get total () {
    if (!(this instanceof ProgressEvent)) {
      throw new TypeError('Illegal invocation')
    }

    return this[kState].total
  }
}

webidl.converters.ProgressEventInit = webidl.dictionaryConverter([
  {
    key: 'lengthComputable',
    converter: webidl.converters.boolean,
    defaultValue: false
  },
  {
    key: 'loaded',
    converter: webidl.converters['unsigned long long'],
    defaultValue: 0
  },
  {
    key: 'total',
    converter: webidl.converters['unsigned long long'],
    defaultValue: 0
  },
  {
    key: 'bubbles',
    converter: webidl.converters.boolean,
    defaultValue: false
  },
  {
    key: 'cancelable',
    converter: webidl.converters.boolean,
    defaultValue: false
  },
  {
    key: 'composed',
    converter: webidl.converters.boolean,
    defaultValue: false
  }
])

module.exports = {
  ProgressEvent
}
