"use strict";
// RegExps used to be special-cased in Web IDL, but that was removed in
// https://github.com/heycam/webidl/commit/bbb2bde. These tests check that implementations no longer
// do any such special-casing.

test(() => {
  const regExp = new RegExp();
  regExp.message = "some message";

  const errorEvent = new ErrorEvent("type", regExp);

  assert_equals(errorEvent.message, "some message");
}, "Conversion to a dictionary works");

test(() => {
  const messageChannel = new MessageChannel();
  const regExp = new RegExp();
  regExp[Symbol.iterator] = function* () {
    yield messageChannel.port1;
  };

  const messageEvent = new MessageEvent("type", { ports: regExp });

  assert_array_equals(messageEvent.ports, [messageChannel.port1]);
}, "Conversion to a sequence works");

promise_test(async () => {
  const regExp = new RegExp();

  const response = new Response(regExp);

  assert_equals(await response.text(), "/(?:)/");
}, "Can convert a RegExp to a USVString");

test(() => {
  let functionCalled = false;

  const regExp = new RegExp();
  regExp.handleEvent = () => {
    functionCalled = true;
  };

  self.addEventListener("testevent", regExp);
  self.dispatchEvent(new Event("testevent"));

  assert_true(functionCalled);
}, "Can be used as an object implementing a callback interface");
