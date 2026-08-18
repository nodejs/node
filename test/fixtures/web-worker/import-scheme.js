'use strict';

let name = null;
try {
  // The only accepted schemes are 'blob', 'file', and 'data'
  importScripts('https://nodejs.org/worker.js');
} catch (err) {
  name = err.name;
}

postMessage(name);
