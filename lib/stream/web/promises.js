'use strict';

const {
  finished: noPromiseFinished,
} = require('internal/webstreams/finished');
const {
  pipeline: noPromisePipeline,
} = require('internal/webstreams/pipeline');


function finished(stream) {
  return new Promise((resolve, reject) => {
    noPromiseFinished(stream, (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

function pipeline(...streams) {
  return new Promise((resolve, reject) => {
    noPromisePipeline(streams, (err, value) => {
      if (err) {
        reject(err);
      } else {
        resolve(value);
      }
    });
  });
}

module.exports = {
  finished,
  pipeline,
};