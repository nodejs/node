'use strict';

const common = require('../common');
const makeDuplexPair = require('../common/duplexpair');

const { clientSide, serverSide } = makeDuplexPair();

serverSide.resume();
serverSide.on('end', function(buf) {
  serverSide.write('out');
  serverSide.write('');
  serverSide.end();
});

clientSide.end();

clientSide.on('data', common.mustCall());
clientSide.on('end', common.mustCall());
