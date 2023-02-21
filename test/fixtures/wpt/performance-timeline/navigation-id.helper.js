// The test functions called in the navigation-counter test. They rely on
// artifacts defined in
// '/html/browsers/browsing-the-web/back-forward-cache/resources/helper.sub.js'
// which should be included before this file to use these functions.

// This function is to obtain navigation ids of all performance entries to
// verify.
let testInitial = () => {
  return window.performance.getEntries().map(e => e.navigationId);
}

let testMarkMeasure = (expectedNavigationId, markName, MeasureName) => {
  const markName1 = 'test-mark';
  const markName2 = 'test-mark' + expectedNavigationId;
  const measureName = 'test-measure' + expectedNavigationId;

  window.performance.mark(markName1);
  window.performance.mark(markName2);
  window.performance.measure(measureName, markName1, markName2);
  return window.performance.getEntriesByName(markName2).concat(
    window.performance.getEntriesByName(measureName)).map(e => e.navigationId);
}

let testResourceTiming = async (expectedNavigationId) => {
  let navigationId = -1;

  let p = new Promise(resolve => {
    new PerformanceObserver((list) => {
      const entry = list.getEntries().find(e => e.name.includes('json_resource') && e.navigationId == expectedNavigationId);
      if (entry) {
        navigationId = entry.navigationId;
        resolve();
      }
    }).observe({ type: 'resource' });
  });

  const resp = await fetch('/performance-timeline/resources/json_resource.json');
  await p;
  return [navigationId];
}

let testElementTiming = async (expectedNavigationId) => {
  let navigationId = -1;
  let p = new Promise(resolve => {
    new PerformanceObserver((list) => {
      const entry = list.getEntries().find(e => e.entryType === 'element' && e.identifier === 'test-element-timing' + expectedNavigationId);
      if (entry) {
        navigationId = entry.navigationId;
        resolve();
      }
    }).observe({ type: 'element' });
  });

  let el = document.createElement('p');
  el.setAttribute('elementtiming', 'test-element-timing' + expectedNavigationId);
  el.textContent = 'test element timing text';
  document.body.appendChild(el);
  await p;
  return [navigationId];
}

let testLongTask = async () => {
  let navigationIds = [];

  let p = new Promise(resolve => {
    new PerformanceObserver((list) => {
      const entry = list.getEntries().find(e => e.entryType === 'longtask')
      if (entry) {
        navigationIds.push(entry.navigationId);
        navigationIds = navigationIds.concat(
          entry.attribution.map(a => a.navigationId));
        resolve();
      }
    }).observe({ type: 'longtask' });
  });

  const script = document.createElement('script');
  script.src = '/performance-timeline/resources/make_long_task.js';
  document.body.appendChild(script);
  await p;
  document.body.removeChild(script);
  return navigationIds;
}

const testFunctionMap = {
  'mark_measure': testMarkMeasure,
  'resource_timing': testResourceTiming,
  'element_timing': testElementTiming,
  'long_task_task_attribution': testLongTask,
};

function runNavigationIdTest(params, description) {
  const defaultParams = {
    openFunc: url => window.open(url, '_blank', 'noopener'),
    scripts: [],
    funcBeforeNavigation: () => { },
    targetOrigin: originCrossSite,
    navigationTimes: 4,
    funcAfterAssertion: () => { },
  }  // Apply defaults.
  params = { ...defaultParams, ...params };

  promise_test(async t => {
    const pageA = new RemoteContext(token());
    const pageB = new RemoteContext(token());

    const urlA = executorPath + pageA.context_id;
    const urlB = params.targetOrigin + executorPath + pageB.context_id;
    // Open url A.
    params.openFunc(urlA);
    await pageA.execute_script(waitForPageShow);

    // Assert navigation id is 1 when the document is loaded first time.
    let navigationIds = await pageA.execute_script(testInitial);
    assert_true(
      navigationIds.every(t => t === 1), 'All Navigation Ids should be 1.');

    for (i = 1; i <= params.navigationTimes; i++) {
      // Navigate away to url B and back.
      await navigateAndThenBack(pageA, pageB, urlB);

      // Assert navigation id increments when the document is load from bfcache.
      navigationIds = await pageA.execute_script(
        testFunctionMap[params.testName], [i + 1]);
      assert_true(
        navigationIds.every(t => t === (i + 1)),
        params.testName + ' Navigation Id should all be ' + (i + 1) + '.');
    }
  }, description);
}
