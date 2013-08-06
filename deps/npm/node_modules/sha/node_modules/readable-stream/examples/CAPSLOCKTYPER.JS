var Transform = require('../transform');
var inherits = require('util').inherits;

// subclass
function MyStream () {
  Transform.call(this, {
    lowWaterMark: 0,
    encoding: 'utf8'
  });
}
inherits(MyStream, Transform);

MyStream.prototype._transform = function (chunk, outputFn, callback) {
  outputFn(new Buffer(String(chunk).toUpperCase()));
  callback();
};

// use it!
var s = new MyStream();
process.stdin.resume();
process.stdin.pipe(s).pipe(process.stdout);
if (process.stdin.setRawMode)
  process.stdin.setRawMode(true);
process.stdin.on('data', function (c) {
  c = c.toString();
  if (c === '\u0003' || c === '\u0004') {
    process.stdin.pause();
    s.end();
  }
  if (c === '\r')
    process.stdout.write('\n');
});
