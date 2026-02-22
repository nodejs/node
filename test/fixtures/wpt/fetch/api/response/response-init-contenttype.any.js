test(() => {
  const response = new Response();
  assert_equals(response.headers.get("Content-Type"), null);
}, "Default Content-Type for Response with empty body");

test(() => {
  const blob = new Blob([]);
  const response = new Response(blob);
  assert_equals(response.headers.get("Content-Type"), null);
}, "Default Content-Type for Response with Blob body (no type set)");

test(() => {
  const blob = new Blob([], { type: "" });
  const response = new Response(blob);
  assert_equals(response.headers.get("Content-Type"), null);
}, "Default Content-Type for Response with Blob body (empty type)");

test(() => {
  const blob = new Blob([], { type: "a/b; c=d" });
  const response = new Response(blob);
  assert_equals(response.headers.get("Content-Type"), "a/b; c=d");
}, "Default Content-Type for Response with Blob body (set type)");

test(() => {
  const buffer = new Uint8Array();
  const response = new Response(buffer);
  assert_equals(response.headers.get("Content-Type"), null);
}, "Default Content-Type for Response with buffer source body");

promise_test(async () => {
  const formData = new FormData();
  formData.append("a", "b");
  const response = new Response(formData);
  const boundary = (await response.text()).split("\r\n")[0].slice(2);
  assert_equals(
    response.headers.get("Content-Type"),
    `multipart/form-data; boundary=${boundary}`,
  );
}, "Default Content-Type for Response with FormData body");

test(() => {
  const usp = new URLSearchParams();
  const response = new Response(usp);
  assert_equals(
    response.headers.get("Content-Type"),
    "application/x-www-form-urlencoded;charset=UTF-8",
  );
}, "Default Content-Type for Response with URLSearchParams body");

test(() => {
  const response = new Response("");
  assert_equals(
    response.headers.get("Content-Type"),
    "text/plain;charset=UTF-8",
  );
}, "Default Content-Type for Response with string body");

test(() => {
  const stream = new ReadableStream();
  const response = new Response(stream);
  assert_equals(response.headers.get("Content-Type"), null);
}, "Default Content-Type for Response with ReadableStream body");

// -----------------------------------------------------------------------------

const OVERRIDE_MIME = "test/only; mime=type";

function responseWithOverrideMime(body) {
  return new Response(
    body,
    { headers: { "Content-Type": OVERRIDE_MIME } },
  );
}

test(() => {
  const response = responseWithOverrideMime(undefined);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with empty body");

test(() => {
  const blob = new Blob([]);
  const response = responseWithOverrideMime(blob);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with Blob body (no type set)");

test(() => {
  const blob = new Blob([], { type: "" });
  const response = responseWithOverrideMime(blob);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with Blob body (empty type)");

test(() => {
  const blob = new Blob([], { type: "a/b; c=d" });
  const response = responseWithOverrideMime(blob);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with Blob body (set type)");

test(() => {
  const buffer = new Uint8Array();
  const response = responseWithOverrideMime(buffer);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with buffer source body");

test(() => {
  const formData = new FormData();
  const response = responseWithOverrideMime(formData);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with FormData body");

test(() => {
  const usp = new URLSearchParams();
  const response = responseWithOverrideMime(usp);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with URLSearchParams body");

test(() => {
  const response = responseWithOverrideMime("");
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with string body");

test(() => {
  const stream = new ReadableStream();
  const response = responseWithOverrideMime(stream);
  assert_equals(response.headers.get("Content-Type"), OVERRIDE_MIME);
}, "Can override Content-Type for Response with ReadableStream body");
