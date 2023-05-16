// An initial implementation for a feature that will hopefully exist in tap
// https://github.com/tapjs/node-tap/issues/789
// This file is only used in tests but it is still tested itself.
// Hopefully it can be removed for a feature in tap in the future

const sep = '.'
const escapeSep = '"'
const has = (o, k) => Object.prototype.hasOwnProperty.call(o, k)
const opd = (o, k) => Object.getOwnPropertyDescriptor(o, k)
const po = (o) => Object.getPrototypeOf(o)
const pojo = (o) => Object.prototype.toString.call(o) === '[object Object]'
const last = (arr) => arr[arr.length - 1]
const dupes = (arr) => arr.filter((k, i) => arr.indexOf(k) !== i)
const dupesStartsWith = (arr) => arr.filter((k1) => arr.some((k2) => k2.startsWith(k1 + sep)))

const splitLastSep = (str) => {
  let escaped = false
  for (let i = str.length - 1; i >= 0; i--) {
    const c = str[i]
    const cp = str[i + 1]
    const cn = str[i - 1]
    if (!escaped && c === escapeSep && (cp == null || cp === sep)) {
      escaped = true
      continue
    }
    if (escaped && c === escapeSep && cn === sep) {
      escaped = false
      continue
    }
    if (!escaped && c === sep) {
      return [
        str.slice(0, i),
        str.slice(i + 1).replace(new RegExp(`^${escapeSep}(.*)${escapeSep}$`), '$1'),
      ]
    }
  }
  return [str]
}

// A weird getter that can look up keys on nested objects but also
// match keys with dots in their names, eg { 'process.env': { TERM: 'a' } }
// can be looked up with the key 'process.env.TERM'
const get = (obj, key, childKey = '') => {
  if (has(obj, key)) {
    return childKey ? get(obj[key], childKey) : obj[key]
  }
  const split = splitLastSep(key)
  if (split.length === 2) {
    const [parentKey, prefix] = split
    return get(
      obj,
      parentKey,
      prefix + (childKey && sep + childKey)
    )
  }
}

// Map an object to an array of nested keys separated by dots
// { a: 1, b: { c: 2, d: [1] } } => ['a', 'b.c', 'b.d']
const getKeys = (values, p = '', acc = []) =>
  Object.entries(values).reduce((memo, [k, value]) => {
    const key = p ? [p, k].join(sep) : k
    return pojo(value) ? getKeys(value, key, memo) : memo.concat(key)
  }, acc)

// Walk prototype chain to get first available descriptor. This is necessary
// to get the current property descriptor for things like `process.on`.
// Since `opd(process, 'on') === undefined` but if you
// walk up the prototype chain you get the original descriptor
// `opd(po(po(process)), 'on') === { value, ... }`
const protoDescriptor = (obj, key) => {
  let descriptor
  // i always wanted to assign variables in a while loop's condition
  // i thought it would feel better than this
  while (!(descriptor = opd(obj, key))) {
    if (!(obj = po(obj))) {
      break
    }
  }
  return descriptor
}

// Path can be different cases across platform so get the original case
// of the path before anything is changed
// XXX: other special cases to handle?
const specialCaseKeys = (() => {
  const originalKeys = {
    PATH: process.env.PATH ? 'PATH' : process.env.Path ? 'Path' : 'path',
  }
  return (key) => {
    switch (key.toLowerCase()) {
      case 'process.env.path':
        return originalKeys.PATH
    }
  }
})()

const _setGlobal = Symbol('setGlobal')
const _nextDescriptor = Symbol('nextDescriptor')

class DescriptorStack {
  #stack = []
  #global = null
  #valueKey = null
  #defaultDescriptor = { configurable: true, writable: true, enumerable: true }
  #delete = () => ({ DELETE: true })
  #isDelete = (o) => o && o.DELETE === true

