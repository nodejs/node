'use strict';

// Legacy _third_party_main.js support

markBootstrapComplete();

process.nextTick(() => {
  require('_third_party_main');
});
