'use strict';

const {
  mustCall,
  mustNotCall,
  hasCrypto,
  platformTimeout,
  skip
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { strictEqual } = require('assert');
const {
  createServer,
  connect,
  constants: {
    HTTP2_HEADER_STATUS,
    HTTP_STATUS_OK
  }
} = require('http2');

{
  // Http2ServerResponse.end accepts chunk, encoding, cb as args
  // It may be invoked repeatedly without throwing errors
  // but callback will only be called once
  const server = createServer(mustCall((request, response) => {
    response.end('end', 'utf8', mustCall(() => {
      response.end(mustNotCall());
      process.nextTick(() => {
        response.end(mustNotCall());
        server.close();
      });
    }));
    response.on('finish', mustCall(() => {
      response.end(mustNotCall());
    }));
    response.end(mustNotCall());
  }));
  server.listen(0, mustCall(() => {
    let data = '';
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.setEncoding('utf8');
      request.on('data', (chunk) => (data += chunk));
      request.on('end', mustCall(() => {
        strictEqual(data, 'end');
        client.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}

{
  // Http2ServerResponse.end can omit encoding arg, sets it to utf-8
  const server = createServer(mustCall((request, response) => {
    response.end('test\uD83D\uDE00', mustCall(() => {
      server.close();
    }));
  }));
  server.listen(0, mustCall(() => {
    let data = '';
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.setEncoding('utf8');
      request.on('data', (chunk) => (data += chunk));
      request.on('end', mustCall(() => {
        strictEqual(data, 'test\uD83D\uDE00');
        client.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}

{
  // Http2ServerResponse.end can omit chunk & encoding args
  const server = createServer(mustCall((request, response) => {
    response.end(mustCall(() => {
      server.close();
    }));
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => client.close()));
      request.end();
      request.resume();
    }));
  }));
}

{
  // Http2ServerResponse.end is necessary on HEAD requests in compat
  // for http1 compatibility
  const server = createServer(mustCall((request, response) => {
    strictEqual(response.finished, true);
    response.writeHead(HTTP_STATUS_OK, { foo: 'bar' });
    response.end('data', mustCall());
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('response', mustCall((headers, flags) => {
        strictEqual(headers[HTTP2_HEADER_STATUS], HTTP_STATUS_OK);
        strictEqual(flags, 5); // the end of stream flag is set
        strictEqual(headers.foo, 'bar');
      }));
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}

{
  // .end should trigger 'end' event on request if user did not attempt
  // to read from the request
  const server = createServer(mustCall((request, response) => {
    request.on('end', mustCall());
    response.end();
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}


{
  // Should be able to call .end with cb from stream 'close'
  const server = createServer(mustCall((request, response) => {
    response.writeHead(HTTP_STATUS_OK, { foo: 'bar' });
    response.stream.on('close', mustCall(() => {
      response.end(mustCall());
    }));
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('response', mustCall((headers, flags) => {
        strictEqual(headers[HTTP2_HEADER_STATUS], HTTP_STATUS_OK);
        strictEqual(flags, 5); // the end of stream flag is set
        strictEqual(headers.foo, 'bar');
      }));
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}

{
  // Should be able to respond to HEAD request after timeout
  const server = createServer(mustCall((request, response) => {
    setTimeout(mustCall(() => {
      response.writeHead(HTTP_STATUS_OK, { foo: 'bar' });
      response.end('data', mustCall());
    }), platformTimeout(10));
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('response', mustCall((headers, flags) => {
        strictEqual(headers[HTTP2_HEADER_STATUS], HTTP_STATUS_OK);
        strictEqual(flags, 5); // the end of stream flag is set
        strictEqual(headers.foo, 'bar');
      }));
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}

{
  // finish should only trigger after 'end' is called
  const server = createServer(mustCall((request, response) => {
    let finished = false;
    response.writeHead(HTTP_STATUS_OK, { foo: 'bar' });
    response.on('finish', mustCall(() => {
      finished = false;
    }));
    response.end('data', mustCall(() => {
      strictEqual(finished, false);
      response.end('data', mustNotCall());
    }));
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('response', mustCall((headers, flags) => {
        strictEqual(headers[HTTP2_HEADER_STATUS], HTTP_STATUS_OK);
        strictEqual(flags, 5); // the end of stream flag is set
        strictEqual(headers.foo, 'bar');
      }));
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}

{
  // Should be able to respond to HEAD with just .end
  const server = createServer(mustCall((request, response) => {
    response.end('data', mustCall());
    response.end(mustNotCall());
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('response', mustCall((headers, flags) => {
        strictEqual(headers[HTTP2_HEADER_STATUS], HTTP_STATUS_OK);
        strictEqual(flags, 5); // the end of stream flag is set
      }));
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}
