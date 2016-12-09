'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

if (!process.features.tls_alpn) {
  console.error('Skipping because node compiled without OpenSSL or ' +
                'with old OpenSSL version.');
  process.exit(0);
}

const assert = require('assert');
const fs = require('fs');
const tls = require('tls');

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var serverIP = common.localhostIPv4;

function checkResults(result, expected) {
  assert.strictEqual(result.server.ALPN, expected.server.ALPN);
  assert.strictEqual(result.server.NPN, expected.server.NPN);
  assert.strictEqual(result.client.ALPN, expected.client.ALPN);
  assert.strictEqual(result.client.NPN, expected.client.NPN);
}

function runTest(clientsOptions, serverOptions, cb) {
  serverOptions.key = loadPEM('agent2-key');
  serverOptions.cert = loadPEM('agent2-cert');
  var results = [];
  var index = 0;
  var server = tls.createServer(serverOptions, function(c) {
    results[index].server = {ALPN: c.alpnProtocol, NPN: c.npnProtocol};
  });

  server.listen(0, serverIP, function() {
    connectClient(clientsOptions);
  });

  function connectClient(options) {
    var opt = options.shift();
    opt.port = server.address().port;
    opt.host = serverIP;
    opt.rejectUnauthorized = false;

    results[index] = {};
    var client = tls.connect(opt, function() {
      results[index].client = {ALPN: client.alpnProtocol,
                               NPN: client.npnProtocol};
      client.destroy();
      if (options.length) {
        index++;
        connectClient(options);
      } else {
        server.close();
        cb(results);
      }
    });
  }

}

// Server: ALPN/NPN, Client: ALPN/NPN
function Test1() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    ALPNProtocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e'],
    NPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y'],
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by ALPN
    checkResults(results[0],
                 {server: {ALPN: 'a', NPN: false},
                  client: {ALPN: 'a', NPN: undefined}});
    // 'b' is selected by ALPN
    checkResults(results[1],
                 {server: {ALPN: 'b', NPN: false},
                  client: {ALPN: 'b', NPN: undefined}});
    // nothing is selected by ALPN
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'first-priority-unsupported'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test2();
  });
}

// Server: ALPN/NPN, Client: ALPN
function Test2() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    ALPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by ALPN
    checkResults(results[0],
                 {server: {ALPN: 'a', NPN: false},
                  client: {ALPN: 'a', NPN: undefined}});
    // 'b' is selected by ALPN
    checkResults(results[1],
                 {server: {ALPN: 'b', NPN: false},
                  client: {ALPN: 'b', NPN: undefined}});
    // nothing is selected by ALPN
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test3();
  });
}

// Server: ALPN/NPN, Client: NPN
function Test3() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    NPNProtocols: ['a', 'b', 'c']
  }, {
    NPPNProtocols: ['c', 'b', 'e']
  }, {
    NPPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by NPN
    checkResults(results[0],
                 {server: {ALPN: false, NPN: 'a'},
                  client: {ALPN: false, NPN: 'a'}});
    // nothing is selected by ALPN
    checkResults(results[1],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test4();
  });
}

// Server: ALPN/NPN, Client: Nothing
function Test4() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{}, {}, {}];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected by ALPN
    checkResults(results[0],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[1],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test5();
  });
}

// Server: ALPN, Client: ALPN/NPN
function Test5() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    ALPNProtocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e'],
    NPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y'],
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by ALPN
    checkResults(results[0], {server: {ALPN: 'a', NPN: false},
                              client: {ALPN: 'a', NPN: undefined}});
    // 'b' is selected by ALPN
    checkResults(results[1], {server: {ALPN: 'b', NPN: false},
                              client: {ALPN: 'b', NPN: undefined}});
    // nothing is selected by ALPN
    checkResults(results[2], {server: {ALPN: false,
                                       NPN: 'first-priority-unsupported'},
                              client: {ALPN: false, NPN: false}});
    // execute next test
    Test6();
  });
}

