// META: title=FileAPI Test: filereader_readAsBinaryString

async_test(t => {
  const blob = new Blob(["σ"]);
  const reader = new FileReader();

  reader.onload = t.step_func_done(() => {
    assert_equals(typeof reader.result, "string", "The result is string");
    assert_equals(reader.result.length, 2, "The result length is 2");
    assert_equals(reader.result, "\xcf\x83", "The result is \xcf\x83");
    assert_equals(reader.readyState, reader.DONE);
  });

  reader.onloadstart = t.step_func(() => {
    assert_equals(reader.readyState, reader.LOADING);
  });

  reader.onprogress = t.step_func(() => {
    assert_equals(reader.readyState, reader.LOADING);
  });

  reader.readAsBinaryString(blob);
});
