// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

'use strict';

// https://w3c.github.io/resource-timing/

idl_test(
  ['resource-timing'],
  ['performance-timeline', 'hr-time', 'dom', 'html'],
  idl_array => {
    try {
      self.resource = performance.getEntriesByType('resource')[0];
    } catch (e) {
      // Will be surfaced when resource is undefined below.
    }

    idl_array.add_objects({
      Performance: ['performance'],
      PerformanceResourceTiming: ['resource']
    });
  }
);
