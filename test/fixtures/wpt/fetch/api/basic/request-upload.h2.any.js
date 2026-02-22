// META: global=window,worker
// META: script=../resources/utils.js
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js

const duplex = "half";

async function assertUpload(url, method, createBody, expectedBody) {
  const requestInit = {method};
  const body = createBody();
  if (body) {
    requestInit["body"] = body;
    requestInit.duplex = "half";
  }
  const resp = await fetch(url, requestInit);
  const text = await resp.text();
  assert_equals(text, expectedBody);
}

function testUpload(desc, url, method, createBody, expectedBody) {
  promise_test(async () => {
    await assertUpload(url, method, createBody, expectedBody);
  }, desc);
}

function createStream(chunks) {
  return new ReadableStream({
    start: (controller) => {
      for (const chunk of chunks) {
        controller.enqueue(chunk);
      }
      controller.close();
    }
  });
}

const url = RESOURCES_DIR + "echo-content.h2.py"

testUpload("Fetch with POST with empty ReadableStream", url,
  "POST",
  () => {
    return new ReadableStream({start: controller => {
      controller.close();
    }})
  },
  "");

testUpload("Fetch with POST with ReadableStream", url,
  "POST",
  () => {
    return new ReadableStream({start: controller => {
      const encoder = new TextEncoder();
      controller.enqueue(encoder.encode("Test"));
      controller.close();
    }})
  },
  "Test");

promise_test(async (test) => {
  const body = new ReadableStream({start: controller => {
    const encoder = new TextEncoder();
    controller.enqueue(encoder.encode("Test"));
    controller.close();
  }});
  const resp = await fetch(
    "/fetch/connection-pool/resources/network-partition-key.py?"
    + `status=421&uuid=${token()}&partition_id=${self.origin}`
    + `&dispatch=check_partition&addcounter=true`,
    {method: "POST", body: body, duplex});
  assert_equals(resp.status, 421);
  const text = await resp.text();
  assert_equals(text, "ok. Request was sent 1 times. 1 connections were created.");
}, "Fetch with POST with ReadableStream on 421 response should return the response and not retry.");

promise_test(async (test) => {
  const request = new Request('', {
    body: new ReadableStream(),
    method: 'POST',
    duplex,
  });

  assert_equals(request.headers.get('Content-Type'), null, `Request should not have a content-type set`);

  const response = await fetch('data:a/a;charset=utf-8,test', {
    method: 'POST',
    body: new ReadableStream(),
    duplex,
  });

  assert_equals(await response.text(), 'test', `Response has correct body`);
}, "Feature detect for POST with ReadableStream");

promise_test(async (test) => {
  const request = new Request('data:a/a;charset=utf-8,test', {
    body: new ReadableStream(),
    method: 'POST',
    duplex,
 });

  assert_equals(request.headers.get('Content-Type'), null, `Request should not have a content-type set`);
  const response = await fetch(request);
  assert_equals(await response.text(), 'test', `Response has correct body`);
}, "Feature detect for POST with ReadableStream, using request object");

test(() => {
  let duplexAccessed = false;

  const request = new Request("", {
    body: new ReadableStream(),
    method: "POST",
    get duplex() {
      duplexAccessed = true;
      return "half";
    },
  });

  assert_equals(
    request.headers.get("Content-Type"),
    null,
    `Request should not have a content-type set`
  );
  assert_true(duplexAccessed, `duplex dictionary property should be accessed`);
}, "Synchronous feature detect");

// The asserts the synchronousFeatureDetect isn't broken by a partial implementation.
// An earlier feature detect was broken by Safari implementing streaming bodies as part of Request,
// but it failed when passed to fetch().
// This tests ensures that UAs must not implement RequestInit.duplex and streaming request bodies without also implementing the fetch() parts.
promise_test(async () => {
  let duplexAccessed = false;

  const request = new Request("", {
    body: new ReadableStream(),
    method: "POST",
    get duplex() {
      duplexAccessed = true;
      return "half";
    },
  });

  const supported =
    request.headers.get("Content-Type") === null && duplexAccessed;

  // If the feature detect fails, assume the browser is being truthful (other tests pick up broken cases here)
  if (!supported) return false;

  await assertUpload(
    url,
    "POST",
    () =>
      new ReadableStream({
        start: (controller) => {
          const encoder = new TextEncoder();
          controller.enqueue(encoder.encode("Test"));
          controller.close();
        },
      }),
    "Test"
  );
}, "Synchronous feature detect fails if feature unsupported");

promise_test(async (t) => {
  const body = createStream(["hello"]);
  const method = "POST";
  await promise_rejects_js(t, TypeError, fetch(url, { method, body, duplex }));
}, "Streaming upload with body containing a String");

promise_test(async (t) => {
  const body = createStream([null]);
  const method = "POST";
  await promise_rejects_js(t, TypeError, fetch(url, { method, body, duplex }));
}, "Streaming upload with body containing null");

promise_test(async (t) => {
  const body = createStream([33]);
  const method = "POST";
  await promise_rejects_js(t, TypeError, fetch(url, { method, body, duplex }));
}, "Streaming upload with body containing a number");

promise_test(async (t) => {
  const url = "/fetch/api/resources/authentication.py?realm=test";
  const body = createStream([]);
  const method = "POST";
  await promise_rejects_js(t, TypeError, fetch(url, { method, body, duplex }));
}, "Streaming upload should fail on a 401 response");

promise_test(async (t) => {
  const abortMessage = 'foo abort';
  let streamCancelPromise = new Promise(async res => {
    var stream = new ReadableStream({
      cancel: function(reason) {
        res(reason);
      }
    });
    let abortController = new AbortController();
    let fetchPromise = promise_rejects_exactly(t, abortMessage, fetch('', {
                                                 method: 'POST',
                                                 body: stream,
                                                 duplex: 'half',
                                                 signal: abortController.signal
                                               }));
    abortController.abort(abortMessage);
    await fetchPromise;
  });

  let cancelReason = await streamCancelPromise;
  assert_equals(
      cancelReason, abortMessage, 'ReadableStream.cancel should be called.');
}, 'ReadbleStream should be closed on signal.abort');
