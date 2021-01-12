/* global btoa fetch token promise_test step_timeout */
/* global assert_equals assert_true assert_own_property assert_throws_js assert_less_than */

const templates = {
  'fresh': {
    'response_headers': [
      ['Expires', 100000],
      ['Last-Modified', 0]
    ]
  },
  'stale': {
    'response_headers': [
      ['Expires', -5000],
      ['Last-Modified', -100000]
    ]
  },
  'lcl_response': {
    'response_headers': [
      ['Location', 'location_target'],
      ['Content-Location', 'content_location_target']
    ]
  },
  'location': {
    'query_arg': 'location_target',
    'response_headers': [
      ['Expires', 100000],
      ['Last-Modified', 0]
    ]
  },
  'content_location': {
    'query_arg': 'content_location_target',
    'response_headers': [
      ['Expires', 100000],
      ['Last-Modified', 0]
    ]
  }
}

const noBodyStatus = new Set([204, 304])

function makeTest (test) {
  return function () {
    var uuid = token()
    var requests = expandTemplates(test)
    var fetchFunctions = makeFetchFunctions(requests, uuid)
    return runTest(fetchFunctions, requests, uuid)
  }
}

function makeFetchFunctions(requests, uuid) {
    var fetchFunctions = []
    for (let i = 0; i < requests.length; ++i) {
      fetchFunctions.push({
        code: function (idx) {
          var config = requests[idx]
          var url = makeTestUrl(uuid, config)
          var init = fetchInit(requests, config)
          return fetch(url, init)
            .then(makeCheckResponse(idx, config))
            .then(makeCheckResponseBody(config, uuid), function (reason) {
              if ('expected_type' in config && config.expected_type === 'error') {
                assert_throws_js(TypeError, function () { throw reason })
              } else {
                throw reason
              }
            })
        },
        pauseAfter: 'pause_after' in requests[i]
      })
    }
    return fetchFunctions
}

function runTest(fetchFunctions, requests, uuid) {
    var idx = 0
    function runNextStep () {
      if (fetchFunctions.length) {
        var nextFetchFunction = fetchFunctions.shift()
        if (nextFetchFunction.pauseAfter === true) {
          return nextFetchFunction.code(idx++)
            .then(pause)
            .then(runNextStep)
        } else {
          return nextFetchFunction.code(idx++)
            .then(runNextStep)
        }
      } else {
        return Promise.resolve()
      }
    }

    return runNextStep()
      .then(function () {
        return getServerState(uuid)
      }).then(function (testState) {
        checkRequests(requests, testState)
        return Promise.resolve()
      })
}

function expandTemplates (test) {
  var rawRequests = test.requests
  var requests = []
  for (let i = 0; i < rawRequests.length; i++) {
    var request = rawRequests[i]
    request.name = test.name
    if ('template' in request) {
      var template = templates[request['template']]
      for (let member in template) {
        if (!request.hasOwnProperty(member)) {
          request[member] = template[member]
        }
      }
    }
    requests.push(request)
  }
  return requests
}

function fetchInit (requests, config) {
  var init = {
    'headers': []
  }
  if ('request_method' in config) init.method = config['request_method']
  if ('request_headers' in config) init.headers = config['request_headers']
  if ('name' in config) init.headers.push(['Test-Name', config.name])
  if ('request_body' in config) init.body = config['request_body']
  if ('mode' in config) init.mode = config['mode']
  if ('credentials' in config) init.mode = config['credentials']
  if ('cache' in config) init.cache = config['cache']
  init.headers.push(['Test-Requests', btoa(JSON.stringify(requests))])
  return init
}

