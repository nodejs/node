'use strict';
require('../common');
const { startNewREPLServer } = require('../common/repl');

const { replServer } = startNewREPLServer();

replServer._inTemplateLiteral = true;

// `null` gets treated like an empty string. (Should it? You have to do some
// strange business to get it into the REPL. Maybe it should really throw?)

replServer.emit('line', null);
replServer.emit('line', '.exit');
