// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

// https://w3c.github.io/user-timing/

'use strict';

idl_test(
  ['user-timing'],
  ['hr-time', 'performance-timeline', 'dom'],
  idl_array => {
    try {
      performance.mark('test');
      performance.measure('test');
      for (const m of performance.getEntriesByType('mark')) {
        self.mark = m;
      }
      for (const m of performance.getEntriesByType('measure')) {
        self.measure = m;
      }
    } catch (e) {
      // Will be surfaced when mark is undefined below.
    }

    idl_array.add_objects({
      Performance: ['performance'],
      PerformanceMark: ['mark'],
      PerformanceMeasure: ['measure'],
    });
  }
);
