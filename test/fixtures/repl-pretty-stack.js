'use strict';

function a() {
  b();
}

function b() {
  c();
}

function c() {
  d(function() { throw new Error('Whoops!'); });
}

function d(f) {
  f();
}

a();