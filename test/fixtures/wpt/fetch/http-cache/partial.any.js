// META: global=window,worker
// META: title=HTTP Cache - Partial Content
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  {
    name: "HTTP cache stores partial content and reuses it",
    requests: [
      {
        request_headers: [
          ['Range', "bytes=-5"]
        ],
        response_status: [206, "Partial Content"],
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Content-Range", "bytes 4-9/10"]
        ],
        response_body: "01234",
        expected_request_headers: [
          ["Range", "bytes=-5"]
        ]
      },
      {
        request_headers: [
          ["Range", "bytes=-5"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "01234"
      }
    ]
  },
  {
    name: "HTTP cache stores complete response and serves smaller ranges from it (byte-range-spec)",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"]
        ],
        response_body: "01234567890"
      },
      {
        request_headers: [
          ['Range', "bytes=0-1"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "01"
      },
    ]
  },
  {
    name: "HTTP cache stores complete response and serves smaller ranges from it (absent last-byte-pos)",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
        ],
        response_body: "01234567890"
      },
      {
        request_headers: [
          ['Range', "bytes=1-"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "1234567890"
      }
    ]
  },
  {
    name: "HTTP cache stores complete response and serves smaller ranges from it (suffix-byte-range-spec)",
    requests: [
      {
        response_headers: [
          ["Cache-Control", "max-age=3600"],
        ],
        response_body: "0123456789A"
      },
      {
        request_headers: [
          ['Range', "bytes=-1"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "A"
      }
    ]
  },
  {
    name: "HTTP cache stores partial response and serves smaller ranges from it (byte-range-spec)",
    requests: [
      {
        request_headers: [
          ['Range', "bytes=-5"]
        ],
        response_status: [206, "Partial Content"],
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Content-Range", "bytes 4-9/10"]
        ],
        response_body: "01234"
      },
      {
        request_headers: [
          ['Range', "bytes=6-8"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "234"
      }
    ]
  },
  {
    name: "HTTP cache stores partial response and serves smaller ranges from it (absent last-byte-pos)",
    requests: [
      {
        request_headers: [
          ['Range', "bytes=-5"]
        ],
        response_status: [206, "Partial Content"],
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Content-Range", "bytes 4-9/10"]
        ],
        response_body: "01234"
      },
      {
        request_headers: [
          ["Range", "bytes=6-"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "234"
      }
    ]
  },
  {
    name: "HTTP cache stores partial response and serves smaller ranges from it (suffix-byte-range-spec)",
    requests: [
      {
        request_headers: [
          ['Range', "bytes=-5"]
        ],
        response_status: [206, "Partial Content"],
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Content-Range", "bytes 4-9/10"]
        ],
        response_body: "01234"
      },
      {
        request_headers: [
          ['Range', "bytes=-1"]
        ],
        expected_type: "cached",
        expected_status: 206,
        expected_response_text: "4"
      }
    ]
  },
  {
    name: "HTTP cache stores partial content and completes it",
    requests: [
      {
        request_headers: [
          ['Range', "bytes=-5"]
        ],
        response_status: [206, "Partial Content"],
        response_headers: [
          ["Cache-Control", "max-age=3600"],
          ["Content-Range", "bytes 0-4/10"]
        ],
        response_body: "01234"
      },
      {
        expected_request_headers: [
          ["range", "bytes=5-"]
        ]
      }
    ]
  },
];
run_tests(tests);
