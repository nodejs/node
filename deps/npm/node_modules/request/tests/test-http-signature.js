var createServer = require('http').createServer
  , request = require('../index')
  , httpSignature = require('http-signature')
  , assert = require('assert')
  ;

var privateKeyPEMs = {}

privateKeyPEMs['key-1'] =
  '-----BEGIN RSA PRIVATE KEY-----\n' +
  'MIIEpAIBAAKCAQEAzWSJl+Z9Bqv00FVL5N3+JCUoqmQPjIlya1BbeqQroNQ5yG1i\n' +
  'VbYTTnMRa1zQtR6r2fNvWeg94DvxivxIG9diDMnrzijAnYlTLOl84CK2vOxkj5b6\n' +
  '8zrLH9b/Gd6NOHsywo8IjvXvCeTfca5WUHcuVi2lT9VjygFs1ILG4RyeX1BXUumu\n' +
  'Y8fzmposxLYdMxCqUTzAn0u9Saq2H2OVj5u114wS7OQPigu6G99dpn/iPHa3zBm8\n' +
  '7baBWDbqZWRW0BP3K6eqq8sut1+NLhNW8ADPTdnO/SO+kvXy7fqd8atSn+HlQcx6\n' +
  'tW42dhXf3E9uE7K78eZtW0KvfyNGAjsI1Fft2QIDAQABAoIBAG1exe3/LEBrPLfb\n' +
  'U8iRdY0lxFvHYIhDgIwohC3wUdMYb5SMurpNdEZn+7Sh/fkUVgp/GKJViu1mvh52\n' +
  'bKd2r52DwG9NQBQjVgkqY/auRYSglIPpr8PpYNSZlcneunCDGeqEY9hMmXc5Ssqs\n' +
  'PQYoEKKPN+IlDTg6PguDgAfLR4IUvt9KXVvmB/SSgV9tSeTy35LECt1Lq3ozbUgu\n' +
  '30HZI3U6/7H+X22Pxxf8vzBtzkg5rRCLgv+OeNPo16xMnqbutt4TeqEkxRv5rtOo\n' +
  '/A1i9khBeki0OJAFJsE82qnaSZodaRsxic59VnN8sWBwEKAt87tEu5A3K3j4XSDU\n' +
  '/avZxAECgYEA+pS3DvpiQLtHlaO3nAH6MxHRrREOARXWRDe5nUQuUNpS1xq9wte6\n' +
  'DkFtba0UCvDLic08xvReTCbo9kH0y6zEy3zMpZuJlKbcWCkZf4S5miYPI0RTZtF8\n' +
  'yps6hWqzYFSiO9hMYws9k4OJLxX0x3sLK7iNZ32ujcSrkPBSiBr0gxkCgYEA0dWl\n' +
  '637K41AJ/zy0FP0syq+r4eIkfqv+/t6y2aQVUBvxJYrj9ci6XHBqoxpDV8lufVYj\n' +
  'fUAfeI9/MZaWvQJRbnYLre0I6PJfLuCBIL5eflO77BGso165AF7QJZ+fwtgKv3zv\n' +
  'ZX75eudCSS/cFo0po9hlbcLMT4B82zEkgT8E2MECgYEAnz+3/wrdOmpLGiyL2dff\n' +
  '3GjsqmJ2VfY8z+niSrI0BSpbD11tT9Ct67VlCBjA7hsOH6uRfpd6/kaUMzzDiFVq\n' +
  'VDAiFvV8QD6zNkwYalQ9aFvbrvwTTPrBpjl0vamMCiJ/YC0cjq1sGr2zh3sar1Ph\n' +
  'S43kP+s97dcZeelhaiJHVrECgYEAsx61q/loJ/LDFeYzs1cLTVn4V7I7hQY9fkOM\n' +
  'WM0AhInVqD6PqdfXfeFYpjJdGisQ7l0BnoGGW9vir+nkcyPvb2PFRIr6+B8tsU5j\n' +
  '7BeVgjDoUfQkcrEBK5fEBtnj/ud9BUkY8oMZZBjVNLRuI7IMwZiPvMp0rcj4zAN/\n' +
  'LfUlpgECgYArBvFcBxSkNAzR3Rtteud1YDboSKluRM37Ey5plrn4BS0DD0jm++aD\n' +
  '0pG2Hsik000hibw92lCkzvvBVAqF8BuAcnPlAeYfsOaa97PGEjSKEN5bJVWZ9/om\n' +
  '9FV1axotRN2XWlwrhixZLEaagkREXhgQc540FS5O8IaI2Vpa80Atzg==\n' +
  '-----END RSA PRIVATE KEY-----'

