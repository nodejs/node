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

[
  () => new Request("about:blank", { method: "POST" }),
  () => new Response(),
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, "");
  }, `${bodyContainer.constructor.name}: MIME type for Blob from empty body`);
});

[
  () => new Request("about:blank", { method: "POST", headers: [["Content-Type", "Mytext/Plain"]] }),
  () => new Response("", { headers: [["Content-Type", "Mytext/Plain"]] })
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, 'mytext/plain');
  }, `${bodyContainer.constructor.name}: MIME type for Blob from empty body with Content-Type`);
});

[
  () => new Request("about:blank", { body: new Blob([""]), method: "POST" }),
  () => new Response(new Blob([""]))
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, "");
    assert_equals(bodyContainer.headers.get("Content-Type"), null);
  }, `${bodyContainer.constructor.name}: MIME type for Blob`);
});

[
  () => new Request("about:blank", { body: new Blob([""], { type: "Text/Plain" }), method: "POST" }),
  () => new Response(new Blob([""], { type: "Text/Plain" }))
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, "text/plain");
    assert_equals(bodyContainer.headers.get("Content-Type"), "text/plain");
  }, `${bodyContainer.constructor.name}: MIME type for Blob with non-empty type`);
});

[
  () => new Request("about:blank", { method: "POST", body: new Blob([""], { type: "Text/Plain" }), headers: [["Content-Type", "Text/Html"]] }),
  () => new Response(new Blob([""], { type: "Text/Plain" }), { headers: [["Content-Type", "Text/Html"]] })
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  const cloned = bodyContainer.clone();
  promise_test(async t => {
    const blobs = [await bodyContainer.blob(), await cloned.blob()];
    assert_equals(blobs[0].type, "text/html");
    assert_equals(blobs[1].type, "text/html");
    assert_equals(bodyContainer.headers.get("Content-Type"), "Text/Html");
    assert_equals(cloned.headers.get("Content-Type"), "Text/Html");
  }, `${bodyContainer.constructor.name}: Extract a MIME type with clone`);
});

[
  () => new Request("about:blank", { body: new Blob([], { type: "text/plain" }), method: "POST", headers: [["Content-Type", "text/html"]] }),
  () => new Response(new Blob([], { type: "text/plain" }), { headers: [["Content-Type", "text/html"]] }),
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    assert_equals(bodyContainer.headers.get("Content-Type"), "text/html");
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, "text/html");
  }, `${bodyContainer.constructor.name}: Content-Type in headers wins Blob"s type`);
});

[
  () => new Request("about:blank", { body: new Blob([], { type: "text/plain" }), method: "POST" }),
  () => new Response(new Blob([], { type: "text/plain" })),
].forEach(bodyContainerCreator => {
  const bodyContainer = bodyContainerCreator();
  promise_test(async t => {
    assert_equals(bodyContainer.headers.get("Content-Type"), "text/plain");
    const newMIMEType = "text/html";
    bodyContainer.headers.set("Content-Type", newMIMEType);
    const blob = await bodyContainer.blob();
    assert_equals(blob.type, newMIMEType);
  }, `${bodyContainer.constructor.name}: setting missing Content-Type in headers and it wins Blob"s type`);
});
