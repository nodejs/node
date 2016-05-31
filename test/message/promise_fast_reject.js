'use strict';
require('../common');

new Promise(function(res, rej) {
  consol.log('One');
});

new Promise(function(res, rej) {
  consol.log('Two');
});
