// This script is loaded in HTTP and HTTPS contexts to validate
// PerformanceResourceTiming entries' attributes when reusing connections.
//
// Note: to ensure that we reuse the connection to fetch multiple resources, we
// use the same XMLHttpRequest object throughout an individual test. Although
// it doesn't seem to be specified, each browser tested by WPT will reuse the
// underlying TCP connection with this approach. Pre-establishing the XHR's
// connection helps us to test connection reuse also in browsers that may key
// their connections on the related request's credentials mode.

const connection_reuse_test = (path, follow_on_assertions, test_label) => {
  const {on_200, on_304} = follow_on_assertions;

  // Make the first request before calling 'attribute_test' so that only the
  // second request's PerformanceResourceTiming entry will be interrogated. We
  // don't check the first request's PerformanceResourceTiming entry because
  // that's not what this test is trying to validate.
  const client = new XMLHttpRequest();
  const identifier = Math.random();
  path = `${path}?tag=${identifier}`;
  client.open("GET", path, false);
  client.send();

  attribute_test(
    async () => {
      client.open("GET", path + "&same_resource=false", false);
      client.send();

      // We expect to get a 200 Ok response because we've requested a different
      // resource than previous requests.
      if (client.status != 200) {
        throw new Error(`Got something other than a 200 response. ` +
                        `client.status: ${client.status}`);
      }
    }, path, entry => {
      invariants.assert_connection_reused(entry);
      on_200(entry);
    },
    `PerformanceResrouceTiming entries need to conform to the spec when a ` +
    `distinct resource is fetched over a persistent connection ` +
    `(${test_label})`);

  attribute_test(
    async () => {
      client.open("GET", path, false);
      client.setRequestHeader("If-None-Match", identifier);
      client.send();

      // We expect to get a 304 Not Modified response because we've used a
      // matching 'identifier' for the If-None-Match header.
      if (client.status != 304) {
        throw new Error(`Got something other than a 304 response. ` +
                        `client.status: ${client.status}, response: ` +
                        `'${client.responseText}'`);
      }
    }, path, entry => {
      invariants.assert_connection_reused(entry);
      on_304(entry);
    },
    `PerformanceResrouceTiming entries need to conform to the spec when the ` +
    `resource is cache-revalidated over a persistent connection ` +
    `(${test_label})`);
}
