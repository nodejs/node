const perf = typeof performance === 'object' && performance &&
  typeof performance.now === 'function' ? performance : Date

const warned = new Set()
const deprecatedOption = (opt, msg) => {
  const code = `LRU_CACHE_OPTION_${opt}`
  if (shouldWarn(code)) {
    warn(code, `The ${opt} option is deprecated. ${msg}`, LRUCache)
  }
}
const deprecatedMethod = (method, msg) => {
  const code = `LRU_CACHE_METHOD_${method}`
  if (shouldWarn(code)) {
    const { prototype } = LRUCache
    const { get } = Object.getOwnPropertyDescriptor(prototype, method)
    warn(code, `The ${method} method is deprecated. ${msg}`, get)
  }
}
const deprecatedProperty = (field, msg) => {
  const code = `LRU_CACHE_PROPERTY_${field}`
  if (shouldWarn(code)) {
    const { prototype } = LRUCache
    const { get } = Object.getOwnPropertyDescriptor(prototype, field)
    warn(code, `The ${field} property is deprecated. ${msg}`, get)
  }
}
const shouldWarn = (code) => !(process.noDeprecation || warned.has(code))
const warn = (code, msg, fn) => {
  warned.add(code)
  process.emitWarning(msg, 'DeprecationWarning', code, fn)
}

const isPosInt = n => n && n === Math.floor(n) && n > 0 && isFinite(n)

/* istanbul ignore next - This is a little bit ridiculous, tbh.
 * The maximum array length is 2^32-1 or thereabouts on most JS impls.
 * And well before that point, you're caching the entire world, I mean,
 * that's ~32GB of just integers for the next/prev links, plus whatever
 * else to hold that many keys and values.  Just filling the memory with
 * zeroes at init time is brutal when you get that big.
 * But why not be complete?
 * Maybe in the future, these limits will have expanded. */
const getUintArray = max => !isPosInt(max) ? null
: max <= Math.pow(2, 8) ? Uint8Array
: max <= Math.pow(2, 16) ? Uint16Array
: max <= Math.pow(2, 32) ? Uint32Array
: max <= Number.MAX_SAFE_INTEGER ? ZeroArray
: null

class ZeroArray extends Array {
  constructor (size) {
    super(size)
    this.fill(0)
  }
}

class Stack {
  constructor (max) {
    const UintArray = getUintArray(max)
    this.heap = new UintArray(max)
    this.length = 0
  }
  push (n) {
    this.heap[this.length++] = n
  }
  pop () {
    return this.heap[--this.length]
  }
}

class LRUCache {
  constructor (options = {}) {
    const {
      max,
      ttl,
      ttlResolution = 1,
      ttlAutopurge,
      updateAgeOnGet,
      allowStale,
      dispose,
      disposeAfter,
      noDisposeOnSet,
      maxSize,
      sizeCalculation,
    } = options

    // deprecated options, don't trigger a warning for getting them if
    // the thing being passed in is another LRUCache we're copying.
    const {
      length,
      maxAge,
      stale,
    } = options instanceof LRUCache ? {} : options

    if (!isPosInt(max)) {
      throw new TypeError('max option must be an integer')
    }

    const UintArray = getUintArray(max)
    if (!UintArray) {
      throw new Error('invalid max value: ' + max)
    }

    this.max = max
    this.maxSize = maxSize || 0
    this.sizeCalculation = sizeCalculation || length
    if (this.sizeCalculation) {
      if (!this.maxSize) {
        throw new TypeError('cannot set sizeCalculation without setting maxSize')
      }
      if (typeof this.sizeCalculation !== 'function') {
        throw new TypeError('sizeCalculating set to non-function')
      }
    }
    this.keyMap = new Map()
    this.keyList = new Array(max).fill(null)
    this.valList = new Array(max).fill(null)
    this.next = new UintArray(max)
    this.prev = new UintArray(max)
    this.head = 0
    this.tail = 0
    this.free = new Stack(max)
    this.initialFill = 1
    this.size = 0

    if (typeof dispose === 'function') {
      this.dispose = dispose
    }
    if (typeof disposeAfter === 'function') {
      this.disposeAfter = disposeAfter
      this.disposed = []
    } else {
      this.disposeAfter = null
      this.disposed = null
    }
    this.noDisposeOnSet = !!noDisposeOnSet

    if (this.maxSize) {
      if (!isPosInt(this.maxSize)) {
        throw new TypeError('maxSize must be a positive integer if specified')
      }
      this.initializeSizeTracking()
    }

    this.allowStale = !!allowStale || !!stale
    this.updateAgeOnGet = !!updateAgeOnGet
    this.ttlResolution = isPosInt(ttlResolution) || ttlResolution === 0
      ? ttlResolution : 1
    this.ttlAutopurge = !!ttlAutopurge
    this.ttl = ttl || maxAge || 0
    if (this.ttl) {
      if (!isPosInt(this.ttl)) {
        throw new TypeError('ttl must be a positive integer if specified')
      }
      this.initializeTTLTracking()
    }

    if (stale) {
      deprecatedOption('stale', 'please use options.allowStale instead')
    }
    if (maxAge) {
      deprecatedOption('maxAge', 'please use options.ttl instead')
    }
    if (length) {
      deprecatedOption('length', 'please use options.sizeCalculation instead')
    }
  }

