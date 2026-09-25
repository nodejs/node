// META: global=sharedworker
// https://crbug.com/239669
const t = async_test("Tests that repeatedly setting 'onerror' within a shared worker doesnt crash.");
onconnect = t.step_func_done((event) => {
  function update() {
    onerror = undefined;
  }
  try {
    for (var i = 0; i < 8; ++i) {
      update();
    }
  } catch (ex) {
    assert_unreached("FAIL: unexpected exception (" + ex + ") received while updating onerror event handler.");
  }
});
