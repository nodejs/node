'use strict'

/* istanbul ignore file: only for Node 12 */

const { kConnected, kSize } = require('../core/symbols')

class CompatWeakRef {
  constructor (value) {
    this.value = value
  }

  deref () {
    return this.value[kConnected] === 0 && this.value[kSize] === 0
      ? undefined
      : this.value
  }
}

class CompatFinalizer {
  constructor (finalizer) {
    this.finalizer = finalizer
  }

  register (dispatcher, key) {
    dispatcher.on('disconnect', () => {
      if (dispatcher[kConnected] === 0 && dispatcher[kSize] === 0) {
        this.finalizer(key)
      }
    })
  }
}

module.exports = function () {
  return {
    WeakRef: global.WeakRef || CompatWeakRef,
    FinalizationRegistry: global.FinalizationRegistry || CompatFinalizer
  }
}
