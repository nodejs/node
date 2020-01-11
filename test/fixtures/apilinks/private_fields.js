'use strict';

// Acorn does not support private fields.
// Verify that apilinks creates empty output
// instead of crashing.

class Foo {
  #a = 1;
}
