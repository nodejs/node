'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

fs.promises.open(__filename).then(common.mustCall((fd) => {
  const rs = new fs.ReadStream(null, {
    fd: fd,
    manualStart: false
  });
  setTimeout(() => assert(rs.bytesRead > 0), common.platformTimeout(10));
}));

fs.promises.open(__filename).then(common.mustCall((fd) => {
  const rs = new fs.ReadStream(null, {
    fd: fd,
    manualStart: true
  });
  setTimeout(() => assert(rs.bytesRead === 0), common.platformTimeout(10));
}));