  initializeTTLTracking () {
    this.ttls = new ZeroArray(this.max)
    this.starts = new ZeroArray(this.max)
    this.setItemTTL = (index, ttl) => {
      this.starts[index] = ttl !== 0 ? perf.now() : 0
      this.ttls[index] = ttl
      if (ttl !== 0 && this.ttlAutopurge) {
        const t = setTimeout(() => {
          if (this.isStale(index)) {
            this.delete(this.keyList[index])
          }
        }, ttl + 1)
        /* istanbul ignore else - unref() not supported on all platforms */
        if (t.unref) {
          t.unref()
        }
      }
    }
    this.updateItemAge = (index) => {
      this.starts[index] = this.ttls[index] !== 0 ? perf.now() : 0
    }
    // debounce calls to perf.now() to 1s so we're not hitting
    // that costly call repeatedly.
    let cachedNow = 0
    const getNow = () => {
      const n = perf.now()
      if (this.ttlResolution > 0) {
        cachedNow = n
        const t = setTimeout(() => cachedNow = 0, this.ttlResolution)
        /* istanbul ignore else - not available on all platforms */
        if (t.unref) {
          t.unref()
        }
      }
      return n
    }
    this.isStale = (index) => {
      return this.ttls[index] !== 0 && this.starts[index] !== 0 &&
        ((cachedNow || getNow()) - this.starts[index] > this.ttls[index])
    }
  }
  updateItemAge (index) {}
  setItemTTL (index, ttl) {}
  isStale (index) { return false }

  initializeSizeTracking () {
    this.calculatedSize = 0
    this.sizes = new ZeroArray(this.max)
    this.removeItemSize = index => this.calculatedSize -= this.sizes[index]
    this.addItemSize = (index, v, k, size, sizeCalculation) => {
      const s = size || (sizeCalculation ? sizeCalculation(v, k) : 0)
      this.sizes[index] = isPosInt(s) ? s : 0
      const maxSize = this.maxSize - this.sizes[index]
      while (this.calculatedSize > maxSize) {
        this.evict()
      }
      this.calculatedSize += this.sizes[index]
    }
    this.delete = k => {
      if (this.size !== 0) {
        const index = this.keyMap.get(k)
        if (index !== undefined) {
          this.calculatedSize -= this.sizes[index]
        }
      }
      return LRUCache.prototype.delete.call(this, k)
    }
  }
  removeItemSize (index) {}
  addItemSize (index, v, k, size, sizeCalculation) {}

