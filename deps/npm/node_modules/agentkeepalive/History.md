
3.5.2 / 2018-10-19
==================

**fixes**
  * [[`5751fc1`](http://github.com/node-modules/agentkeepalive/commit/5751fc1180ed6544602c681ffbd08ca66a0cb12c)] - fix: sockLen being miscalculated when removing sockets (#60) (Ehden Sinai <<cixel@users.noreply.github.com>>)

3.5.1 / 2018-07-31
==================

**fixes**
  * [[`495f1ab`](http://github.com/node-modules/agentkeepalive/commit/495f1ab625d43945d72f68096b97db723d4f0657)] - fix: add the lost npm files (#66) (Henry Zhuang <<zhuanghengfei@gmail.com>>)

3.5.0 / 2018-07-31
==================

**features**
  * [[`16f5aea`](http://github.com/node-modules/agentkeepalive/commit/16f5aeadfda57f1c602652f1472a63cc83cd05bf)] - feat: add typing define. (#65) (Henry Zhuang <<zhuanghengfei@gmail.com>>)

**others**
  * [[`28fa062`](http://github.com/node-modules/agentkeepalive/commit/28fa06246fb5103f88ebeeb8563757a9078b8157)] - docs: add "per host" to description of maxFreeSockets (tony-gutierrez <<tony.gutierrez@bluefletch.com>>)
  * [[`7df2577`](http://github.com/node-modules/agentkeepalive/commit/7df25774f00a1031ca4daad2878a17e0539072a2)] - test: run test on node 10 (#63) (fengmk2 <<fengmk2@gmail.com>>)

3.4.1 / 2018-03-08
==================

**fixes**
  * [[`4d3a3b1`](http://github.com/node-modules/agentkeepalive/commit/4d3a3b1f7b16595febbbd39eeed72b2663549014)] - fix: Handle ipv6 addresses in host-header correctly with TLS (#53) (Mattias Holmlund <<u376@m1.holmlund.se>>)

**others**
  * [[`55a7a5c`](http://github.com/node-modules/agentkeepalive/commit/55a7a5cd33e97f9a8370083dcb041c5552f10ac9)] - test: stop timer after test end (fengmk2 <<fengmk2@gmail.com>>)

3.4.0 / 2018-02-27
==================

**features**
  * [[`bc7cadb`](http://github.com/node-modules/agentkeepalive/commit/bc7cadb30ecd2071e2b341ac53ae1a2b8155c43d)] - feat: use socket custom freeSocketKeepAliveTimeout first (#59) (fengmk2 <<fengmk2@gmail.com>>)

**others**
  * [[`138eda8`](http://github.com/node-modules/agentkeepalive/commit/138eda81e10b632aaa87bea0cb66d8667124c4e8)] - doc: fix `keepAliveMsecs` params description (#55) (Hongcai Deng <<admin@dhchouse.com>>)

3.3.0 / 2017-06-20
==================

  * feat: add statusChanged getter (#51)
  * chore: format License

3.2.0 / 2017-06-10
==================

  * feat: add expiring active sockets
  * test: add node 8 (#49)

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
