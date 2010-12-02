var Timer = process.binding('timer').Timer;
var assert = process.assert;

/*
 * To enable debug statements for the timers do NODE_DEBUG=8 ./node script.js
 */
var debugLevel = parseInt(process.env.NODE_DEBUG, 16);
var debug;
if (debugLevel & 0x8) {
  debug = function() { require('util').error.apply(this, arguments); };
} else {
  debug = function() { };
}


// IDLE TIMEOUTS
//
// Because often many sockets will have the same idle timeout we will not
// use one timeout watcher per item. It is too much overhead.  Instead
// we'll use a single watcher for all sockets with the same timeout value
// and a linked list. This technique is described in the libev manual:
// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#Be_smart_about_timeouts

// Object containing all lists, timers
// key = time in milliseconds
// value = list
var lists = {};

// show the most idle item
function peek(list) {
  if (list._idlePrev == list) return null;
  return list._idlePrev;
}


// remove the most idle item from the list
function shift(list) {
  var first = list._idlePrev;
  remove(first);
  return first;
}


// remove a item from its list
function remove(item) {
  item._idleNext._idlePrev = item._idlePrev;
  item._idlePrev._idleNext = item._idleNext;
}


// remove a item from its list and place at the end.
function append(list, item) {
  item._idleNext = list._idleNext;
  list._idleNext._idlePrev = item;
  item._idlePrev = list;
  list._idleNext = item;
}


// the main function - creates lists on demand and the watchers associated
// with them.
function insert(item, msecs) {
  item._idleStart = new Date();
  item._idleTimeout = msecs;

  if (msecs < 0) return;

  var list;

  if (lists[msecs]) {
    list = lists[msecs];
  } else {
    list = new Timer();
    list._idleNext = list;
    list._idlePrev = list;

    lists[msecs] = list;

    list.callback = function() {
      debug('timeout callback ' + msecs);
      // TODO - don't stop and start the watcher all the time.
      // just set its repeat
      var now = new Date();
      debug('now: ' + now);
      var first;
      while (first = peek(list)) {
        var diff = now - first._idleStart;
        if (diff < msecs) {
          list.again(msecs - diff);
          debug(msecs + ' list wait because diff is ' + diff);
          return;
        } else {
          remove(first);
          assert(first !== peek(list));
          if (first._onTimeout) first._onTimeout();
        }
      }
      debug(msecs + ' list empty');
      assert(list._idleNext === list); // list is empty
      list.stop();
    };
  }

  if (list._idleNext === list) {
    // if empty (re)start the timer
    list.again(msecs);
  }

  append(list, item);
  assert(list._idleNext !== list); // list is not empty
}


var unenroll = exports.unenroll = function(item) {
  if (item._idleNext) {
    remove(item);

    var list = lists[item._idleTimeout];
    // if empty then stop the watcher
    //debug('unenroll');
    if (list && list._idlePrev == list) {
      //debug('unenroll: list empty');
      list.stop();
    }
  }
};


// Does not start the time, just sets up the members needed.
exports.enroll = function(item, msecs) {
  // if this item was already in a list somewhere
  // then we should unenroll it from that
  if (item._idleNext) unenroll(item);

  item._idleTimeout = msecs;
  item._idleNext = item;
  item._idlePrev = item;
};

// call this whenever the item is active (not idle)
// it will reset its timeout.
exports.active = function(item) {
  var msecs = item._idleTimeout;
  if (msecs >= 0) {
    var list = lists[msecs];
    if (item._idleNext == item) {
      insert(item, msecs);
    } else {
      // inline append
      item._idleStart = new Date();
      item._idleNext._idlePrev = item._idlePrev;
      item._idlePrev._idleNext = item._idleNext;
      item._idleNext = list._idleNext;
      item._idleNext._idlePrev = item;
      item._idlePrev = list;
      list._idleNext = item;
    }
  }
};


/*
 * DOM-style timers
 */


exports.setTimeout = function(callback, after) {
  var timer;

  if (after <= 0) {
    // Use the slow case for after == 0
    timer = new Timer();
    timer.callback = callback;
  } else {
    timer = { _idleTimeout: after, _onTimeout: callback };
    timer._idlePrev = timer;
    timer._idleNext = timer;
  }

  /*
   * Sometimes setTimeout is called with arguments, EG
   *
   *   setTimeout(callback, 2000, "hello", "world")
   *
   * If that's the case we need to call the callback with
   * those args. The overhead of an extra closure is not
   * desired in the normal case.
   */
  if (arguments.length > 2) {
    var args = Array.prototype.slice.call(arguments, 2);
    var c = function() {
      callback.apply(timer, args);
    };

    if (timer instanceof Timer) {
      timer.callback = c;
    } else {
      timer._onTimeout = c;
    }
  }

  if (timer instanceof Timer) {
    timer.start(0, 0);
  } else {
    exports.active(timer);
  }

  return timer;
};


exports.clearTimeout = function(timer) {
  if (timer && (timer.callback || timer._onTimeout)) {
    timer.callback = timer._onTimeout = null;
    exports.unenroll(timer);
    if (timer instanceof Timer) timer.stop(); // for after === 0
  }
};


exports.setInterval = function(callback, repeat) {
  var timer = new Timer();

  if (arguments.length > 2) {
    var args = Array.prototype.slice.call(arguments, 2);
    timer.callback = function() {
      callback.apply(timer, args);
    };
  } else {
    timer.callback = callback;
  }

  timer.start(repeat, repeat ? repeat : 1);
  return timer;
};


exports.clearInterval = function(timer) {
  if (timer instanceof Timer) {
    timer.callback = null;
    timer.stop();
  }
};
