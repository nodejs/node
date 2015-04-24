var assert = require('assert');

var HTTPParser = require('_http_parser');

var REQUEST = HTTPParser.REQUEST;

var cases = [
  {
    name: 'proxy request',
    url: 'http://hostname/'
  },
  {
    name: 'proxy request with port',
    url: 'http://hostname:444/'
  },
  {
    name: 'CONNECT request',
    url: 'hostname:443',
    method: 'CONNECT'
  },
  {
    name: 'CONNECT request but not connect',
    url: 'hostname:443',
    error: true
  },
  {
    name: 'proxy ipv6 request',
    url: 'http://[1:2::3:4]/'
  },
  {
    name: 'proxy ipv6 request with port',
    url: 'http://[1:2::3:4]:67/'
  },
  {
    name: 'CONNECT ipv6 address',
    url: '[1:2::3:4]:443',
    method: 'CONNECT'
  },
  {
    name: 'ipv4 in ipv6 address',
    url: 'http://[2001:0000:0000:0000:0000:0000:1.9.1.1]/'
  },
  {
    name: 'extra ? in query string',
    url: 'http://a.tbcdn.cn/p/fp/2010c/??fp-header-min.css,fp-base-min.css,' +
         'fp-channel-min.css,fp-product-min.css,fp-mall-min.css,' +
         'fp-category-min.css,fp-sub-min.css,fp-gdp4p-min.css,' +
         'fp-css3-min.css,fp-misc-min.css?t=20101022.css'
  },
  {
    name: 'space URL encoded',
    url: '/toto.html?toto=a%20b'
  },
  {
    name: 'URL fragment',
    url: '/toto.html#titi'
  },
  {
    name: 'complex URL fragment',
    url: 'http://www.webmasterworld.com/r.cgi?f=21&d=8405&url=' +
         'http://www.example.com/index.html?foo=bar&hello=world#midpage'
  },
  {
    name: 'complex URL from node js url parser doc',
    url: 'http://host.com:8080/p/a/t/h?query=string#hash'
  },
  {
    name: 'complex URL with basic auth from node js url parser doc',
    url: 'http://a:b@host.com:8080/p/a/t/h?query=string#hash'
  },
  {
    name: 'double @',
    url: 'http://a:b@@hostname:443/',
    error: true
  },
  {
    name: 'proxy empty host',
    url: 'http://:443/',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy empty port',
    url: 'http://hostname:/',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'CONNECT with basic auth',
    url: 'a:b@hostname:443',
    method: 'CONNECT',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'CONNECT empty host',
    url: ':443',
    method: 'CONNECT',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'CONNECT empty port',
    url: 'hostname:',
    method: 'CONNECT',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'CONNECT with extra bits',
    url: 'hostname:443/',
    method: 'CONNECT',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'space in URL',
    url: '/foo bar/',
    error: true
  },
  {
    name: 'proxy basic auth with space url encoded',
    url: 'http://a%20:b@host.com/'
  },
  {
    name: 'carriage return in URL',
    url: '/foo\rbar/',
    error: true
  },
  {
    name: 'proxy double : in URL',
    url: 'http://hostname::443/',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy basic auth with double :',
    url: 'http://a::b@host.com/'
  },
  {
    name: 'line feed in URL',
    url: '/foo\nbar/',
    error: true
  },
  {
    name: 'proxy empty basic auth',
    url: 'http://@hostname/fo'
  },
  {
    name: 'proxy line feed in hostname',
    url: 'http://host\name/fo',
    error: true
  },
  {
    name: 'proxy % in hostname',
    url: 'http://host%name/fo',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy ; in hostname',
    url: 'http://host;ame/fo',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy basic auth with unreservedchars',
    url: 'http://a!;-_!=+$@host.com/'
  },
  {
    name: 'proxy only empty basic auth',
    url: 'http://@/fo',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy only basic auth',
    url: 'http://toto@/fo',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy empty hostname',
    url: 'http:///fo',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'proxy = in URL',
    url: 'http://host=ame/fo',
    // Commented out because the JavaScript HTTP parser only performs basic
    // character validation and does not actually parse the URL into its
    // separate parts
    //error: true
  },
  {
    name: 'tab in URL',
    url: '/foo\tbar/',
    error: true
  },
  {
    name: 'form feed in URL',
    url: '/foo\fbar/',
    error: true
  },
];


// Prevent EE warnings since we have many test cases which attach `exit` event
// handlers
process.setMaxListeners(0);

// Test predefined urls
cases.forEach(function(testCase) {
  var parser = new HTTPParser(REQUEST);
  var method = (testCase.method || 'GET');
  var url = testCase.url;
  var input = new Buffer(method + ' ' + url + ' HTTP/1.0\r\n\r\n', 'binary');
  var sawHeaders = false;
  var completed = false;

  function onHeaders(versionMajor, versionMinor, headers, method_, url_,
                     statusCode, statusText, upgrade, shouldKeepAlive) {
    sawHeaders = true;
    assert.strictEqual(versionMajor, 1);
    assert.strictEqual(versionMinor, 0);
    assert.deepEqual(headers, []);
    assert.strictEqual(method_, method);
    assert.strictEqual(url_, url);
    assert.strictEqual(statusCode, null);
    assert.strictEqual(statusText, null);
    assert.strictEqual(upgrade, method === 'CONNECT');
    assert.strictEqual(shouldKeepAlive, false);
  }

  function onBody(data, offset, len) {
    assert('Unexpected body');
  }

  function onComplete() {
    assert.strictEqual(sawHeaders, true);
    completed = true;
  }

  parser.onHeaders = onHeaders;
  parser.onBody = onBody;
  parser.onComplete = onComplete;

  process.on('exit', function() {
    assert.strictEqual(completed,
                       true,
                       'Parsing did not complete for: ' + testCase.name);
  });

  var ret;
  try {
    ret = parser.execute(input);
    parser.finish();
  } catch (ex) {
    throw new Error('Unexpected error thrown for: ' + testCase.name + ':\n\n' +
                    ex.stack + '\n');
  }
  if (testCase.error !== undefined && typeof ret === 'number')
    throw new Error('Expected error for: ' + testCase.name);
  else if (testCase.error === undefined && typeof ret !== 'number') {
    throw new Error('Unexpected error for: ' + testCase.name + ':\n\n' +
                    ret.stack + '\n');
  }
  if (testCase.error !== undefined) {
    completed = true; // Prevent error from throwing on script exit
    return;
  }
});
