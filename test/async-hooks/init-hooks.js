'use strict';
// Flags: --expose-gc

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const util = require('util');
const print = process._rawDebug;

if (typeof global.gc === 'function') {
  (function exity(cntr) {
    process.once('beforeExit', () => {
      global.gc();
      if (cntr < 4) setImmediate(() => exity(cntr + 1));
    });
  })(0);
}

function noop() {}

class ActivityCollector {
  constructor(start, {
    allowNoInit = false,
    oninit,
    onbefore,
    onafter,
    ondestroy,
    logid = null,
    logtype = null
  } = {}) {
    this._start = start;
    this._allowNoInit = allowNoInit;
    this._activities = new Map();
    this._logid = logid;
    this._logtype = logtype;

    // register event handlers if provided
    this.oninit = typeof oninit === 'function' ? oninit : noop;
    this.onbefore = typeof onbefore === 'function' ? onbefore : noop;
    this.onafter = typeof onafter === 'function' ? onafter : noop;
    this.ondestroy = typeof ondestroy === 'function' ? ondestroy : noop;

    // create the hook with which we'll collect activity data
    this._asyncHook = async_hooks.createHook({
      init: this._init.bind(this),
      before: this._before.bind(this),
      after: this._after.bind(this),
      destroy: this._destroy.bind(this)
    });
  }

  enable() {
    this._asyncHook.enable();
  }

  disable() {
    this._asyncHook.disable();
  }

  sanityCheck(types) {
    if (types != null && !Array.isArray(types)) types = [ types ];

    function activityString(a) {
      return util.inspect(a, false, 5, true);
    }

    const violations = [];
    function v(msg) { violations.push(msg); }
    for (const a of this._activities.values()) {
      if (types != null && types.indexOf(a.type) < 0) continue;

      if (a.init && a.init.length > 1) {
        v('Activity inited twice\n' + activityString(a) +
          '\nExpected "init" to be called at most once');
      }
      if (a.destroy && a.destroy.length > 1) {
        v('Activity destroyed twice\n' + activityString(a) +
          '\nExpected "destroy" to be called at most once');
      }
      if (a.before && a.after) {
        if (a.before.length < a.after.length) {
          v('Activity called "after" without calling "before"\n' +
            activityString(a) +
            '\nExpected no "after" call without a "before"');
        }
        if (a.before.some((x, idx) => x > a.after[idx])) {
          v('Activity had an instance where "after" ' +
            'was invoked before "before"\n' +
            activityString(a) +
            '\nExpected "after" to be called after "before"');
        }
      }
      if (a.before && a.destroy) {
        if (a.before.some((x, idx) => x > a.destroy[idx])) {
          v('Activity had an instance where "destroy" ' +
            'was invoked before "before"\n' +
            activityString(a) +
            '\nExpected "destroy" to be called after "before"');
        }
      }
      if (a.after && a.destroy) {
        if (a.after.some((x, idx) => x > a.destroy[idx])) {
          v('Activity had an instance where "destroy" ' +
            'was invoked before "after"\n' +
            activityString(a) +
            '\nExpected "destroy" to be called after "after"');
        }
      }
    }
    if (violations.length) {
      console.error(violations.join('\n'));
      assert.fail(violations.length, 0, `Failed sanity checks: ${violations}`);
    }
  }

  inspect(opts = {}) {
    if (typeof opts === 'string') opts = { types: opts };
    const { types = null, depth = 5, stage = null } = opts;
    const activities = types == null ?
      Array.from(this._activities.values()) :
      this.activitiesOfTypes(types);

    if (stage != null) console.log('\n%s', stage);
    console.log(util.inspect(activities, false, depth, true));
  }

  activitiesOfTypes(types) {
    if (!Array.isArray(types)) types = [ types ];
    return this.activities.filter((x) => types.indexOf(x.type) >= 0);
  }

  get activities() {
    return Array.from(this._activities.values());
  }

  _stamp(h, hook) {
    if (h == null) return;
    if (h[hook] == null) h[hook] = [];
    const time = process.hrtime(this._start);
    h[hook].push((time[0] * 1e9) + time[1]);
  }

  _getActivity(uid, hook) {
    const h = this._activities.get(uid);
    if (!h) {
      // if we allowed handles without init we ignore any further life time
      // events this makes sense for a few tests in which we enable some hooks
      // later
      if (this._allowNoInit) {
        const stub = { uid, type: 'Unknown' };
        this._activities.set(uid, stub);
        return stub;
      } else {
        const err = new Error(`Found a handle whose ${hook}` +
                              ' hook was invoked but not its init hook');
        // Don't throw if we see invocations due to an assertion in a test
        // failing since we want to list the assertion failure instead
        if (/process\._fatalException/.test(err.stack)) return null;
        throw err;
      }
    }
    return h;
  }

  _init(uid, type, triggerId, handle) {
    const activity = { uid, type, triggerId };
    this._stamp(activity, 'init');
    this._activities.set(uid, activity);
    this._maybeLog(uid, type, 'init');
    this.oninit(uid, type, triggerId, handle);
  }

  _before(uid) {
    const h = this._getActivity(uid, 'before');
    this._stamp(h, 'before');
    this._maybeLog(uid, h && h.type, 'before');
    this.onbefore(uid);
  }

  _after(uid) {
    const h = this._getActivity(uid, 'after');
    this._stamp(h, 'after');
    this._maybeLog(uid, h && h.type, 'after');
    this.onafter(uid);
  }

  _destroy(uid) {
    const h = this._getActivity(uid, 'destroy');
    this._stamp(h, 'destroy');
    this._maybeLog(uid, h && h.type, 'destroy');
    this.ondestroy(uid);
  }

  _maybeLog(uid, type, name) {
    if (this._logid &&
      (type == null || this._logtype == null || this._logtype === type)) {
      print(this._logid + '.' + name + '.uid-' + uid);
    }
  }
}

exports = module.exports = function initHooks({
    oninit,
    onbefore,
    onafter,
    ondestroy,
    allowNoInit,
    logid,
    logtype } = {}) {
  return new ActivityCollector(process.hrtime(), {
    oninit,
    onbefore,
    onafter,
    ondestroy,
    allowNoInit,
    logid,
    logtype
  });
};
