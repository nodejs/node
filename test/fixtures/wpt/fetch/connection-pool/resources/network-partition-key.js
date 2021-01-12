// Runs multiple fetches that validate connections see only a single partition_id.
// Requests are run in parallel so that they use multiple connections to maximize the
// chance of exercising all matching connections in the connection pool. Only returns
// once all requests have completed to make cleaning up server state non-racy.
function check_partition_ids(location) {
  const NUM_FETCHES = 20;

  var base_url = 'SUBRESOURCE_PREFIX:&dispatch=check_partition';

  // Not a perfect parse of the query string, but good enough for this test.
  var include_credentials = base_url.search('include_credentials=true') != -1;
  var exclude_credentials = base_url.search('include_credentials=false') != -1;
  if (include_credentials != !exclude_credentials)
    throw new Exception('Credentials mode not specified');


  // Run NUM_FETCHES in parallel.
  var fetches = [];
  for (i = 0; i < NUM_FETCHES; ++i) {
    var fetch_params = {
      credentials: 'omit',
      mode: 'cors',
      headers: {
        'Header-To-Force-CORS': 'cors'
      },
    };

    // Use a unique URL for each request, in case the caching layer serializes multiple
    // requests for the same URL.
    var url = `${base_url}&${token()}`;

    fetches.push(fetch(url, fetch_params).then(
        function (response) {
          return response.text().then(function(text) {
            assert_equals(text, 'ok', `Socket unexpectedly reused`);
          });
        }));
  }

  // Wait for all promises to complete.
  return Promise.allSettled(fetches).then(function (results) {
    results.forEach(function (result) {
      if (result.status != 'fulfilled')
        throw result.reason;
    });
  });
}
