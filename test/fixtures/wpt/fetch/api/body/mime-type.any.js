[
 () => new Request("about:blank", { headers: { "Content-Type": "text/plain" } }),
 () => new Response("", { headers: { "Content-Type": "text/plain" } })
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    assert_equals(bodyContainer.headers.get("Content-Type"), "text/plain");
    const newMIMEType = "test/test";
    bodyContainer.headers.set("Content-Type", newMIMEType);
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, newMIMEType);
  }, `${bodyContainer.constructor.name}: overriding explicit Content-Type`);
});

[
  () => new Request("about:blank", { body: new URLSearchParams(), method: "POST" }),
  () => new Response(new URLSearchParams()),
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    assert_equals(bodyContainer.headers.get("Content-Type"), "application/x-www-form-urlencoded;charset=UTF-8");
    bodyContainer.headers.delete("Content-Type");
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, "");
  }, `${bodyContainer.constructor.name}: removing implicit Content-Type`);
});

[
  () => new Request("about:blank", { body: new ArrayBuffer(), method: "POST" }),
  () => new Response(new ArrayBuffer()),
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    assert_equals(bodyContainer.headers.get("Content-Type"), null);
    const newMIMEType = "test/test";
    bodyContainer.headers.set("Content-Type", newMIMEType);
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, newMIMEType);
  }, `${bodyContainer.constructor.name}: setting missing Content-Type`);
});
