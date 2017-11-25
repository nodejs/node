'use strict';

function take(it, n) {
  const result = [];
  for (const e of it) {
    if (--n < 0)
      break;
    result.push(e);
  }
  return result;
}

function previewMapIterator(it, n) {
  return take(%MapIteratorClone(it), n);
}

function previewSetIterator(it, n) {
  return take(%SetIteratorClone(it), n);
}

module.exports = { previewMapIterator, previewSetIterator };
