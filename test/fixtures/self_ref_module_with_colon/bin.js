#!/usr/bin/env node

'use strict';

try {
  console.log(require('file:package'));
} catch (e) {
  console.log(e);
}
import('file:package').then(console.log, console.log);
import('file%3Apackage').then(console.log, console.log);
