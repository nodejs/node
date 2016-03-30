module.exports = LRUCache

// This will be a proper iterable 'Map' in engines that support it,
// or a fakey-fake PseudoMap in older versions.
var Map = require('pseudomap')

function naiveLength () { return 1 }

function LRUCache (options) {
  if (!(this instanceof LRUCache))
    return new LRUCache(options)

  if (typeof options === 'number')
    options = { max: options }

  if (!options)
    options = {}

  this._max = options.max
  // Kind of weird to have a default max of Infinity, but oh well.
  if (!this._max || !(typeof this._max === "number") || this._max <= 0 )
    this._max = Infinity

  this._lengthCalculator = options.length || naiveLength
  if (typeof this._lengthCalculator !== "function")
    this._lengthCalculator = naiveLength

  this._allowStale = options.stale || false
  this._maxAge = options.maxAge || null
  this._dispose = options.dispose
  this.reset()
}

// resize the cache when the max changes.
Object.defineProperty(LRUCache.prototype, "max",
  { set : function (mL) {
      if (!mL || !(typeof mL === "number") || mL <= 0 ) mL = Infinity
      this._max = mL
      if (this._length > this._max) trim(this)
    }
  , get : function () { return this._max }
  , enumerable : true
  })

// resize the cache when the lengthCalculator changes.
Object.defineProperty(LRUCache.prototype, "lengthCalculator",
  { set : function (lC) {
      if (typeof lC !== "function") {
        this._lengthCalculator = naiveLength
        this._length = this._lruList.size
        this._cache.forEach(function (value, key) {
          value.length = 1
        })
      } else {
        this._lengthCalculator = lC
        this._length = 0
        this._cache.forEach(function (value, key) {
          value.length = this._lengthCalculator(value.value, key)
          this._length += value.length
        }, this)
      }

      if (this._length > this._max) trim(this)
    }
  , get : function () { return this._lengthCalculator }
  , enumerable : true
  })

Object.defineProperty(LRUCache.prototype, "length",
  { get : function () { return this._length }
  , enumerable : true
  })

Object.defineProperty(LRUCache.prototype, "itemCount",
  { get : function () { return this._lruList.size }
  , enumerable : true
  })

function reverseKeys (map) {
  // keys live in lruList map in insertion order.
  // we want them in reverse insertion order.
  // flip the list of keys.
  var itemCount = map.size
  var keys = new Array(itemCount)
  var i = itemCount
  map.forEach(function (value, key) {
    keys[--i] = key
  })

  return keys
}

LRUCache.prototype.rforEach = function (fn, thisp) {
  thisp = thisp || this
  this._lruList.forEach(function (hit) {
    forEachStep(this, fn, hit, thisp)
  }, this)
}

function forEachStep (self, fn, hit, thisp) {
  if (isStale(self, hit)) {
    del(self, hit)
    if (!self._allowStale) {
      hit = undefined
    }
  }
  if (hit) {
    fn.call(thisp, hit.value, hit.key, self)
  }
}


LRUCache.prototype.forEach = function (fn, thisp) {
  thisp = thisp || this

  var keys = reverseKeys(this._lruList)
  for (var k = 0; k < keys.length; k++) {
    var hit = this._lruList.get(keys[k])
    forEachStep(this, fn, hit, thisp)
  }
}

LRUCache.prototype.keys = function () {
  return reverseKeys(this._lruList).map(function (k) {
    return this._lruList.get(k).key
  }, this)
}

LRUCache.prototype.values = function () {
  return reverseKeys(this._lruList).map(function (k) {
    return this._lruList.get(k).value
  }, this)
}

LRUCache.prototype.reset = function () {
  if (this._dispose && this._cache) {
    this._cache.forEach(function (entry, key) {
      this._dispose(key, entry.value)
    }, this)
  }

  this._cache = new Map() // hash of items by key
  this._lruList = new Map() // list of items in order of use recency
  this._mru = 0 // most recently used
  this._lru = 0 // least recently used
  this._length = 0 // number of items in the list
}

LRUCache.prototype.dump = function () {
  var arr = []
  var i = 0
  var size = this._lruList.size
  return reverseKeys(this._lruList).map(function (k) {
    var hit = this._lruList.get(k)
    if (!isStale(this, hit)) {
      return {
        k: hit.key,
        v: hit.value,
        e: hit.now + (hit.maxAge || 0)
      }
    }
  }, this).filter(function (h) {
    return h
  })
}

LRUCache.prototype.dumpLru = function () {
  return this._lruList
}

