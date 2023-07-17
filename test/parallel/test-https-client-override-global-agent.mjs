import { hasCrypto, skip, mustCall } from '../common/index.mjs';
if (!hasCrypto) skip('missing crypto');
import { readKey } from '../common/fixtures.mjs';
import assert from 'assert';
// To override https.globalAgent we need to use namespace import
import * as https from 'https';

// Disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const options = {
  key: readKey('agent1-key.pem'),
  cert: readKey('agent1-cert.pem'),
};

const server = https.Server(
  options,
  mustCall((req, res) => {
    res.writeHead(200);
    res.end('Hello, World!');
  })
);

server.listen(
  0,
  mustCall(() => {
    const agent = new https.Agent();
    const name = agent.getName({ port: server.address().port });
    // That is exactly what we want here:)
    /* eslint-disable no-import-assign */
    https.globalAgent = agent;

    makeRequest();
    assert(name in agent.sockets); // Agent has indeed been used
  })
);

function makeRequest() {
  const req = https.get({
    port: server.address().port,
  });
  req.on('close', () => server.close());
}
