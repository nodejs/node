#!/usr/bin/env node
/*
 * Stream out JSON objects.
 *
 * Usage:
 *  docstream.js [TIMES] [PERIOD] [RANDOM-CHUNKS]
 *
 * where:
 *    TIMES is an integer number of times to emit the JSON object. Default 10.
 *    PERIOD is the number of ms between emitted chunks. Default 1000.
 *    RANDOM-CHUNKS, if present, will result in non-complete JSON objects
 *        being emitted. Default *not* randomized.
 */


function randint(min, max) {
  if (max === undefined) {
    max = min;
    min = 0;
  }
  return (Math.floor(Math.random() * (max - min + 1)) + min);
}


//---- mainline

var times = Number(process.argv[2]);
if (isNaN(times))
  times = 5;
var period = Number(process.argv[3]);
if (isNaN(period))
  period = 1000;
// i.e. don't emit whole JSON objects.
var randomize = (process.argv[4] !== undefined);

var leftover = '';
var interval = setInterval(function () {
  times--;
  var s = leftover + JSON.stringify({foo: 'bar'}) + '\n';
  var len = s.length; // emit the whole thing
  if (times <= 0) {
    clearInterval(interval);
  } else if (randomize) {
    len = randint(0, s.length);
  }
  //console.warn("XXX docstream.js [%d]: emit %d chars: '%s'", times, len,
  //  s.slice(0, len));
  process.stdout.write(s.slice(0, len));
  leftover = s.slice(len);
}, period);