  *indexes () {
    if (this.size) {
      for (let i = this.tail; true; i = this.prev[i]) {
        if (!this.isStale(i)) {
          yield i
        }
        if (i === this.head) {
          break
        }
      }
    }
  }
  *rindexes () {
    if (this.size) {
      for (let i = this.head; true; i = this.next[i]) {
        if (!this.isStale(i)) {
          yield i
        }
        if (i === this.tail) {
          break
        }
      }
    }
  }

  *entries () {
    for (const i of this.indexes()) {
      yield [this.keyList[i], this.valList[i]]
    }
  }

  *keys () {
    for (const i of this.indexes()) {
      yield this.keyList[i]
    }
  }

  *values () {
    for (const i of this.indexes()) {
      yield this.valList[i]
    }
  }

  [Symbol.iterator] () {
    return this.entries()
  }

  find (fn, getOptions = {}) {
    for (const i of this.indexes()) {
      if (fn(this.valList[i], this.keyList[i], this)) {
        return this.get(this.keyList[i], getOptions)
      }
    }
  }

  forEach (fn, thisp = this) {
    for (const i of this.indexes()) {
      fn.call(thisp, this.valList[i], this.keyList[i], this)
    }
  }

  rforEach (fn, thisp = this) {
    for (const i of this.rindexes()) {
      fn.call(thisp, this.valList[i], this.keyList[i], this)
    }
  }

  get prune () {
    deprecatedMethod('prune', 'Please use cache.purgeStale() instead.')
    return this.purgeStale
  }

  purgeStale () {
    let deleted = false
    if (this.size) {
      for (let i = this.head; true; i = this.next[i]) {
        const b = i === this.tail
        if (this.isStale(i)) {
          this.delete(this.keyList[i])
          deleted = true
        }
        if (b) {
          break
        }
      }
    }
    return deleted
  }

  dump () {
    const arr = []
    for (const i of this.indexes()) {
      const key = this.keyList[i]
      const value = this.valList[i]
      const entry = { value }
      if (this.ttls) {
        entry.ttl = this.ttls[i]
      }
      if (this.sizes) {
        entry.size = this.sizes[i]
      }
      arr.unshift([key, entry])
    }
    return arr
  }

  load (arr) {
    this.clear()
    for (const [key, entry] of arr) {
      this.set(key, entry.value, entry)
    }
  }

  dispose (v, k, reason) {}

  set (k, v, {
    ttl = this.ttl,
    noDisposeOnSet = this.noDisposeOnSet,
    size = 0,
    sizeCalculation = this.sizeCalculation,
  } = {}) {
    let index = this.size === 0 ? undefined : this.keyMap.get(k)
    if (index === undefined) {
      // addition
      index = this.newIndex()
      this.keyList[index] = k
      this.valList[index] = v
      this.keyMap.set(k, index)
      this.next[this.tail] = index
      this.prev[index] = this.tail
      this.tail = index
      this.size ++
      this.addItemSize(index, v, k, size, sizeCalculation)
    } else {
      // update
      const oldVal = this.valList[index]
      if (v !== oldVal) {
        if (!noDisposeOnSet) {
          this.dispose(oldVal, k, 'set')
          if (this.disposeAfter) {
            this.disposed.push([oldVal, k, 'set'])
          }
        }
        this.removeItemSize(index)
        this.valList[index] = v
        this.addItemSize(index, v, k, size, sizeCalculation)
      }
      this.moveToTail(index)
    }
    if (ttl !== 0 && this.ttl === 0 && !this.ttls) {
      this.initializeTTLTracking()
    }
    this.setItemTTL(index, ttl)
    if (this.disposeAfter) {
      while (this.disposed.length) {
        this.disposeAfter(...this.disposed.shift())
      }
    }
    return this
  }

  newIndex () {
    if (this.size === 0) {
      return this.tail
    }
    if (this.size === this.max) {
      return this.evict()
    }
    if (this.free.length !== 0) {
      return this.free.pop()
    }
    // initial fill, just keep writing down the list
    return this.initialFill++
  }

  pop () {
    if (this.size) {
      const val = this.valList[this.head]
      this.evict()
      return val
    }
  }

