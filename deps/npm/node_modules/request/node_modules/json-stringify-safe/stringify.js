module.exports = stringify;

function getSerialize (fn, decycle) {
  var seen = [];
  decycle = decycle || function(key, value) {
    return '[Circular]';
  };
  return function(key, value) {
    var ret = value;
    if (typeof value === 'object' && value) {
      if (seen.indexOf(value) !== -1)
        ret = decycle(key, value);
      else
        seen.push(value);
    }
    if (fn) ret = fn(key, ret);
    return ret;
  }
}

function stringify(obj, fn, spaces, decycle) {
  return JSON.stringify(obj, getSerialize(fn, decycle), spaces);
}

stringify.getSerialize = getSerialize;
