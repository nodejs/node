var events = require('events');
var util = require('util');

function Stream() {
  events.EventEmitter.call(this);
}
util.inherits(Stream, events.EventEmitter);
exports.Stream = Stream;

Stream.prototype.pipe = function(dest /* options, filter */) {
  var source = this;

  // parse arguments
  var options, filter;
  if (typeof arguments[1] == 'object') {
    options = arguments[1];
    filter = arguments[2];
  } else {
    filter = arguments[1];
  }

  function ondata(chunk) {
    // FIXME shouldn't need to test writable - this is working around bug.
    // .writable should not change before a 'end' event is fired.
    if (dest.writable) {
      if (false === dest.write(chunk)) source.pause();
    }
  }

  if (!filter) {
    source.on('data', ondata);
  } else {
    //
    // TODO: needs tests
    //
    var wait = false;
    var waitQueue = [];

    function done () {
      wait = false;
      // Drain the waitQueue
      if (dest.writable && waitQueue.length) {
        wait = true;
        filter(waitQueue.shift(), ondata, done);
      }
    }

    source.on('data', function (d) {
      if (wait) {
        waitQueue.push(d);
        source.pause();
      } else {
        wait = true;
        filter(d, ondata, done);
      }
    });
  }


  function ondrain() {
    if (source.readable) source.resume();
  }

  dest.on('drain', ondrain);

  /*
   * If the 'end' option is not supplied, dest.end() will be called when
   * source gets the 'end' event.
   */

  if (!options || options.end !== false) {
    function onend() {
      dest.end();
    }

    source.on('end', onend);
  }

  dest.on('close', function() {
    source.removeListener('data', ondata);
    dest.removeListener('drain', ondrain);
    source.removeListener('end', onend);
  });


  /*
   * Questionable:
   */

  if (!source.pause) {
    source.pause = function() {
      source.emit('pause');
    };
  }

  if (!source.resume) {
    source.resume = function() {
      source.emit('resume');
    };
  }

  dest.on('pause', function() {
    source.pause();
  });

  dest.on('resume', function() {
    if (source.readable) source.resume();
  });
};
