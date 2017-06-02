// This is the same code from
// https://github.com/isaacs/node-graceful-fs/blob/master/fs.js just to test
// if we don't break userland modules which monkey-patches the native `fs`.

'use strict';

var mod = require('module');
var pre = '(function (exports, require, module, __filename, __dirname) { ';
var post = '});';
var src = pre + process.binding('natives').fs + post;
var vm = require('vm');
var fn = vm.runInThisContext(src);
fn(exports, require, module, __filename, __dirname);
