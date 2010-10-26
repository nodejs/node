var Timer = process.binding('timer').Timer;
var assert = process.assert;

/* 
 * To enable debug statements for the timers do NODE_DEBUG=8 ./node script.js
 */
var debugLevel = parseInt(process.env.NODE_DEBUG, 16);
function debug () {
  if (debugLevel & 0x8) {
    require('util').error.apply(this, arguments);
  }
}

// IDLE TIMEOUTS
//
// Because often many sockets will have the same idle timeout we will not
// use one timeout watcher per socket. It is too much overhead.  Instead
// we'll use a single watcher for all sockets with the same timeout value
// and a linked list. This technique is described in the libev manual:
// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#Be_smart_about_timeouts

// Object containing all lists, timers
// key = time in milliseconds
// value = list
var lists = {};

// show the most idle socket
function peek (list) {
  if (list._idlePrev == list) return null;
  return list._idlePrev;
}


// remove the most idle socket from the list
function shift (list) {
  var first = list._idlePrev;
  remove(first);
  return first;
}


// remove a socket from its list
function remove (socket) {
  socket._idleNext._idlePrev = socket._idlePrev;
  socket._idlePrev._idleNext = socket._idleNext;
}


// remove a socket from its list and place at the end.
function append (list, socket) {
  remove(socket);
  socket._idleNext = list._idleNext;
  socket._idleNext._idlePrev = socket;
  socket._idlePrev = list;
  list._idleNext = socket;
}


// the main function - creates lists on demand and the watchers associated
// with them.
function insert (socket, msecs) {
  socket._idleStart = new Date();
  socket._idleTimeout = msecs;

  if (!msecs) return;

  var list;

  if (lists[msecs]) {
    list = lists[msecs];
  } else {
    list = new Timer();
    list._idleNext = list;
    list._idlePrev = list;

    lists[msecs] = list;

    list.callback = function () {
      debug('timeout callback ' + msecs);
      // TODO - don't stop and start the watcher all the time.
      // just set its repeat
      var now = new Date();
      debug("now: " + now);
      var first;
      while (first = peek(list)) {
        var diff = now - first._idleStart;
        if (diff < msecs) {
          list.again(msecs - diff);
          debug(msecs + ' list wait because diff is '  + diff);
          return;
        } else {
          remove(first);
          assert(first != peek(list));
          if (first._onTimeout) first._onTimeout();
        }
      }
      debug(msecs + ' list empty');
      assert(list._idleNext == list); // list is empty
      list.stop();
    };
  }

  if (list._idleNext == list) {
    // if empty (re)start the timer
    list.again(msecs);
  }

  append(list, socket);
  assert(list._idleNext != list); // list is not empty
}


var unenroll = exports.unenroll = function (socket) {
  if (socket._idleNext) {
    socket._idleNext._idlePrev = socket._idlePrev;
    socket._idlePrev._idleNext = socket._idleNext;

    var list = lists[socket._idleTimeout];
    // if empty then stop the watcher
    //debug('unenroll');
    if (list && list._idlePrev == list) {
      //debug('unenroll: list empty');
      list.stop();
    }
  }
};


// Does not start the time, just sets up the members needed.
exports.enroll = function (socket, msecs) {
  // if this socket was already in a list somewhere
  // then we should unenroll it from that
  if (socket._idleNext) unenroll(socket);

  socket._idleTimeout = msecs;
  socket._idleNext = socket;
  socket._idlePrev = socket;
};

// call this whenever the socket is active (not idle)
// it will reset its timeout.
exports.active = function (socket) {
  var msecs = socket._idleTimeout;
  if (msecs) {
    var list = lists[msecs];
    if (socket._idleNext == socket) {
      insert(socket, msecs);
    } else {
      // inline append
      socket._idleStart = new Date();
      socket._idleNext._idlePrev = socket._idlePrev;
      socket._idlePrev._idleNext = socket._idleNext;
      socket._idleNext = list._idleNext;
      socket._idleNext._idlePrev = socket;
      socket._idlePrev = list;
      list._idleNext = socket;
    }
  }
};
