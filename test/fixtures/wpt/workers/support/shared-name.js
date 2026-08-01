"use strict";

self.counter = 0;

self.onconnect = e => {
  ++self.counter;
  e.source.postMessage({ counter: self.counter, name: self.name });
};