function makeCheckResponse (idx, config) {
  return function checkResponse (response) {
    var reqNum = idx + 1
    var resNum = parseInt(response.headers.get('Server-Request-Count'))
    if ('expected_type' in config) {
      if (config.expected_type === 'error') {
        assert_true(false, `Request ${reqNum} doesn't throw an error`)
        return response.text()
      }
      if (config.expected_type === 'cached') {
        assert_less_than(resNum, reqNum, `Response ${reqNum} does not come from cache`)
      }
      if (config.expected_type === 'not_cached') {
        assert_equals(resNum, reqNum, `Response ${reqNum} comes from cache`)
      }
    }
    if ('expected_status' in config) {
      assert_equals(response.status, config.expected_status,
        `Response ${reqNum} status is ${response.status}, not ${config.expected_status}`)
    } else if ('response_status' in config) {
      assert_equals(response.status, config.response_status[0],
        `Response ${reqNum} status is ${response.status}, not ${config.response_status[0]}`)
    } else {
      assert_equals(response.status, 200, `Response ${reqNum} status is ${response.status}, not 200`)
    }
    if ('response_headers' in config) {
      config.response_headers.forEach(function (header) {
        if (header.len < 3 || header[2] === true) {
          assert_equals(response.headers.get(header[0]), header[1],
            `Response ${reqNum} header ${header[0]} is "${response.headers.get(header[0])}", not "${header[1]}"`)
        }
      })
    }
    if ('expected_response_headers' in config) {
      config.expected_response_headers.forEach(function (header) {
        assert_equals(response.headers.get(header[0]), header[1],
          `Response ${reqNum} header ${header[0]} is "${response.headers.get(header[0])}", not "${header[1]}"`)
      })
    }
    return response.text()
  }
}

function makeCheckResponseBody (config, uuid) {
  return function checkResponseBody (resBody) {
    var statusCode = 200
    if ('response_status' in config) {
      statusCode = config.response_status[0]
    }
    if ('expected_response_text' in config) {
      if (config.expected_response_text !== null) {
        assert_equals(resBody, config.expected_response_text,
          `Response body is "${resBody}", not expected "${config.expected_response_text}"`)
      }
    } else if ('response_body' in config && config.response_body !== null) {
      assert_equals(resBody, config.response_body,
        `Response body is "${resBody}", not sent "${config.response_body}"`)
    } else if (!noBodyStatus.has(statusCode)) {
      assert_equals(resBody, uuid, `Response body is "${resBody}", not default "${uuid}"`)
    }
  }
}

function checkRequests (requests, testState) {
  var testIdx = 0
  for (let i = 0; i < requests.length; ++i) {
    var expectedValidatingHeaders = []
    var config = requests[i]
    var serverRequest = testState[testIdx]
    var reqNum = i + 1
    if ('expected_type' in config) {
      if (config.expected_type === 'cached') continue // the server will not see the request
      if (config.expected_type === 'etag_validated') {
        expectedValidatingHeaders.push('if-none-match')
      }
      if (config.expected_type === 'lm_validated') {
        expectedValidatingHeaders.push('if-modified-since')
      }
    }
    testIdx++
    expectedValidatingHeaders.forEach(vhdr => {
      assert_own_property(serverRequest.request_headers, vhdr,
        `request ${reqNum} doesn't have ${vhdr} header`)
    })
    if ('expected_request_headers' in config) {
      config.expected_request_headers.forEach(expectedHdr => {
        assert_equals(serverRequest.request_headers[expectedHdr[0].toLowerCase()], expectedHdr[1],
          `request ${reqNum} header ${expectedHdr[0]} value is "${serverRequest.request_headers[expectedHdr[0].toLowerCase()]}", not "${expectedHdr[1]}"`)
      })
    }
  }
}

function pause () {
  return new Promise(function (resolve, reject) {
    step_timeout(function () {
      return resolve()
    }, 3000)
  })
}

function makeTestUrl (uuid, config) {
  var arg = ''
  var base_url = ''
  if ('base_url' in config) {
    base_url = config.base_url
  }
  if ('query_arg' in config) {
    arg = `&target=${config.query_arg}`
  }
  return `${base_url}resources/http-cache.py?dispatch=test&uuid=${uuid}${arg}`
}

function getServerState (uuid) {
  return fetch(`resources/http-cache.py?dispatch=state&uuid=${uuid}`)
    .then(function (response) {
      return response.text()
    }).then(function (text) {
      return JSON.parse(text) || []
    })
}

function run_tests (tests) {
  tests.forEach(function (test) {
    promise_test(makeTest(test), test.name)
  })
}

var contentStore = {}
function http_content (csKey) {
  if (csKey in contentStore) {
    return contentStore[csKey]
  } else {
    var content = btoa(Math.random() * Date.now())
    contentStore[csKey] = content
    return content
  }
}
