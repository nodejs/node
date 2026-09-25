'use strict';

postMessage('before-close');
// close() terminates the worker immediately
close();
// This shouldn't be called
postMessage('after-close');
