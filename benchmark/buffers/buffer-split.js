var common = require('../common.js');
var crypto = require('crypto');
var assert = require('assert');

const big = crypto.randomBytes(100000000 / 2).toString('hex') // 100mb
const bigAssert = 100*1000*1000
const small = `4320abc78a25ff6d9afbcb51924dcb4eef8ac424a0d7451b6e5001528c8a46` +
              `5decff7d8e82e26176769b4266fdc05e4773bf` // 100byte
const smallAssert = `4320abc78a25ff6d9afbCB51924dCB4eef8ac424a0d7451b6e5001528c8a46` +
                    `5decff7d8e82e26176769b4266fdc05e4773bf` // 100byte

var bench = common.createBenchmark(main, {
  size: ['small', 'big'],
  assert: ['true', 'false'],
  type: ['cpp', 'string'],
  n: [1024]
});

var smallBuf = new Buffer(small)
var bigBuf = new Buffer(big)
var findStr = 'cb'
var find = new Buffer(findStr)
var replaceStr = 'CB'
var replace = new Buffer(replaceStr)
var splitAndJoin = null

Buffer.prototype.replace = function replace(sub, newSub, encoding) {
  // input must be buffer; else force convert it to one
  // !(newSub instanceof Buffer) ? newSub = new Buffer(newSub) : 0
  let indexes = [], i = -1; // target and index for occurences
  let cursorOld = 0, cursorNew = 0, diff = 0 // cursors for replacing
  // get all occurences of sub
  while ((i = this.indexOf(sub, i + 1)) != -1) {
    indexes.push(i);
  }
  // resulting buffer; shortly unallocated; destroy in error case
  let res = Buffer.allocUnsafe(this.length -
                               indexes.length * sub.length +
                               indexes.length * newSub.length)

  indexes.forEach((el, index, array) => {
    // copy everthing until first occurence into new buffer. As consquence
    // advance cursors (of the buffer reads) either according to the distances
    // plus size of the insert
    this.copy(res, cursorNew, cursorOld, el)
    // calculate diff for the next jump mark of th cursor.
    // First change requires a diff that is for both the same. This is different
    // for first call where diffing begins at 0. Else note: diff between to
    // occurences must be bumped up by 1
    diff = el - (index > 0 ? array[index - 1] + 1 : 0)
    cursorNew += diff
    cursorOld += diff
    // copy over the replacement for sub part of bufffer
    newSub.copy(res, cursorNew, 0, newSub.length)
    // advance cursor by replacement length
    cursorNew += newSub.length
    // advance cursor by one except for the last time. At last call we just want
    // to copy until the end of the resulting buffer
    index !== array.length ? cursorOld += sub.length : 0
  })
  // this doesn't need iteration since we just want to fill the rest of the
  // buffer starting from the last occurence until computed end.
  this.copy(res, cursorNew, cursorOld, res.length)

  return res
}


function main(conf) {
  let buf = null
  let len = null
  var n = +conf.n;

  switch (conf.size) {
    case 'small':
      buf = smallBuf
      len = n * 1024
      break;
    case 'big':
      buf = bigBuf
      len = 10
      break;
    default:
      buf = smallBuf
      len = n
  }
  switch (conf.type) {
    case 'js':
      splitAndJoin = function (buf) {
        return buf.replace(find, replace)
      }
      break;
    case 'string':
      splitAndJoin = function (buf) {
        let res = buf.toString().split(findStr)
        return res.join(replaceStr)
      }
      break;
    case 'cpp':
      splitAndJoin = function (buf) {
        let res = buf.split(find)
        return Buffer.join(res, replace)
      }
      break;
  }

  bench.start();
  let res = null
  for (var i = 0; i < len; i++) {
    res = splitAndJoin(buf)
  }

  if (conf.assert === 'true' && conf.size === 'small') {
    assert(res.toString() === smallAssert)
  }

  if (conf.assert === 'true' && conf.size === 'big') {
    assert(res.toString().length === bigAssert)
  }
  res = null
  buf = null
  len = null
  bench.end(n);
}
