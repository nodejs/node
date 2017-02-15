// Flags: --allow_natives_syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server =
    http.createServer(onrequest).listen(0, common.localhostIPv4, () => next(0));

function onrequest(req, res) {
  res.end('ok');
  onrequest.requests.push(req);
  onrequest.responses.push(res);
}
onrequest.requests = [];
onrequest.responses = [];

function next(n) {
  const { address: host, port } = server.address();
  const req = http.get({ host, port });
  req.once('response', (res) => onresponse(n, req, res));
}

function onresponse(n, req, res) {
  res.resume();

  if (n < 3) {
    res.once('end', () => next(n + 1));
  } else {
    server.close();
  }

  onresponse.requests.push(req);
  onresponse.responses.push(res);
}
onresponse.requests = [];
onresponse.responses = [];

function allSame(list) {
  assert(list.length >= 2);
  // Use |elt| in no-op position to pacify eslint.
  for (const elt of list) elt, eval('%DebugPrint(elt)');
  for (const elt of list) elt, assert(eval('%HaveSameMap(list[0], elt)'));
}

process.on('exit', () => {
  eval('%CollectGarbage(0)');
  // TODO(bnoordhuis) Investigate why the first IncomingMessage ends up
  // with a deprecated map.  The map is stable after the first request.
  allSame(onrequest.requests.slice(1));
  allSame(onrequest.responses);
  allSame(onresponse.requests);
  allSame(onresponse.responses);
});
