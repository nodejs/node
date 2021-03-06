'use strict';
require('../common');
const repl = require('repl');

const replserver = new repl.REPLServer();

replserver._inTemplateLiteral = true;

// `null` gets treated like an empty string. (Should it? You have to do some
// strange business to get it into the REPL. Maybe it should really throw?)

replserver.emit('line', null);
replserver.emit('line', '.exit');
