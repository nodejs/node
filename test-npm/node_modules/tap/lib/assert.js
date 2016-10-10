var synonyms = require('./synonyms.js')
var deeper = require('deeper') // strict
var shallower = require('only-shallow') // in touch with its feelings
var tmatch = require('tmatch') // ok with partial estimates

// Load Buffer the old way for browserify's sake
var Buffer = require('buffer').Buffer // eslint-disable-line

// this is actually the "working half" of the Test class.
// each method figures out if it's a pass or fail, and decorates
// the extra bit, and then calls either pass() or fail() or some
// other assert method.
//
// typically, a plugin would do this on a specific instance, eg on
// the root test harness instance.  but we do this here to add some
// useful prototype methods.

exports.decorate = decorate

function decorate (t) {
  t.addAssert('ok', 1, function (obj, message, extra) {
    message = message || 'expect truthy value'
    if (obj) {
      return this.pass(message, extra)
    }

    return this.fail(message, extra)
  })

  t.addAssert('notOk', 1, function (obj, message, extra) {
    message = message || 'expect falsey value'
    return this.ok(!obj, message, extra)
  })

  t.addAssert('error', 1, function (er, message, extra) {
    if (!er) {
      return this.pass(message || 'should not error', extra)
    }

    if (!(er instanceof Error)) {
      extra.found = er
      return this.fail(message || 'non-Error error encountered', extra)
    }

    message = message || er.message
    extra.found = er
    return this.fail(message, extra)
  })

  t.addAssert('equal', 2, function (f, w, m, e) {
    m = m || 'should be equal'
    if (f === w) {
      return this.pass(m, e)
    }

    e.found = f
    e.wanted = w
    e.compare = '==='

    if (typeof f === 'object' &&
        typeof w === 'object' &&
        f &&
        w &&
        shallower(f, w)) {
      e.note = 'Objects never === one another'
    }

    return this.fail(m, e)
  })

  t.addAssert('not', 2, function (f, w, m, e) {
    m = m || 'should not be equal'
    if (f !== w) {
      return this.pass(m, e)
    }

    e.found = f
    e.doNotWant = w
    e.compare = '!=='

    return this.fail(m, e)
  })

  t.addAssert('same', 2, function (f, w, m, e) {
    m = m || 'should be equivalent'
    e.found = f
    e.wanted = w
    return this.ok(shallower(f, w), m, e)
  })

  t.addAssert('notSame', 2, function (f, w, m, e) {
    m = m || 'should not be equivalent'
    e.found = f
    e.doNotWant = w
    return this.notOk(shallower(f, w), m, e)
  })

  t.addAssert('strictSame', 2, function (f, w, m, e) {
    m = m || 'should be equivalent strictly'
    e.found = f
    e.wanted = w
    return this.ok(deeper(f, w), m, e)
  })

  t.addAssert('strictNotSame', 2, function (f, w, m, e) {
    m = m || 'should be equivalent strictly'
    e.found = f
    e.doNotWant = w
    return this.notOk(deeper(f, w), m, e)
  })

  t.addAssert('match', 2, function (f, w, m, e) {
    m = m || 'should match pattern provided'
    e.found = f
    e.pattern = w
    return this.ok(tmatch(f, w), m, e)
  })

  t.addAssert('notMatch', 2, function (f, w, m, e) {
    m = m || 'should not match pattern provided'
    e.found = f
    e.pattern = w
    return this.ok(!tmatch(f, w), m, e)
  })

  t.addAssert('type', 2, function (obj, klass, m, e) {
    var name = klass
    if (typeof name === 'function') {
      name = name.name || '(anonymous constructor)'
    }
    m = m || 'type is ' + name

    // simplest case, it literally is the same thing
    if (obj === klass) {
      return this.pass(m, e)
    }

    var type = typeof obj
    if (!obj && type === 'object') {
      type = 'null'
    }

    if (type === 'object' && klass !== 'object') {
      if (typeof klass === 'function') {
        e.found = Object.getPrototypeOf(obj).constructor.name
        e.wanted = name
        return this.ok(obj instanceof klass, m, e)
      }

      // check prototype chain for name
      // at this point, we already know klass is not a function
      // if the klass specified is an obj in the proto chain, pass
      // if the name specified is the name of a ctor in the chain, pass
      var p = obj
      do {
        var ctor = p.constructor && p.constructor.name
        if (p === klass || ctor === name) {
          return this.pass(m, e)
        }
        p = Object.getPrototypeOf(p)
      } while (p)
    }

    return this.equal(type, name, m, e)
  })

  t.addAssert('throws', 4, function (fn_, wanted_, m_, e_, m, e__) {
    var fn, wanted, e
    for (var i = 0; i < arguments.length - 1; i++) {
      var arg = arguments[i]
      if (typeof arg === 'function') {
        if (arg === Error || arg.prototype instanceof Error) {
          wanted = arg
        } else if (!fn) {
          fn = arg
        }
      } else if (typeof arg === 'string' && arg) {
        m = arg
      } else if (typeof arg === 'object') {
        if (!wanted) {
          wanted = arg
        } else {
          e = arg
        }
      }
    }

    // Copy local properties of the 'extra' object, like 'skip' etc
    Object.keys(e__).forEach(function (i) {
      e[i] = e__[i]
    })

    if (!m) {
      m = fn && fn.name || 'expected to throw'
    }

    if (wanted) {
      if (wanted instanceof Error) {
        var w = {
          message: wanted.message
        }
        if (wanted.name) {
          w.name = wanted.name
        }

        // intentionally copying non-local properties, since this
        // is an Error object, and those are funky.
        for (i in wanted) {
          w[i] = wanted[i]
        }
        wanted = w

        m += ': ' + (wanted.name || 'Error') + ' ' + wanted.message
        e = e || {}
        if (e !== wanted) {
          e.wanted = wanted
        }
      }
    }

    if (typeof fn !== 'function') {
      e = e || {}
      e.todo = true
      return this.pass(m, e)
    }

    try {
      fn()
      return this.fail(m, e)
    } catch (er) {
      // 'name' is a getter.
      if (er.name) {
        er.name = er.name + ''
      }

      if (wanted) {
        if (Object.prototype.toString.call(wanted) === '[object RegExp]') {
          return this.match(er.message, wanted, m, e)
        }
        return this.has(er, wanted, m, e)
      } else {
        return this.pass(m, e)
      }
    }
  })

  t.addAssert('doesNotThrow', 1, function (fn, m, e) {
    if (typeof fn === 'string') {
      var x = fn
      fn = m
      m = x
    }

    if (!m) {
      m = fn && fn.name || 'expected to not throw'
    }

    if (typeof fn !== 'function') {
      e.todo = true
      return this.pass(m, e)
    }

    try {
      fn()
      return this.pass(m, e)
    } catch (er) {
      return this.fail(m, this._extraFromError(er, e))
    }
  })

  // synonyms are helpful.
  Object.keys(synonyms).forEach(function (c) {
    if (t[c]) {
      synonyms[c].forEach(function (s) {
        Object.defineProperty(t, s, {
          value: t[c],
          enumerable: false,
          configurable: true,
          writable: true
        })
      })
    }
  })
}
