// META: global=window,worker
// META: title=HTTP Cache - Freshness
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  // response directives
  {
    name: "HTTP cache reuses a response with a future Expires",
    requests: [
      {
        response_headers: [
          ["Expires", (30 * 24 * 60 * 60)]
        ]
      },
      {
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache does not reuse a response with a past Expires",
    requests: [
      {
        response_headers: [
          ["Expires", (-30 * 24 * 60 * 60)]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does not reuse a response with a present Expires",
    requests: [
      {
        response_headers: [
          ["Expires", 0]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does not reuse a response with an invalid Expires",
    requests: [
      {
        response_headers: [
          ["Expires", "0"]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache reuses a response with positive Cache-Control: max-age",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"]
        ]
      },
      {
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache does not reuse a response with Cache-Control: max-age=0",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=0"]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache reuses a response with positive Cache-Control: max-age and a past Expires",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Expires", -10000]
        ]
      },
      {
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache reuses a response with positive Cache-Control: max-age and an invalid Expires",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Expires", "0"]
        ]
      },
      {
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache does not reuse a response with Cache-Control: max-age=0 and a future Expires",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=0"],
          ["Expires", 10000]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does not prefer Cache-Control: s-maxage over Cache-Control: max-age",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=1, s-maxage=3600"]
        ],
        pause_after: true,
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does not reuse a response when the Age header is greater than its freshness lifetime",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Age", "12000"]
        ],
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does not store a response with Cache-Control: no-store",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "no-store"]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does not store a response with Cache-Control: no-store, even with max-age and Expires",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=10000, no-store"],
          ["Expires", 10000]
        ]
      },
      {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache stores a response with Cache-Control: no-cache, but revalidates upon use",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "no-cache"],
          ["ETag", "abcd"]
        ]
      },
      {
        expected_type: "etag_validated"
      }
    ]
  },
  {
    name: "HTTP cache stores a response with Cache-Control: no-cache, but revalidates upon use, even with max-age and Expires",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=10000, no-cache"],
          ["Expires", 10000],
          ["ETag", "abcd"]
        ]
      },
      {
        expected_type: "etag_validated"
      }
    ]
  },
];
run_tests(tests);
