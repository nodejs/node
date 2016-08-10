'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertNPNProtocols(buffer, out);
  out.NPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.NPNProtocols.equals(Buffer.from('efgh')));
}

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertALPNProtocols(buffer, out);
  out.ALPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.ALPNProtocols.equals(Buffer.from('efgh')));
}
