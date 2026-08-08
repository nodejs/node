importScripts("/resources/testharness.js");

async_test(t => {
  const res = [];
  requestAnimationFrame(t.step_func(dt => {
    res.push(dt);
    requestAnimationFrame(t.step_func(dt => {
      res.push(dt);
      requestAnimationFrame(t.step_func_done(dt => {
        res.push(dt);
        assert_equals(res.length, 3);
        assert_less_than(res[0], res[1]);
        assert_less_than(res[1], res[2]);
      }));
    }));
  }));
});

done();