  constructor (key) {
    const keys = splitLastSep(key)
    this.#global = keys.length === 1 ? global : get(global, keys[0])
    this.#valueKey = specialCaseKeys(key) || last(keys)
    // If the global object doesnt return a descriptor for the key
    // then we mark it for deletion on teardown
    this.#stack = [
      protoDescriptor(this.#global, this.#valueKey) || this.#delete(),
    ]
  }

  add (value) {
    // This must be a unique object so we can find it later via indexOf
    // That's why delete/nextDescriptor create new objects
    const nextDescriptor = this[_nextDescriptor](value)
    this.#stack.push(this[_setGlobal](nextDescriptor))

    return () => {
      const index = this.#stack.indexOf(nextDescriptor)
      // If the stack doesnt contain the descriptor anymore
      // than do nothing. This keeps the reset function indempotent
      if (index > -1) {
        // Resetting removes a descriptor from the stack
        this.#stack.splice(index, 1)
        // But we always reset to what is now the most recent in case
        // resets are being called manually out of order
        this[_setGlobal](last(this.#stack))
      }
    }
  }

  reset () {
    // Everything could be reset manually so only
    // teardown if we have an initial descriptor left
    // and then delete the rest of the stack
    if (this.#stack.length) {
      this[_setGlobal](this.#stack[0])
      this.#stack.length = 0
    }
  }

  [_setGlobal] (d) {
    if (this.#isDelete(d)) {
      delete this.#global[this.#valueKey]
    } else {
      Object.defineProperty(this.#global, this.#valueKey, d)
    }
    return d
  }

  [_nextDescriptor] (value) {
    if (value === undefined) {
      return this.#delete()
    }
    const d = last(this.#stack)
    return {
      // If the previous descriptor was one to delete the property
      // then use the default descriptor as the base
      ...(this.#isDelete(d) ? this.#defaultDescriptor : d),
      ...(d && d.get ? { get: () => value } : { value }),
    }
  }
}

class MockGlobals {
  #descriptors = {}

  register (globals, { replace = false } = {}) {
    // Replace means dont merge in object values but replace them instead
    // so we only get top level keys instead of walking the obj
    const keys = replace ? Object.keys(globals) : getKeys(globals)

    // An error state where due to object mode there are multiple global
    // values to be set with the same key
    const duplicates = dupes(keys)
    if (duplicates.length) {
      throw new Error(`mockGlobals was called with duplicate keys: ${duplicates}`)
    }

    // Another error where when in replace mode overlapping keys are set like
    // process and process.stdout which would cause unexpected behavior
    const overlapping = dupesStartsWith(keys)
    if (overlapping.length) {
      const message = overlapping
        .map((k) => `${k} -> ${keys.filter((kk) => kk.startsWith(k + sep))}`)
      throw new Error(`mockGlobals was called with overlapping keys: ${message}`)
    }

    // Set each property passed in and return fns to reset them
    // Return an object with each path as a key for manually resetting in each test
    return keys.reduce((acc, key) => {
      const desc = this.#descriptors[key] || (this.#descriptors[key] = new DescriptorStack(key))
      acc[key] = desc.add(get(globals, key))
      return acc
    }, {})
  }

  teardown (key) {
    if (!key) {
      Object.values(this.#descriptors).forEach((d) => d.reset())
      return
    }
    this.#descriptors[key].reset()
  }
}

// Each test has one instance of MockGlobals so it can be called multiple times per test
// Its a weak map so that it can be garbage collected along with the tap tests without
// needing to explicitly call cache.delete
const cache = new WeakMap()

module.exports = (t, globals, options) => {
  let instance = cache.get(t)
  if (!instance) {
    instance = cache.set(t, new MockGlobals()).get(t)
    // Teardown only needs to be initialized once. The instance
    // will keep track of its own state during the test
    t.teardown(() => instance.teardown())
  }

  return {
    // Reset contains only the functions to reset the globals
    // set by this function call
    reset: instance.register(globals, options),
    // Teardown will reset across all calls tied to this test
    teardown: () => instance.teardown(),
  }
}
