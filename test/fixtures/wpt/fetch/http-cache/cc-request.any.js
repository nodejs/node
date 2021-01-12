// META: global=window,worker
// META: title=HTTP Cache - Cache-Control Request Directives
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  {
    name: "HTTP cache doesn't use aged but fresh response when request contains Cache-Control: max-age=0",
    requests: [
      {
        template: "fresh",
        pause_after: true
      },
      {
        request_headers: [
          ["Cache-Control", "max-age=0"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache doesn't use aged but fresh response when request contains Cache-Control: max-age=1",
    requests: [
      {
        template: "fresh",
        pause_after: true
      },
      {
        request_headers: [
          ["Cache-Control", "max-age=1"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache doesn't use fresh response with Age header when request contains Cache-Control: max-age that is greater than remaining freshness",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Age", "1800"]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "max-age=600"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache does use aged stale response when request contains Cache-Control: max-stale that permits its use",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=1"]
        ],
        pause_after: true
      },
      {
        request_headers: [
          ["Cache-Control", "max-stale=1000"]
        ],
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache does reuse stale response with Age header when request contains Cache-Control: max-stale that permits its use",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=1500"],
          ["Age", "2000"]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "max-stale=1000"]
        ],
        expected_type: "cached"
      }
    ]
  },
  {
    name: "HTTP cache doesn't reuse fresh response when request contains Cache-Control: min-fresh that wants it fresher",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=1500"]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "min-fresh=2000"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache doesn't reuse fresh response with Age header when request contains Cache-Control: min-fresh that wants it fresher",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=1500"],
          ["Age", "1000"]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "min-fresh=1000"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache doesn't reuse fresh response when request contains Cache-Control: no-cache",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "no-cache"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache validates fresh response with Last-Modified when request contains Cache-Control: no-cache",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Last-Modified", -10000]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "no-cache"]
        ],
        expected_type: "lm_validate"
      }
    ]
  },
  {
    name: "HTTP cache validates fresh response with ETag when request contains Cache-Control: no-cache",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["ETag", http_content("abc")]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "no-cache"]
        ],
        expected_type: "etag_validate"
      }
    ]
  },
  {
    name: "HTTP cache doesn't reuse fresh response when request contains Cache-Control: no-store",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"]
        ]
      },
      {
        request_headers: [
          ["Cache-Control", "no-store"]
        ],
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: "HTTP cache generates 504 status code when nothing is in cache and request contains Cache-Control: only-if-cached",
    requests: [
      {
        request_headers: [
          ["Cache-Control", "only-if-cached"]
        ],
        expected_status: 504,
        expected_response_text: null
      }
    ]
  }
];
run_tests(tests);
