function requestFromBody(body) {
  return new Request(
    "https://example.com",
    {
      method: "POST",
      body,
      duplex: "half",
    },
  );
}

test(() => {
  const request = requestFromBody(undefined);
  assert_equals(request.headers.get("Content-Type"), null);
}, "Default Content-Type for Request with empty body");

test(() => {
  const blob = new Blob([]);
  const request = requestFromBody(blob);
  assert_equals(request.headers.get("Content-Type"), null);
}, "Default Content-Type for Request with Blob body (no type set)");

test(() => {
  const blob = new Blob([], { type: "" });
  const request = requestFromBody(blob);
  assert_equals(request.headers.get("Content-Type"), null);
}, "Default Content-Type for Request with Blob body (empty type)");

test(() => {
  const blob = new Blob([], { type: "a/b; c=d" });
  const request = requestFromBody(blob);
  assert_equals(request.headers.get("Content-Type"), "a/b; c=d");
}, "Default Content-Type for Request with Blob body (set type)");

test(() => {
  const buffer = new Uint8Array();
  const request = requestFromBody(buffer);
  assert_equals(request.headers.get("Content-Type"), null);
}, "Default Content-Type for Request with buffer source body");

promise_test(async () => {
  const formData = new FormData();
  formData.append("a", "b");
  const request = requestFromBody(formData);
  const boundary = (await request.text()).split("\r\n")[0].slice(2);
  assert_equals(
    request.headers.get("Content-Type"),
    `multipart/form-data; boundary=${boundary}`,
  );
}, "Default Content-Type for Request with FormData body");

test(() => {
  const usp = new URLSearchParams();
  const request = requestFromBody(usp);
  assert_equals(
    request.headers.get("Content-Type"),
    "application/x-www-form-urlencoded;charset=UTF-8",
  );
}, "Default Content-Type for Request with URLSearchParams body");

test(() => {
  const request = requestFromBody("");
  assert_equals(
    request.headers.get("Content-Type"),
    "text/plain;charset=UTF-8",
  );
}, "Default Content-Type for Request with string body");

test(() => {
  const stream = new ReadableStream();
  const request = requestFromBody(stream);
  assert_equals(request.headers.get("Content-Type"), null);
}, "Default Content-Type for Request with ReadableStream body");

// -----------------------------------------------------------------------------

const OVERRIDE_MIME = "test/only; mime=type";

function requestFromBodyWithOverrideMime(body) {
  return new Request(
    "https://example.com",
    {
      method: "POST",
      body,
      headers: { "Content-Type": OVERRIDE_MIME },
      duplex: "half",
    },
  );
}

test(() => {
  const request = requestFromBodyWithOverrideMime(undefined);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with empty body");

test(() => {
  const blob = new Blob([]);
  const request = requestFromBodyWithOverrideMime(blob);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with Blob body (no type set)");

test(() => {
  const blob = new Blob([], { type: "" });
  const request = requestFromBodyWithOverrideMime(blob);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with Blob body (empty type)");

test(() => {
  const blob = new Blob([], { type: "a/b; c=d" });
  const request = requestFromBodyWithOverrideMime(blob);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with Blob body (set type)");

test(() => {
  const buffer = new Uint8Array();
  const request = requestFromBodyWithOverrideMime(buffer);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with buffer source body");

test(() => {
  const formData = new FormData();
  const request = requestFromBodyWithOverrideMime(formData);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with FormData body");

test(() => {
  const usp = new URLSearchParams();
  const request = requestFromBodyWithOverrideMime(usp);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with URLSearchParams body");

test(() => {
  const request = requestFromBodyWithOverrideMime("");
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with string body");

test(() => {
  const stream = new ReadableStream();
  const request = requestFromBodyWithOverrideMime(stream);
  assert_equals(request.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Request with ReadableStream body");
