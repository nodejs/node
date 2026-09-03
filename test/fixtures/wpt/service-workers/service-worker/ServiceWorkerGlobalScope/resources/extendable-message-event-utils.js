var ExtendableMessageEventUtils = {};

// Create a representation of a given ExtendableMessageEvent that is suitable
// for transmission via the `postMessage` API.
ExtendableMessageEventUtils.serialize = function(event) {
  var ports = event.ports.map(function(port) {
        return { constructor: { name: port.constructor.name } };
    });
  return {
    constructor: {
      name: event.constructor.name
    },
    origin: event.origin,
    lastEventId: event.lastEventId,
    source: {
      constructor: {
        name: event.source.constructor.name
      },
      url: event.source.url,
      frameType: event.source.frameType,
      visibilityState: event.source.visibilityState,
      focused: event.source.focused
    },
    ports: ports
  };
};

// Compare the actual and expected values of an ExtendableMessageEvent that has
// been transformed using the `serialize` function defined in this file.
ExtendableMessageEventUtils.assert_equals = function(actual, expected) {
  assert_equals(
    actual.constructor.name, expected.constructor.name, 'event constructor'
  );
  assert_equals(actual.origin, expected.origin, 'event `origin` property');
  assert_equals(
    actual.lastEventId,
    expected.lastEventId,
    'event `lastEventId` property'
  );

  assert_equals(
    actual.source.constructor.name,
    expected.source.constructor.name,
    'event `source` property constructor'
  );
  assert_equals(
    actual.source.url, expected.source.url, 'event `source` property `url`'
  );
  assert_equals(
    actual.source.frameType,
    expected.source.frameType,
    'event `source` property `frameType`'
  );
  assert_equals(
    actual.source.visibilityState,
    expected.source.visibilityState,
    'event `source` property `visibilityState`'
  );
  assert_equals(
    actual.source.focused,
    expected.source.focused,
    'event `source` property `focused`'
  );

  assert_equals(
    actual.ports.length,
    expected.ports.length,
    'event `ports` property length'
  );

  for (var idx = 0; idx < expected.ports.length; ++idx) {
    assert_equals(
      actual.ports[idx].constructor.name,
      expected.ports[idx].constructor.name,
      'MessagePort #' + idx + ' constructor'
    );
  }
};
