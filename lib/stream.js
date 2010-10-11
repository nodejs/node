var events = require('events');
var util = require('util');

function Stream () {
  events.EventEmitter.call(this);
}
util.inherits(Stream, events.EventEmitter);
exports.Stream = Stream;

Stream.prototype.pipe = function (dest, options) {
  var source = this;

  source.on("data", function (chunk) {
    if (false === dest.write(chunk)) source.pause();
  });

  dest.on("drain", function () {
    if (source.readable) source.resume();
  });

  /*
   * If the 'end' option is not supplied, dest.end() will be called when
   * source gets the 'end' event.
   */

  if (!options || options.end !== false) {
    source.on("end", function () {
      dest.end();
    });
  }

  /*
   * Questionable:
   */

  if (!source.pause) {
    source.pause = function () {
      source.emit("pause");
    };
  }

  if (!source.resume) {
    source.resume = function () {
      source.emit("resume");
    };
  }

  dest.on("pause", function () {
    source.pause();
  });

  dest.on("resume", function () {
    if (source.readable) source.resume();
  });
};
