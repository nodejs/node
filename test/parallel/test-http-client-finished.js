'use strict';
const common = require('../common');
const http = require('http');
const { finished } = require('stream');

{
  // Test abort before finished.

  const server = http.createServer(function(req, res) {
    res.write('asd');
  });

  server.listen(0, common.mustCall(function() {
    http.request({
      port: this.address().port
    })
    .on('response', (res) => {
      res.on('readable', () => {
        res.destroy();
      });
      finished(res, common.mustCall(() => {
        server.close();
      }));
    })
    .end();
  }));
}

{
  // Test abort before finished.

  const server = http.createServer(function(req, res) {
  });

  server.listen(0, common.mustCall(function() {
    const req = http.request({
      port: this.address().port
    }, common.mustNotCall());
    req.abort();
    finished(req, common.mustCall(() => {
      server.close();
    }));
  }));
}

{
  // Test abort after request.

  const server = http.createServer(function(req, res) {
  });

  server.listen(0, common.mustCall(function() {
    const req = http.request({
      port: this.address().port
    }).end();
    finished(req, (err) => {
      common.expectsError({
        name: 'Error',
        code: 'ERR_STREAM_PREMATURE_CLOSE'
      })(err);
      finished(req, common.mustCall(() => {
        server.close();
      }));
    });
    req.abort();
  }));
}

{
  // Test abort before end.

  const server = http.createServer(function(req, res) {
    res.write('test');
  });

  server.listen(0, common.mustCall(function() {
    const req = http.request({
      port: this.address().port
    }).on('response', common.mustCall((res) => {
      req.abort();
      finished(res, common.mustCall(() => {
        finished(res, common.mustCall(() => {
          server.close();
        }));
      }));
    })).end();
  }));
}

{
  // Test destroy before end.

  const server = http.createServer(function(req, res) {
    res.write('test');
  });

  server.listen(0, common.mustCall(function() {
    http.request({
      port: this.address().port
    }).on('response', common.mustCall((res) => {
      // TODO(ronag): Bug? Won't emit 'close' unless read.
      res.on('data', () => {});
      res.destroy();
      finished(res, common.mustCall(() => {
        finished(res, common.mustCall(() => {
          server.close();
        }));
      }));
    })).end();
  }));
}

{
  // Test finish after end.

  const server = http.createServer(function(req, res) {
    res.end('asd');
  });

  server.listen(0, common.mustCall(function() {
    http.request({
      port: this.address().port
    }).on('response', common.mustCall((res) => {
      // TODO(ronag): Bug? Won't emit 'close' unless read.
      res.on('data', () => {});
      finished(res, common.mustCall(() => {
        finished(res, common.mustCall(() => {
          server.close();
        }));
      }));
    })).end();
  }));
}