var publicKeyPEMs = {}

publicKeyPEMs['key-1'] =
  '-----BEGIN PUBLIC KEY-----\n' +
  'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzWSJl+Z9Bqv00FVL5N3+\n' +
  'JCUoqmQPjIlya1BbeqQroNQ5yG1iVbYTTnMRa1zQtR6r2fNvWeg94DvxivxIG9di\n' +
  'DMnrzijAnYlTLOl84CK2vOxkj5b68zrLH9b/Gd6NOHsywo8IjvXvCeTfca5WUHcu\n' +
  'Vi2lT9VjygFs1ILG4RyeX1BXUumuY8fzmposxLYdMxCqUTzAn0u9Saq2H2OVj5u1\n' +
  '14wS7OQPigu6G99dpn/iPHa3zBm87baBWDbqZWRW0BP3K6eqq8sut1+NLhNW8ADP\n' +
  'TdnO/SO+kvXy7fqd8atSn+HlQcx6tW42dhXf3E9uE7K78eZtW0KvfyNGAjsI1Fft\n' +
  '2QIDAQAB\n' +
  '-----END PUBLIC KEY-----'

publicKeyPEMs['key-2'] =
  '-----BEGIN PUBLIC KEY-----\n' +
  'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAqp04VVr9OThli9b35Omz\n' +
  'VqSfWbsoQuRrgyWsrNRn3XkFmbWw4FzZwQ42OgGMzQ84Ta4d9zGKKQyFriTiPjPf\n' +
  'xhhrsaJnDuybcpVhcr7UNKjSZ0S59tU3hpRiEz6hO+Nc/OSSLkvalG0VKrxOln7J\n' +
  'LK/h3rNS/l6wDZ5S/KqsI6CYtV2ZLpn3ahLrizvEYNY038Qcm38qMWx+VJAvZ4di\n' +
  'qqmW7RLIsLT59SWmpXdhFKnkYYGhxrk1Mwl22dBTJNY5SbriU5G3gWgzYkm8pgHr\n' +
  '6CtrXch9ciJAcDJehPrKXNvNDOdUh8EW3fekNJerF1lWcwQg44/12v8sDPyfbaKB\n' +
  'dQIDAQAB\n' +
  '-----END PUBLIC KEY-----'

var server = createServer(function (req, res) {
  var parsed = httpSignature.parseRequest(req)
  var publicKeyPEM = publicKeyPEMs[parsed.keyId]
  var verified = httpSignature.verifySignature(parsed, publicKeyPEM)
  res.writeHead(verified ? 200 : 400)
  res.end()
})

server.listen(8080, function () {
  function correctKeyTest(callback) {
    var options = {
      httpSignature: {
        keyId: 'key-1',
        key: privateKeyPEMs['key-1']
      }
    }
    request('http://localhost:8080', options, function (e, r, b) {
      assert.equal(200, r.statusCode)
      callback()
    })
  }

  function incorrectKeyTest(callback) {
    var options = {
      httpSignature: {
        keyId: 'key-2',
        key: privateKeyPEMs['key-1']
      }
    }
    request('http://localhost:8080', options, function (e, r, b) {
      assert.equal(400, r.statusCode)
      callback()
    })
  }

  var tests = [correctKeyTest, incorrectKeyTest]
  var todo = tests.length;
  for(var i = 0; i < tests.length; ++i) {
    tests[i](function() {
      if(!--todo) {
        server.close()
      }
    })
  }
})