// Server: ALPN, Client: ALPN
function Test6() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    ALPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by ALPN
    checkResults(results[0], {server: {ALPN: 'a', NPN: false},
                              client: {ALPN: 'a', NPN: undefined}});
    // 'b' is selected by ALPN
    checkResults(results[1], {server: {ALPN: 'b', NPN: false},
                              client: {ALPN: 'b', NPN: undefined}});
    // nothing is selected by ALPN
    checkResults(results[2], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // execute next test
    Test7();
  });
}

// Server: ALPN, Client: NPN
function Test7() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    NPNProtocols: ['a', 'b', 'c']
  }, {
    NPNProtocols: ['c', 'b', 'e']
  }, {
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected by ALPN
    checkResults(results[0], {server: {ALPN: false, NPN: 'a'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[1], {server: {ALPN: false, NPN: 'c'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'first-priority-unsupported'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test8();
  });
}

// Server: ALPN, Client: Nothing
function Test8() {
  var serverOptions = {
    ALPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{}, {}, {}];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected by ALPN
    checkResults(results[0], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[1], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected by ALPN
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test9();
  });
}

// Server: NPN, Client: ALPN/NPN
function Test9() {
  var serverOptions = {
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    ALPNrotocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e'],
    NPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y'],
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by NPN
    checkResults(results[0], {server: {ALPN: false, NPN: 'a'},
                              client: {ALPN: false, NPN: 'a'}});
    // 'b' is selected by NPN
    checkResults(results[1], {server: {ALPN: false, NPN: 'b'},
                              client: {ALPN: false, NPN: 'b'}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'first-priority-unsupported'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test10();
  });
}

// Server: NPN, Client: ALPN
function Test10() {
  var serverOptions = {
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    ALPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[1], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[2], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // execute next test
    Test11();
  });
}

// Server: NPN, Client: NPN
function Test11() {
  var serverOptions = {
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{
    NPNProtocols: ['a', 'b', 'c']
  }, {
    NPNProtocols: ['c', 'b', 'e']
  }, {
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by NPN
    checkResults(results[0], {server: {ALPN: false, NPN: 'a'},
                              client: {ALPN: false, NPN: 'a'}});
    // 'b' is selected by NPN
    checkResults(results[1], {server: {ALPN: false, NPN: 'b'},
                              client: {ALPN: false, NPN: 'b'}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'first-priority-unsupported'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test12();
  });
}

// Server: NPN, Client: Nothing
function Test12() {
  var serverOptions = {
    NPNProtocols: ['a', 'b', 'c']
  };

  var clientsOptions = [{}, {}, {}];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[1], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test13();
  });
}

// Server: Nothing, Client: ALPN/NPN
function Test13() {
  var serverOptions = {};

  var clientsOptions = [{
    ALPNrotocols: ['a', 'b', 'c'],
    NPNProtocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e'],
    NPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y'],
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], {server: {ALPN: false, NPN: 'a'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[1], {server: {ALPN: false, NPN: 'c'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'first-priority-unsupported'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test14();
  });
}

// Server: Nothing, Client: ALPN
function Test14() {
  var serverOptions = {};

  var clientsOptions = [{
    ALPNrotocols: ['a', 'b', 'c']
  }, {
    ALPNProtocols: ['c', 'b', 'e']
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[1], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test15();
  });
}

// Server: Nothing, Client: NPN
function Test15() {
  var serverOptions = {};

  var clientsOptions = [{
    NPNProtocols: ['a', 'b', 'c']
  }, {
    NPNProtocols: ['c', 'b', 'e']
  }, {
    NPNProtocols: ['first-priority-unsupported', 'x', 'y']
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], {server: {ALPN: false, NPN: 'a'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[1], {server: {ALPN: false, NPN: 'c'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'first-priority-unsupported'},
                  client: {ALPN: false, NPN: false}});
    // execute next test
    Test16();
  });
}

// Server: Nothing, Client: Nothing
function Test16() {
  var serverOptions = {};

  var clientsOptions = [{}, {}, {}];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[1], {server: {ALPN: false, NPN: 'http/1.1'},
                              client: {ALPN: false, NPN: false}});
    // nothing is selected
    checkResults(results[2],
                 {server: {ALPN: false, NPN: 'http/1.1'},
                  client: {ALPN: false, NPN: false}});
  });
}

Test1();
