function test_frame_timing_before_load_event(type) {
  promise_test(async t => {
    const {document, performance} = type === 'frame' ? window.parent : window;
    const delay = 500;
    const frame = document.createElement(type);
    t.add_cleanup(() => frame.remove());
    await new Promise(resolve => {
      frame.addEventListener('load', resolve);
      frame.src = `/resource-timing/resources/iframe-with-delay.sub.html?delay=${delay}`;
      (type === 'frame' ? document.querySelector('frameset') : document.body).appendChild(frame);
    });

    const entries = performance.getEntriesByName(frame.src);
    const navigationEntry = frame.contentWindow.performance.getEntriesByType('navigation')[0];
    assert_equals(entries.length, 1);
    assert_equals(entries[0].initiatorType, type);
    assert_greater_than(performance.now(), entries[0].responseEnd + delay);
    const domContentLoadedEventAbsoluteTime =
      navigationEntry.domContentLoadedEventStart +
      frame.contentWindow.performance.timeOrigin;
    const frameResponseEndAbsoluteTime = entries[0].responseEnd + performance.timeOrigin;
    assert_greater_than(domContentLoadedEventAbsoluteTime, frameResponseEndAbsoluteTime);
  }, `A ${type} should report its RT entry when the response is done and before it is completely loaded`);
}


function test_frame_timing_change_src(type,
                                      origin1 = document.origin,
                                      origin2 = document.origin,
                                      tao = false, label = '') {
  const uid = token();
  promise_test(async t => {
    const {document, performance} = type === 'frame' ? window.parent : window;
    const frame = document.createElement(type);
    t.add_cleanup(() => frame.remove());
    function createURL(origin) {
      const url = new URL(`${origin}/resource-timing/resources/green.html`, location.href);
      url.searchParams.set("uid", uid);
      if (tao)
        url.searchParams.set("pipe", "header(Timing-Allow-Origin, *)");
      return url.toString();
    }

    await new Promise(resolve => {
      const done = () => {
        resolve();
        frame.removeEventListener('load', done);
      }
      frame.addEventListener('load', done);
      frame.src = createURL(origin1);
      const root = type === 'frame' ? document.querySelector('frameset') : document.body;
      root.appendChild(frame);
    });

    await new Promise(resolve => {
      frame.addEventListener('load', resolve);
      frame.src = createURL(origin2);
    });

    const entries = performance.getEntries().filter(e => e.name.includes(uid));
    assert_equals(entries.length, 2);
  }, label || `A ${type} should report separate RT entries if its src changed from the outside`);
}
