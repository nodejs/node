'use strict';

// Use an infinite loop to prevent this service worker from advancing past the
// 'parsed' state.
let i = 0;
while (true) {
  ++i;
}
