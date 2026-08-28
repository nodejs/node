// META: global=worker
async_test(t => {
  var target = self;
  target.addEventListener('custom-event', t.step_func_done());

  var event = new Event('custom-event');
  target.dispatchEvent(event);
}, 'Test CustomEvents on workers.');
