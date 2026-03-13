# AsyncHooks Coverage Overview

Showing which kind of async resource is covered by which test:

| Resource Type       | Test                                    |
| ------------------- | --------------------------------------- |
| CONNECTION          | test-connection.ssl.js                  |
| FSEVENTWRAP         | test-fseventwrap.js                     |
| FSREQCALLBACK       | test-fsreqcallback-{access,readFile}.js |
| GETADDRINFOREQWRAP  | test-getaddrinforeqwrap.js              |
| GETNAMEINFOREQWRAP  | test-getnameinforeqwrap.js              |
| HTTPINCOMINGMESSAGE | test-httpparser.request.js              |
| HTTPCLIENTREQUEST   | test-httpparser.response.js             |
| Immediate           | test-immediate.js                       |
| JSSTREAM            | TODO (crashes when accessing directly)  |
| PBKDF2REQUEST       | test-crypto-pbkdf2.js                   |
| PIPECONNECTWRAP     | test-pipeconnectwrap.js                 |
| PIPEWRAP            | test-pipewrap.js                        |
| PROCESSWRAP         | test-pipewrap.js                        |
| QUERYWRAP           | test-querywrap.js                       |
| RANDOMBYTESREQUEST  | test-crypto-randomBytes.js              |
| SHUTDOWNWRAP        | test-shutdownwrap.js                    |
| SIGNALWRAP          | test-signalwrap.js                      |
| STATWATCHER         | test-statwatcher.js                     |
| TCPCONNECTWRAP      | test-tcpwrap.js                         |
| TCPWRAP             | test-tcpwrap.js                         |
| TLSWRAP             | test-tlswrap.js                         |
| TTYWRAP             | test-ttywrap.{read,write}stream.js      |
| UDPSENDWRAP         | test-udpsendwrap.js                     |
| UDPWRAP             | test-udpwrap.js                         |
| WRITEWRAP           | test-writewrap.js                       |
| ZLIB                | test-zlib.zlib-binding.deflate.js       |
