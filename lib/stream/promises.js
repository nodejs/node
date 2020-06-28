'use strict';

const {
  Promise,
} = primordials;

let pl;
let eos;

function pipeline(...streams) {
  if (!pl) pl = require('internal/streams/pipeline');
  return new Promise((resolve, reject) => {
    pl(...streams, (err, value) => {
      if (err) {
        reject(err);
      } else {
        resolve(value);
      }
    });
  });
}

function finished(stream, opts) {
  if (!eos) eos = require('internal/streams/end-of-stream');
  return new Promise((resolve, reject) => {
    eos(stream, opts, (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

module.exports = {
  finished,
  pipeline,
};
