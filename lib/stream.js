var events = require('events');
var util = require('util');

function Stream() {
  events.EventEmitter.call(this);
}
util.inherits(Stream, events.EventEmitter);
exports.Stream = Stream;

Stream.prototype.pipe = function(dest, options) {
  var source = this;

  function ondata(chunk) {
    if (dest.writable) {
      if (false === dest.write(chunk)) source.pause();
    }
  }

  source.on('data', ondata);

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
