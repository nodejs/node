wrk - a HTTP benchmarking tool

  wrk is a modern HTTP benchmarking tool capable of generating significant
  load when run on a single multi-core CPU. It combines a multithreaded
  design with scalable event notification systems such as epoll and kqueue.

  An optional LuaJIT script can perform HTTP request generation, response
  processing, and custom reporting. Several example scripts are located in
  scripts/

Basic Usage

  wrk -t12 -c400 -d30s http://127.0.0.1:8080/index.html

  This runs a benchmark for 30 seconds, using 12 threads, and keeping
  400 HTTP connections open.

  Output:

  Running 30s test @ http://127.0.0.1:8080/index.html
    12 threads and 400 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
      Latency   635.91us    0.89ms  12.92ms   93.69%
      Req/Sec    56.20k     8.07k   62.00k    86.54%
    22464657 requests in 30.00s, 17.76GB read
  Requests/sec: 748868.53
  Transfer/sec:    606.33MB

Scripting

  wrk's public Lua API is:

    init     = function(args)
    request  = function()
    response = function(status, headers, body)
    done     = function(summary, latency, requests)

    wrk = {
      scheme  = "http",
      host    = "localhost",
      port    = nil,
      method  = "GET",
      path    = "/",
      headers = {},
      body    = nil
    }

    function wrk.format(method, path, headers, body)

      wrk.format returns a HTTP request string containing the passed
      parameters merged with values from the wrk table.

    global init     -- function called when the thread is initialized
    global request  -- function returning the HTTP message for each request
    global response -- optional function called with HTTP response data
    global done     -- optional function called with results of run

  The init() function receives any extra command line arguments for the
  script. Script arguments must be separated from wrk arguments with "--"
  and scripts that override init() but not request() must call wrk.init()

  The done() function receives a table containing result data, and two
  statistics objects representing the sampled per-request latency and
  per-thread request rate. Duration and latency are microsecond values
  and rate is measured in requests per second.

    latency.min              -- minimum value seen
    latency.max              -- maximum value seen
    latency.mean             -- average value seen
    latency.stdev            -- standard deviation
    latency:percentile(99.0) -- 99th percentile value
    latency[i]               -- raw sample value

    summary = {
      duration = N,  -- run duration in microseconds
      requests = N,  -- total completed requests
      bytes    = N,  -- total bytes received
      errors   = {
        connect = N, -- total socket connection errors
        read    = N, -- total socket read errors
        write   = N, -- total socket write errors
        status  = N, -- total HTTP status codes > 399
        timeout = N  -- total request timeouts
      }
    }

Benchmarking Tips

  The machine running wrk must have a sufficient number of ephemeral ports
  available and closed sockets should be recycled quickly. To handle the
  initial connection burst the server's listen(2) backlog should be greater
  than the number of concurrent connections being tested.

  A user script that only changes the HTTP method, path, adds headers or
  a body, will have no performance impact. If multiple HTTP requests are
  necessary they should be pre-generated and returned via a quick lookup in
  the request() call. Per-request actions, particularly building a new HTTP
  request, and use of response() will necessarily reduce the amount of load
  that can be generated.

Acknowledgements

  wrk contains code from a number of open source projects including the
  'ae' event loop from redis, the nginx/joyent/node.js 'http-parser',
  Mike Pall's LuaJIT, and the Tiny Mersenne Twister PRNG. Please consult
  the NOTICE file for licensing details.
