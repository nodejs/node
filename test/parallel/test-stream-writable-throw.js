'use strict';

const common = require('../common');
const { Writable } = require('stream');
const assert = require('assert');

{
  const writable = new Writable({ autoDestroy: false });

  writable._write = () => {
    throw new Error();
  };
  writable.on('error', common.mustCall());
  writable.on('close', common.mustNotCall());
  writable.write('asd');
}

{
  const writable = new Writable({ autoDestroy: true });

  writable._write = () => {
    throw new Error();
  };
  writable.on('error', common.mustCall());
  writable.on('close', common.mustCall());
  writable.write('asd');
}

{
  const writable = new Writable({ autoDestroy: false });

  writable._final = () => {
    throw new Error();
  };
  writable.on('error', common.mustCall());
  writable.on('close', common.mustNotCall());
  writable.end('asd');
}

{
  const writable = new Writable({ autoDestroy: true });

  writable._final = () => {
    throw new Error();
  };
  writable.on('error', common.mustCall());
  writable.on('close', common.mustCall());
  writable.end('asd');
}
