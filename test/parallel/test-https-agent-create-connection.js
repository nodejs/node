'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

const agent = new https.Agent();

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

const expectedHeader = /^HTTP\/1\.1 200 OK/;
const expectedBody = /hello world\n/;
const expectCertError = /^Error: unable to verify the first certificate$/;

const checkRequest = (socket, server) => {
  let result = '';
  socket.on('connect', common.mustCall((data) => {
    socket.write('GET / HTTP/1.1\r\nHost: example.com\r\n\r\n');
    socket.end();
  }));
  socket.on('data', common.mustCall((chunk) => {
    result += chunk;
  }));
  socket.on('end', common.mustCall(() => {
    assert.match(result, expectedHeader);
    assert.match(result, expectedBody);
    server.close();
  }));
};

function createServer() {
  return https.createServer(options, (req, res) => {
    res.end('hello world\n');
  });
}

// use option connect
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = {
      port: port,
      host: host,
      rejectUnauthorized: false,
      _agentKey: agent.getName({ port, host })
    };

    const socket = agent.createConnection(options);
    checkRequest(socket, server);
  }));
}

// Use port and option connect
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = {
      rejectUnauthorized: false,
      _agentKey: agent.getName({ port, host })
    };
    const socket = agent.createConnection(port, options);
    checkRequest(socket, server);
  }));
}

// Use port and host and option connect
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = {
      rejectUnauthorized: false,
      _agentKey: agent.getName({ port, host })
    };
    const socket = agent.createConnection(port, host, options);
    checkRequest(socket, server);
  }));
}

// Use port and host and option does not have agentKey
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = {
      rejectUnauthorized: false,
    };
    const socket = agent.createConnection(port, host, options);
    checkRequest(socket, server);
  }));
}

// `options` is null
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = null;
    const socket = agent.createConnection(port, host, options);
    socket.on('error', common.mustCall((e) => {
      assert.match(e.toString(), expectCertError);
      server.close();
    }));
  }));
}

// `options` is undefined
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = undefined;
    const socket = agent.createConnection(port, host, options);
    socket.on('error', common.mustCall((e) => {
      assert.match(e.toString(), expectCertError);
      server.close();
    }));
  }));
}

// `options` should not be modified
{
  const server = createServer();
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const host = 'localhost';
    const options = common.mustNotMutateObjectDeep({
      port: 3000,
      rejectUnauthorized: false,
    });

    const socket = agent.createConnection(port, host, options);
    socket.on('connect', common.mustCall((data) => {
      socket.end();
    }));
    socket.on('end', common.mustCall(() => {
      server.close();
    }));
  }));
}
