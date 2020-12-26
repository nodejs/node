'use strict';
const common = require('../common');
const net = require('net');
const { finished } = require('stream');

const server = net.createServer(function(con) {
  con.on('close', common.mustCall());
});

server.listen(0, common.mustCall(function() {
  const con = net.connect({
    port: this.address().port
  })
  .on('connect', () => {
    finished(con, common.mustCall());
    con.destroy();
    finished(con, common.mustCall());
    con.on('close', common.mustCall(() => {
      finished(con, common.mustCall(() => {
        server.close();
      }));
    }));
  })
  .end();
}));