LRUCache.prototype.set = function (key, value, maxAge) {
  maxAge = maxAge || this._maxAge

  var now = maxAge ? Date.now() : 0
  var len = this._lengthCalculator(value, key)

  if (this._cache.has(key)) {
    if (len > this._max) {
      del(this, this._cache.get(key))
      return false
    }

    var item = this._cache.get(key)

    // dispose of the old one before overwriting
    if (this._dispose)
      this._dispose(key, item.value)

    item.now = now
    item.maxAge = maxAge
    item.value = value
    this._length += (len - item.length)
    item.length = len
    this.get(key)

    if (this._length > this._max)
      trim(this)

    return true
  }

  var hit = new Entry(key, value, this._mru, len, now, maxAge)
  incMru(this)

  // oversized objects fall out of cache automatically.
  if (hit.length > this._max) {
    if (this._dispose) this._dispose(key, value)
    return false
  }

  this._length += hit.length
  this._cache.set(key, hit)
  this._lruList.set(hit.lu, hit)

  if (this._length > this._max)
    trim(this)

  return true
}

LRUCache.prototype.has = function (key) {
  if (!this._cache.has(key)) return false
  var hit = this._cache.get(key)
  if (isStale(this, hit)) {
    return false
  }
  return true
}

LRUCache.prototype.get = function (key) {
  return get(this, key, true)
}

LRUCache.prototype.peek = function (key) {
  return get(this, key, false)
}

LRUCache.prototype.pop = function () {
  var hit = this._lruList.get(this._lru)
  del(this, hit)
  return hit || null
}

LRUCache.prototype.del = function (key) {
  del(this, this._cache.get(key))
}

LRUCache.prototype.load = function (arr) {
  //reset the cache
  this.reset();

  var now = Date.now()
  // A previous serialized cache has the most recent items first
  for (var l = arr.length - 1; l >= 0; l--) {
    var hit = arr[l]
    var expiresAt = hit.e || 0
    if (expiresAt === 0) {
      // the item was created without expiration in a non aged cache
      this.set(hit.k, hit.v)
    } else {
      var maxAge = expiresAt - now
      // dont add already expired items
      if (maxAge > 0) {
        this.set(hit.k, hit.v, maxAge)
      }
    }
  }
}

function get (self, key, doUse) {
  var hit = self._cache.get(key)
  if (hit) {
    if (isStale(self, hit)) {
      del(self, hit)
      if (!self._allowStale) hit = undefined
    } else {
      if (doUse) use(self, hit)
    }
    if (hit) hit = hit.value
  }
  return hit
}

function isStale(self, hit) {
  if (!hit || (!hit.maxAge && !self._maxAge)) return false
  var stale = false;
  var diff = Date.now() - hit.now
  if (hit.maxAge) {
    stale = diff > hit.maxAge
  } else {
    stale = self._maxAge && (diff > self._maxAge)
  }
  return stale;
}

function use (self, hit) {
  shiftLU(self, hit)
  hit.lu = self._mru
  incMru(self)
  self._lruList.set(hit.lu, hit)
}

function trim (self) {
  if (self._length > self._max) {
    var keys = reverseKeys(self._lruList)
    for (var k = keys.length - 1; self._length > self._max; k--) {
      // We know that we're about to delete this one, and also
      // what the next least recently used key will be, so just
      // go ahead and set it now.
      self._lru = keys[k - 1]
      del(self, self._lruList.get(keys[k]))
    }
  }
}

function shiftLU (self, hit) {
  self._lruList.delete(hit.lu)
  if (hit.lu === self._lru)
    self._lru = reverseKeys(self._lruList).pop()
}

function del (self, hit) {
  if (hit) {
    if (self._dispose) self._dispose(hit.key, hit.value)
    self._length -= hit.length
    self._cache.delete(hit.key)
    shiftLU(self, hit)
  }
}

// classy, since V8 prefers predictable objects.
function Entry (key, value, lu, length, now, maxAge) {
  this.key = key
  this.value = value
  this.lu = lu
  this.length = length
  this.now = now
  if (maxAge) this.maxAge = maxAge
}


// Incrementers and decrementers that loop at MAX_SAFE_INTEGER
// only relevant for the lu, lru, and mru counters, since they
// get touched a lot and can get very large. Also, since they
// only go upwards, and the sets will tend to be much smaller than
// the max, we can very well assume that a very small number comes
// after a very large number, rather than before it.
var maxSafeInt = Number.MAX_SAFE_INTEGER || 9007199254740991
function intInc (number) {
  return (number === maxSafeInt) ? 0 : number + 1
}
function incMru (self) {
  do {
    self._mru = intInc(self._mru)
  } while (self._lruList.has(self._mru))
}
