// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

idl_test(
  ['encoding'],
  [], // No deps
  idl_array => {
    idl_array.add_objects({
      TextEncoder: ['new TextEncoder()'],
      TextDecoder: ['new TextDecoder()']
    });
  }
);
