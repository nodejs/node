'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const debug = require('node:util').debuglog('test');

const testResBody = 'response content\n';

{
  // Happy flow - string argument

  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      link: '</styles.css>; rel=preload; as=style'
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });

    debug('Client sending request...');

    req.on('information', common.mustCall((res) => {
      assert.strictEqual(res.headers.link, '</styles.css>; rel=preload; as=style');
    }));

    req.on('response', common.mustCall((res) => {
      let body = '';

      assert.strictEqual(res.statusCode, 200, `Final status code was ${res.statusCode}, not 200.`);

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', common.mustCall(() => {
        debug('Got full response.');
        assert.strictEqual(body, testResBody);
        server.close();
      }));
    }));

    req.end();
  }));
}

{
  // Happy flow - array argument

  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      link: [
        '</styles.css>; rel=preload; as=style',
        '</scripts.js>; crossorigin; rel=preload; as=script',
        '</scripts.js>; rel=preload; as=script; crossorigin',
      ]
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });
    debug('Client sending request...');

    req.on('information', common.mustCall((res) => {
      assert.strictEqual(
        res.headers.link,
        '</styles.css>; rel=preload; as=style, </scripts.js>; crossorigin; ' +
        'rel=preload; as=script, </scripts.js>; rel=preload; as=script; crossorigin'
      );
    }));

    req.on('response', common.mustCall((res) => {
      let body = '';

      assert.strictEqual(res.statusCode, 200, `Final status code was ${res.statusCode}, not 200.`);

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', common.mustCall(() => {
        debug('Got full response.');
        assert.strictEqual(body, testResBody);
        server.close();
      }));
    }));

    req.end();
  }));
}

{
  // Happy flow - empty array

  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      link: []
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });
    debug('Client sending request...');

    req.on('information', common.mustNotCall());

    req.on('response', common.mustCall((res) => {
      let body = '';

      assert.strictEqual(res.statusCode, 200, `Final status code was ${res.statusCode}, not 200.`);

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', common.mustCall(() => {
        debug('Got full response.');
        assert.strictEqual(body, testResBody);
        server.close();
      }));
    }));

    req.end();
  }));
}

{
  // Happy flow - object argument with string

  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      'link': '</styles.css>; rel=preload; as=style',
      'x-trace-id': 'id for diagnostics'
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });
    debug('Client sending request...');

    req.on('information', common.mustCall((res) => {
      assert.strictEqual(
        res.headers.link,
        '</styles.css>; rel=preload; as=style'
      );
      assert.strictEqual(res.headers['x-trace-id'], 'id for diagnostics');
    }));

    req.on('response', common.mustCall((res) => {
      let body = '';

      assert.strictEqual(res.statusCode, 200, `Final status code was ${res.statusCode}, not 200.`);

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', common.mustCall(() => {
        debug('Got full response.');
        assert.strictEqual(body, testResBody);
        server.close();
      }));
    }));

    req.end();
  }));
}

{
  // Happy flow - object argument with array of strings

  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      'link': [
        '</styles.css>; rel=preload; as=style',
        '</scripts.js>; rel=preload; as=script',
      ],
      'x-trace-id': 'id for diagnostics'
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });
    debug('Client sending request...');

    req.on('information', common.mustCall((res) => {
      assert.strictEqual(
        res.headers.link,
        '</styles.css>; rel=preload; as=style, </scripts.js>; rel=preload; as=script'
      );
      assert.strictEqual(res.headers['x-trace-id'], 'id for diagnostics');
    }));

    req.on('response', common.mustCall((res) => {
      let body = '';

      assert.strictEqual(res.statusCode, 200, `Final status code was ${res.statusCode}, not 200.`);

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', common.mustCall(() => {
        debug('Got full response.');
        assert.strictEqual(body, testResBody);
        server.close();
      }));
    }));

    req.end();
  }));
}

{
  // Happy flow - empty object

  const server = http.createServer(common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({});

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port, path: '/'
    });
    debug('Client sending request...');

    req.on('information', common.mustNotCall());

    req.on('response', common.mustCall((res) => {
      let body = '';

      assert.strictEqual(res.statusCode, 200, `Final status code was ${res.statusCode}, not 200.`);

      res.on('data', (chunk) => {
        body += chunk;
      });

      res.on('end', common.mustCall(() => {
        debug('Got full response.');
        assert.strictEqual(body, testResBody);
        server.close();
      }));
    }));

    req.end();
  }));
}
