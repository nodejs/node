// META: global=window,worker
// META: script=../resources/utils.js
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js

promise_test(async (test) => {
  const resp = await fetch(
    "/fetch/connection-pool/resources/network-partition-key.py?"
    + `status=425&uuid=${token()}&partition_id=${get_host_info().ORIGIN}`
    + `&dispatch=check_partition&addcounter=true`);
  assert_equals(resp.status, 425);
  const text = await resp.text();
  assert_equals(text, "ok. Request was sent 1 times. 1 connections were created.");
}, "Fetch on 425 response should not be retried for non TLS early data.");
