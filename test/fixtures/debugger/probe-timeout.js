'use strict';

function neverCalled() {
  console.log('never');
}

setInterval(() => {}, 1000);
