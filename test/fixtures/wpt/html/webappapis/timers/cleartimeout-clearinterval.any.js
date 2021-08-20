async_test((t) => {
  const handle = setTimeout(
    t.step_func(() => {
      assert_unreached("Timeout was not canceled");
    }),
    0
  );

  clearInterval(handle);

  setTimeout(() => {
    t.done();
  }, 100);
}, "Clear timeout with clearInterval");

async_test((t) => {
  const handle = setInterval(
    t.step_func(() => {
      assert_unreached("Interval was not canceled");
    }),
    0
  );

  clearTimeout(handle);

  setTimeout(() => {
    t.done();
  }, 100);
}, "Clear interval with clearTimeout");
