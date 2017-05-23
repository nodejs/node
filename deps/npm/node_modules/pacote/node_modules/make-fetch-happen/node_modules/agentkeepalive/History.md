
3.1.0 / 2017-02-20
==================

  * feat: timeout support humanize ms (#48)

3.0.0 / 2016-12-20
==================

  * fix: emit agent socket close event
  * test: add remove excess calls to removeSocket
  * test: use egg-ci
  * test: refactor test with eslint rules
  * feat: merge _http_agent.js from 7.2.1

2.2.0 / 2016-06-26
==================

  * feat: Add browser shim (noop) for isomorphic use. (#39)
  * chore: add security check badge

2.1.1 / 2016-04-06
==================

  * https: fix ssl socket leak when keepalive is used
  * chore: remove circle ci image

2.1.0 / 2016-04-02
==================

  * fix: opened sockets number overflow maxSockets

2.0.5 / 2016-03-16
==================

  * fix: pick _evictSession to httpsAgent

2.0.4 / 2016-03-13
==================

  * test: add Circle ci
  * test: add appveyor ci build
  * refactor: make sure only one error listener
  * chore: use codecov
  * fix: handle idle socket error
  * test: run on more node versions

2.0.3 / 2015-08-03
==================

 * fix: add default error handler to avoid Unhandled error event throw

2.0.2 / 2015-04-25
==================

 * fix: remove socket from freeSockets on 'timeout' (@pmalouin)

2.0.1 / 2015-04-19
==================

 * fix: add timeoutSocketCount to getCurrentStatus()
 * feat(getCurrentStatus): add getCurrentStatus

2.0.0 / 2015-04-01
==================

 * fix: socket.destroyed always be undefined on 0.10.x
 * Make it compatible with node v0.10.x (@lattmann)

1.2.1 / 2015-03-23
==================

 * patch from iojs: don't overwrite servername option
 * patch commits from joyent/node
 * add max sockets test case
 * add nagle algorithm delayed link

1.2.0 / 2014-09-02
==================

 * allow set keepAliveTimeout = 0
 * support timeout on working socket. fixed #6

1.1.0 / 2014-08-28
==================

 * add some socket counter for deep monitor

1.0.0 / 2014-08-13
==================

 * update _http_agent, only support 0.11+, only support node 0.11.0+

0.2.2 / 2013-11-19 
==================

  * support node 0.8 and node 0.10

0.2.1 / 2013-11-08 
==================

  * fix socket does not timeout bug, it will hang on life, must use 0.2.x on node 0.11

0.2.0 / 2013-11-06 
==================

  * use keepalive agent on node 0.11+ impl

0.1.5 / 2013-06-24 
==================

  * support coveralls
  * add node 0.10 test
  * add 0.8.22 original https.js
  * add original http.js module to diff
  * update jscover
  * mv pem to fixtures
  * add https agent usage
