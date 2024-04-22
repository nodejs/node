'use strict';
const common = require('../common');
const repl = require('repl');

var r = repl.start({});
r.setupHistory(__dirname + '/history', () => { }); // Different results will occur here
r.close();