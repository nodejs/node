'use strict';

// This file checks additional interface requirements, on top of the basic IDL
// that is validated in service-workers/idlharness.any.js

importScripts('/resources/testharness.js');

test(function() {
    var req = new Request('http://{{host}}/',
                          {method: 'POST',
                           headers: [['Content-Type', 'Text/Html']]});
    assert_equals(
      new ExtendableEvent('ExtendableEvent').type,
      'ExtendableEvent', 'Type of ExtendableEvent should be ExtendableEvent');
    assert_throws_js(TypeError, function() {
        new FetchEvent('FetchEvent');
    }, 'FetchEvent constructor with one argument throws');
    assert_throws_js(TypeError, function() {
        new FetchEvent('FetchEvent', {});
    }, 'FetchEvent constructor with empty init dict throws');
    assert_throws_js(TypeError, function() {
        new FetchEvent('FetchEvent', {request: null});
    }, 'FetchEvent constructor with null request member throws');
    assert_equals(
      new FetchEvent('FetchEvent', {request: req}).type,
      'FetchEvent', 'Type of FetchEvent should be FetchEvent');
    assert_equals(
      new FetchEvent('FetchEvent', {request: req}).cancelable,
      false, 'Default FetchEvent.cancelable should be false');
    assert_equals(
      new FetchEvent('FetchEvent', {request: req}).bubbles,
      false, 'Default FetchEvent.bubbles should be false');
    assert_equals(
      new FetchEvent('FetchEvent', {request: req}).clientId,
      '', 'Default FetchEvent.clientId should be the empty string');
    assert_equals(
      new FetchEvent('FetchEvent', {request: req, cancelable: false}).cancelable,
      false, 'FetchEvent.cancelable should be false');
    assert_equals(
      new FetchEvent('FetchEvent', {request: req, clientId : 'test-client-id'}).clientId, 'test-client-id',
      'FetchEvent.clientId with option {clientId : "test-client-id"} should be "test-client-id"');
    assert_equals(
      new FetchEvent('FetchEvent', {request : req}).request.url,
      'http://{{host}}/',
      'FetchEvent.request.url should return the value it was initialized to');
    assert_equals(
      new FetchEvent('FetchEvent', {request : req}).isReload,
      undefined,
      'FetchEvent.isReload should not exist');

  }, 'Event constructors');

test(() => {
    assert_false('XMLHttpRequest' in self);
  }, 'xhr is not exposed');

test(() => {
    assert_false('createObjectURL' in self.URL);
  }, 'URL.createObjectURL is not exposed')
