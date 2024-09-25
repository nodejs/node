function testResourceInitiator(resourceName, expectedInitiator) {
    return new Promise(resolve => {
      const observer = new PerformanceObserver(list => {
        const entries = list.getEntriesByType('resource');
        for (const entry of entries) {
          if (entry.name.endsWith(resourceName)) {
            observer.disconnect();
            assert_equals(entry.initiator, expectedInitiator, `Test ${resourceName} initiator`);
            resolve();
            return;
          }
        }
      });
      observer.observe({entryTypes: ['resource']});
    });
}