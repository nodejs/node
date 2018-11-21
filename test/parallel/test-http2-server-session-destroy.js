'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();
server.listen(0, common.mustCall(() => {
  h2.connect(`http://localhost:${server.address().port}`, (session) => {
    session.request({ ':method': 'POST' }).end(common.mustCall(() => {
      session.destroy();
      server.close();
    }));
  });
}));
