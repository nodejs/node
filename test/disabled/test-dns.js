process.mixin(require("./common"));

var dns = require("dns"),
    sys = require("sys");

var hosts = ['example.com', 'example.org',
             'ietf.org', // AAAA
             'google.com', // MX, multiple A records
             '_xmpp-client._tcp.google.com', // SRV
             'oakalynhall.co.uk']; // Multiple PTR replies

var records = ['A', 'AAAA', 'MX', 'TXT', 'SRV'];

var i = hosts.length;
while (i--) {

  var j = records.length;
  while (j--) {
    var hostCmd = "dig -t " + records[j] + " " + hosts[i] +
                  "| grep '^" + hosts[i] + "\\.\\W.*IN.*" + records[j] + "'" +
                  "| sed -E 's/[[:space:]]+/ /g' | cut -d ' ' -f 5- " +
                  "| sed -e 's/\\.$//'";

    sys.exec(hostCmd).addCallback(checkDnsRecord(hosts[i], records[j]));
  }
}

function checkDnsRecord(host, record) {
  var myHost = host,
      myRecord = record;
  return function(stdout) {
    var expected = stdout.substr(0, stdout.length - 1).split("\n");
    
    var resolution = dns.resolve(myHost, myRecord);

    switch (myRecord) {
      case "A":
      case "AAAA":
        resolution.addCallback(function (result, ttl, cname) {
            cmpResults(expected, result, ttl, cname);

            // do reverse lookup check
            var ll = result.length;
            while (ll--) {
              var ip = result[ll];

              var reverseCmd = "host " + ip +
                               "| cut -d \" \" -f 5-" + 
                               "| sed -e 's/\\.$//'";

              sys.exec(reverseCmd).addCallback(checkReverse(ip));
            }
          }); 
        break;
      case "MX":
        resolution.addCallback(function (result, ttl, cname) {
            var strResult = [];
            var ll = result.length;
            while (ll--) {
              strResult.push(result[ll].priority + " " + result[ll].exchange);
            }

            cmpResults(expected, strResult, ttl, cname);
        });
        break;
      case "TXT":
        resolution.addCallback(function (result, ttl, cname) {
            var strResult = [];
            var ll = result.length;
            while (ll--) {
              strResult.push('"' + result[ll] + '"');
            }
            cmpResults(expected, strResult, ttl, cname);
        });
        break;
      case "SRV":
        resolution.addCallback(function (result, ttl, cname) {
            var strResult = [];
            var ll = result.length;
            while (ll--) {
              strResult.push(result[ll].priority + " " +
                             result[ll].weight + " " +
                             result[ll].port + " " +
                             result[ll].name);
            }
            cmpResults(expected, strResult, ttl, cname);
        });
        break;
    }
  }
}

function checkReverse(ip) {
  var myIp = ip;

  return function (stdout) {
    var expected = stdout.substr(0, stdout.length - 1).split("\n");

    var reversing = dns.reverse(myIp);

    reversing.addCallback(
      function (domains, ttl, cname) {
        cmpResults(expected, domains, ttl, cname);
      });
  }
}

function cmpResults(expected, result, ttl, cname) {
  assert.equal(expected.length, result.length);

  expected.sort();
  result.sort();

  ll = expected.length;
  while (ll--) {
    assert.equal(result[ll], expected[ll]);
//    puts("Result " + result[ll] + " was equal to expected " + expected[ll]);
  }
}