  evict () {
    const head = this.head
    const k = this.keyList[head]
    const v = this.valList[head]
    this.dispose(v, k, 'evict')
    if (this.disposeAfter) {
      this.disposed.push([v, k, 'evict'])
    }
    this.removeItemSize(head)
    this.head = this.next[head]
    this.keyMap.delete(k)
    this.size --
    return head
  }

  has (k) {
    return this.keyMap.has(k) && !this.isStale(this.keyMap.get(k))
  }

  // like get(), but without any LRU updating or TTL expiration
  peek (k, { allowStale = this.allowStale } = {}) {
    const index = this.keyMap.get(k)
    if (index !== undefined && (allowStale || !this.isStale(index))) {
      return this.valList[index]
    }
  }

  get (k, {
    allowStale = this.allowStale,
    updateAgeOnGet = this.updateAgeOnGet,
  } = {}) {
    const index = this.keyMap.get(k)
    if (index !== undefined) {
      if (this.isStale(index)) {
        const value = allowStale ? this.valList[index] : undefined
        this.delete(k)
        return value
      } else {
        this.moveToTail(index)
        if (updateAgeOnGet) {
          this.updateItemAge(index)
        }
        return this.valList[index]
      }
    }
  }

  connect (p, n) {
    this.prev[n] = p
    this.next[p] = n
  }

  moveToTail (index) {
    // if tail already, nothing to do
    // if head, move head to next[index]
    // else
    //   move next[prev[index]] to next[index] (head has no prev)
    //   move prev[next[index]] to prev[index]
    // prev[index] = tail
    // next[tail] = index
    // tail = index
    if (index !== this.tail) {
      if (index === this.head) {
        this.head = this.next[index]
      } else {
        this.connect(this.prev[index], this.next[index])
      }
      this.connect(this.tail, index)
      this.tail = index
    }
  }

  get del () {
    deprecatedMethod('del', 'Please use cache.delete() instead.')
    return this.delete
  }
  delete (k) {
    let deleted = false
    if (this.size !== 0) {
      const index = this.keyMap.get(k)
      if (index !== undefined) {
        deleted = true
        if (this.size === 1) {
          this.clear()
        } else {
          this.removeItemSize(index)
          this.dispose(this.valList[index], k, 'delete')
          if (this.disposeAfter) {
            this.disposed.push([this.valList[index], k, 'delete'])
          }
          this.keyMap.delete(k)
          this.keyList[index] = null
          this.valList[index] = null
          if (index === this.tail) {
            this.tail = this.prev[index]
          } else if (index === this.head) {
            this.head = this.next[index]
          } else {
            this.next[this.prev[index]] = this.next[index]
            this.prev[this.next[index]] = this.prev[index]
          }
          this.size --
          this.free.push(index)
        }
      }
    }
    if (this.disposed) {
      while (this.disposed.length) {
        this.disposeAfter(...this.disposed.shift())
      }
    }
    return deleted
  }

  clear () {
    if (this.dispose !== LRUCache.prototype.dispose) {
      for (const index of this.rindexes()) {
        this.dispose(this.valList[index], this.keyList[index], 'delete')
      }
    }
    if (this.disposeAfter) {
      for (const index of this.rindexes()) {
        this.disposed.push([this.valList[index], this.keyList[index], 'delete'])
      }
    }
    this.keyMap.clear()
    this.valList.fill(null)
    this.keyList.fill(null)
    if (this.ttls) {
      this.ttls.fill(0)
      this.starts.fill(0)
    }
    if (this.sizes) {
      this.sizes.fill(0)
    }
    this.head = 0
    this.tail = 0
    this.initialFill = 1
    this.free.length = 0
    this.calculatedSize = 0
    this.size = 0
    if (this.disposed) {
      while (this.disposed.length) {
        this.disposeAfter(...this.disposed.shift())
      }
    }
  }
  get reset () {
    deprecatedMethod('reset', 'Please use cache.clear() instead.')
    return this.clear
  }

  get length () {
    deprecatedProperty('length', 'Please use cache.size instead.')
    return this.size
  }
}

module.exports = LRUCache
