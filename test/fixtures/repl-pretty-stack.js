'use strict';

function a() {
  b();
}

function b() {
  c();
}

function c() {
  throw new Error('Whoops!');
}

a();