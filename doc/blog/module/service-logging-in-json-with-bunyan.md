title: Service logging in JSON with Bunyan
author: trentmick
date: Wed Mar 28 2012 12:25:26 GMT-0700 (PDT)
status: publish
category: module
slug: service-logging-in-json-with-bunyan

<div style="float:right;margin:0 0 15px 15px;">
<img class="alignnone size-full wp-image-469" title="Bunyan" src="http://nodeblog.files.wordpress.com/2012/03/bunyan.png" alt="Paul Bunyan and Babe the Blue Ox" width="240" height="320" /><br />
<a href="http://www.flickr.com/photos/stublag/2876034487">Photo by Paul Carroll</a>
</div>

<p>Service logs are gold, if you can mine them. We scan them for occasional debugging. Perhaps we grep them looking for errors or warnings, or setup an occasional nagios log regex monitor. If that. This is a waste of the best channel for data about a service.</p>

<p><a href="http://www.youtube.com/watch?v=01-2pNCZiNk">"Log. (Huh) What is it good for. Absolutely ..."</a></p>

<ul>
<li>debugging</li>
<li>monitors tools that alert operators</li>
<li>non real-time analysis (business or operational analysis)</li>
<li>historical analysis</li>
</ul>

<p>These are what logs are good for. The current state of logging is barely adequate for the first of these. Doing reliable analysis, and even monitoring, of varied <a href="http://journal.paul.querna.org/articles/2011/12/26/log-for-machines-in-json/">"printf-style" logs</a> is a grueling or hacky task that most either don't bother with, fallback to paying someone else to do (viz. Splunk's great successes), or, for web sites, punt and use the plethora of JavaScript-based web analytics tools.</p>

<p>Let's log in JSON. Let's format log records with a filter <em>outside</em> the app. Let's put more info in log records by not shoehorning into a printf-message. Debuggability can be improved. Monitoring and analysis can <em>definitely</em> be improved. Let's <em>not</em> write another regex-based parser, and use the time we've saved writing tools to collate logs from multiple nodes and services, to query structured logs (from all services, not just web servers), etc.</p>

<p>At <a href="http://joyent.com">Joyent</a> we use node.js for running many core services -- loosely coupled through HTTP REST APIs and/or AMQP. In this post I'll draw on experiences from my work on Joyent's <a href="http://www.joyent.com/products/smartdatacenter/">SmartDataCenter product</a> and observations of <a href="http://www.joyentcloud.com/">Joyent Cloud</a> operations to suggest some improvements to service logging. I'll show the (open source) <strong>Bunyan logging library and tool</strong> that we're developing to improve the logging toolchain.</p>

<h1 style="margin:48px 0 24px;" id="current-state-of-log-formatting">Current State of Log Formatting</h1>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code># apache access log
10.0.1.22 - - [15/Oct/2010:11:46:46 -0700] "GET /favicon.ico HTTP/1.1" 404 209
fe80::6233:4bff:fe29:3173 - - [15/Oct/2010:11:46:58 -0700] "GET / HTTP/1.1" 200 44

# apache error log
[Fri Oct 15 11:46:46 2010] [error] [client 10.0.1.22] File does not exist: /Library/WebServer/Documents/favicon.ico
[Fri Oct 15 11:46:58 2010] [error] [client fe80::6233:4bff:fe29:3173] File does not exist: /Library/WebServer/Documents/favicon.ico

# Mac /var/log/secure.log
Oct 14 09:20:56 banana loginwindow[41]: in pam_sm_authenticate(): Failed to determine Kerberos principal name.
Oct 14 12:32:20 banana com.apple.SecurityServer[25]: UID 501 authenticated as user trentm (UID 501) for right 'system.privilege.admin'

# an internal joyent agent log
[2012-02-07 00:37:11.898] [INFO] AMQPAgent - Publishing success.
[2012-02-07 00:37:11.910] [DEBUG] AMQPAgent - { req_id: '8afb8d99-df8e-4724-8535-3d52adaebf25',
  timestamp: '2012-02-07T00:37:11.898Z',

# typical expressjs log output
[Mon, 21 Nov 2011 20:52:11 GMT] 200 GET /foo (1ms)
Blah, some other unstructured output to from a console.log call.
</code></pre>

<p>What're we doing here? Five logs at random. Five different date formats. As <a href="http://journal.paul.querna.org/articles/2011/12/26/log-for-machines-in-json/">Paul Querna points out</a> we haven't improved log parsability in 20 years. Parsability is enemy number one. You can't use your logs until you can parse the records, and faced with the above the inevitable solution is a one-off regular expression.</p>

<p>The current state of the art is various <a href="http://search.cpan.org/~akira/Apache-ParseLog-1.02/ParseLog.pm">parsing libs</a>, <a href="http://www.webalizer.org/">analysis</a> <a href="http://awstats.sourceforge.net/">tools</a> and homebrew scripts ranging from grep to Perl, whose scope is limited to a few niches log formats.</p>

<h1 style="margin:48px 0 24px;" id="json-for-logs">JSON for Logs</h1>

<p><code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">JSON.parse()</code> solves all that. Let's log in JSON. But it means a change in thinking: <strong>The first-level audience for log files shouldn't be a person, but a machine.</strong></p>

<p>That is not said lightly. The "Unix Way" of small focused tools lightly coupled with text output is important. JSON is less "text-y" than, e.g., Apache common log format. JSON makes <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">grep</code> and <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">awk</code> awkward. Using <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">less</code> directly on a log is handy.</p>

<p>But not handy enough. That <a href="http://bit.ly/wTPlN3">80's pastel jumpsuit awkwardness</a> you're feeling isn't the JSON, it's your tools. Time to find a <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">json</code> tool -- <a href="https://github.com/trentm/json">json</a> is one, <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">bunyan</code> described below is another one. Time to learn your JSON library instead of your regex library: <a href="https://developer.mozilla.org/en/JSON">JavaScript</a>, <a href="http://docs.python.org/library/json.html">Python</a>, <a href="http://flori.github.com/json/">Ruby</a>, <a href="http://json.org/java/">Java</a>, <a href="http://search.cpan.org/~makamaka/JSON-2.53/lib/JSON.pm">Perl</a>.</p>

<p>Time to burn your log4j Layout classes and move formatting to the tools side. Creating a log message with semantic information and throwing that away to make a string is silly. The win at being able to trivially parse log records is huge. The possibilities at being able to add ad hoc structured information to individual log records is interesting: think program state metrics, think feeding to Splunk, or loggly, think easy audit logs.</p>

<h1 style="margin:48px 0 24px;" id="introducing-bunyan">Introducing Bunyan</h1>

<p><a href="https://github.com/trentm/node-bunyan">Bunyan</a> is <strong>a node.js module for logging in JSON</strong> and <strong>a <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">bunyan</code> CLI tool</strong> to view those logs.</p>

<p>Logging with Bunyan basically looks like this:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>$ cat hi.js
var Logger = require('bunyan');
var log = new Logger({name: 'hello' /*, ... */});
log.info("hi %s", "paul");
</code></pre>

<p>And you'll get a log record like this:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>$ node hi.js
{"name":"hello","hostname":"banana.local","pid":40026,"level":30,"msg":"hi paul","time":"2012-03-28T17:25:37.050Z","v":0}
</code></pre>

<p>Pipe that through the <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">bunyan</code> tool that is part of the "node-bunyan" install to get more readable output:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>$ node hi.js | ./node_modules/.bin/bunyan       # formatted text output
[2012-02-07T18:50:18.003Z]  INFO: hello/40026 on banana.local: hi paul

$ node hi.js | ./node_modules/.bin/bunyan -j    # indented JSON output
{
  "name": "hello",
  "hostname": "banana.local",
  "pid": 40087,
  "level": 30,
  "msg": "hi paul",
  "time": "2012-03-28T17:26:38.431Z",
  "v": 0
}
</code></pre>

<p>Bunyan is log4j-like: create a Logger with a name, call <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">log.info(...)</code>, etc. However it has no intention of reproducing much of the functionality of log4j. IMO, much of that is overkill for the types of services you'll tend to be writing with node.js.</p>

<h1 style="margin:48px 0 24px;" id="longer-bunyan-example">Longer Bunyan Example</h1>

<p>Let's walk through a bigger example to show some interesting things in Bunyan. We'll create a very small "Hello API" server using the excellent <a href="https://github.com/mcavage/node-restify">restify</a> library -- which we used heavily here at <a href="http://joyent.com">Joyent</a>. (Bunyan doesn't require restify at all, you can easily use Bunyan with <a href="http://expressjs.com/">Express</a> or whatever.)</p>

<p><em>You can follow along in <a href="https://github.com/trentm/hello-json-logging">https://github.com/trentm/hello-json-logging</a> if you like. Note that I'm using the current HEAD of the bunyan and restify trees here, so details might change a bit. Prerequisite: a node 0.6.x installation.</em></p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>git clone https://github.com/trentm/hello-json-logging.git
cd hello-json-logging
make
</code></pre>

<h2 id="bunyan-logger">Bunyan Logger</h2>

<p>Our <a href="https://github.com/trentm/hello-json-logging/blob/master/server.js">server</a> first creates a Bunyan logger:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>var Logger = require('bunyan');
var log = new Logger({
  name: 'helloapi',
  streams: [
    {
      stream: process.stdout,
      level: 'debug'
    },
    {
      path: 'hello.log',
      level: 'trace'
    }
  ],
  serializers: {
    req: Logger.stdSerializers.req,
    res: restify.bunyan.serializers.response,
  },
});
</code></pre>

<p>Every Bunyan logger must have a <strong>name</strong>. Unlike log4j, this is not a hierarchical dotted namespace. It is just a name field for the log records.</p>

<p>Every Bunyan logger has one or more <strong>streams</strong>, to which log records are written. Here we've defined two: logging at DEBUG level and above is written to stdout, and logging at TRACE and above is appended to 'hello.log'.</p>

<p>Bunyan has the concept of <strong>serializers</strong>: a registry of functions that know how to convert a JavaScript object for a certain log record field to a nice JSON representation for logging. For example, here we register the <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">Logger.stdSerializers.req</code> function to convert HTTP Request objects (using the field name "req") to JSON. More on serializers later.</p>

<h2 id="restify-server">Restify Server</h2>

<p>Restify 1.x and above has bunyan support baked in. You pass in your Bunyan logger like this:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>var server = restify.createServer({
  name: 'Hello API',
  log: log   // Pass our logger to restify.
});
</code></pre>

<p>Our simple API will have a single <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">GET /hello?name=NAME</code> endpoint:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>server.get({path: '/hello', name: 'SayHello'}, function(req, res, next) {
  var caller = req.params.name || 'caller';
  req.log.debug('caller is "%s"', caller);
  res.send({"hello": caller});
  return next();
});
</code></pre>

<p>If we run that, <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">node server.js</code>, and call the endpoint, we get the expected restify response:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>$ curl -iSs http://0.0.0.0:8080/hello?name=paul
HTTP/1.1 200 OK
Access-Control-Allow-Origin: *
Access-Control-Allow-Headers: Accept, Accept-Version, Content-Length, Content-MD5, Content-Type, Date, X-Api-Version
Access-Control-Expose-Headers: X-Api-Version, X-Request-Id, X-Response-Time
Server: Hello API
X-Request-Id: f6aaf942-c60d-4c72-8ddd-bada459db5e3
Access-Control-Allow-Methods: GET
Connection: close
Content-Length: 16
Content-MD5: Xmn3QcFXaIaKw9RPUARGBA==
Content-Type: application/json
Date: Tue, 07 Feb 2012 19:12:35 GMT
X-Response-Time: 4

{"hello":"paul"}
</code></pre>

<h2 id="setup-server-logging">Setup Server Logging</h2>

<p>Let's add two things to our server. First, we'll use the <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">server.pre</code> to hook into restify's request handling before routing where we'll <strong>log the request</strong>.</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>server.pre(function (request, response, next) {
  request.log.info({req: request}, 'start');        // (1)
  return next();
});
</code></pre>

<p>This is the first time we've seen this <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">log.info</code> style with an object as the first argument. Bunyan logging methods (<code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">log.trace</code>, <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">log.debug</code>, ...) all support an optional <strong>first object argument with extra log record fields</strong>:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>log.info(&lt;object&gt; fields, &lt;string&gt; msg, ...)
</code></pre>

<p>Here we pass in the restify Request object, <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">req</code>. The "req" serializer we registered above will come into play here, but bear with me.</p>

<p>Remember that we already had this debug log statement in our endpoint handler:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>req.log.debug('caller is "%s"', caller);            // (2)
</code></pre>

<p>Second, use the restify server <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">after</code> event to <strong>log the response</strong>:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>server.on('after', function (req, res, route) {
  req.log.info({res: res}, "finished");             // (3)
});
</code></pre>

<h2 id="log-output">Log Output</h2>

<p>Now lets see what log output we get when somebody hits our API's endpoint:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>$ curl -iSs http://0.0.0.0:8080/hello?name=paul
HTTP/1.1 200 OK
...
X-Request-Id: 9496dfdd-4ec7-4b59-aae7-3fed57aed5ba
...

{"hello":"paul"}
</code></pre>

<p>Here is the server log:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>[trentm@banana:~/tm/hello-json-logging]$ node server.js
... intro "listening at" log message elided ...
{"name":"helloapi","hostname":"banana.local","pid":40341,"level":30,"req":{"method":"GET","url":"/hello?name=paul","headers":{"user-agent":"curl/7.19.7 (universal-apple-darwin10.0) libcurl/7.19.7 OpenSSL/0.9.8r zlib/1.2.3","host":"0.0.0.0:8080","accept":"*/*"},"remoteAddress":"127.0.0.1","remotePort":59831},"msg":"start","time":"2012-03-28T17:37:29.506Z","v":0}
{"name":"helloapi","hostname":"banana.local","pid":40341,"route":"SayHello","req_id":"9496dfdd-4ec7-4b59-aae7-3fed57aed5ba","level":20,"msg":"caller is \"paul\"","time":"2012-03-28T17:37:29.507Z","v":0}
{"name":"helloapi","hostname":"banana.local","pid":40341,"route":"SayHello","req_id":"9496dfdd-4ec7-4b59-aae7-3fed57aed5ba","level":30,"res":{"statusCode":200,"headers":{"access-control-allow-origin":"*","access-control-allow-headers":"Accept, Accept-Version, Content-Length, Content-MD5, Content-Type, Date, X-Api-Version","access-control-expose-headers":"X-Api-Version, X-Request-Id, X-Response-Time","server":"Hello API","x-request-id":"9496dfdd-4ec7-4b59-aae7-3fed57aed5ba","access-control-allow-methods":"GET","connection":"close","content-length":16,"content-md5":"Xmn3QcFXaIaKw9RPUARGBA==","content-type":"application/json","date":"Wed, 28 Mar 2012 17:37:29 GMT","x-response-time":3}},"msg":"finished","time":"2012-03-28T17:37:29.510Z","v":0}
</code></pre>

<p>Lets look at each in turn to see what is interesting -- pretty-printed with <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">node server.js | ./node_modules/.bin/bunyan -j</code>:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>{                                                   // (1)
  "name": "helloapi",
  "hostname": "banana.local",
  "pid": 40442,
  "level": 30,
  "req": {
    "method": "GET",
    "url": "/hello?name=paul",
    "headers": {
      "user-agent": "curl/7.19.7 (universal-apple-darwin10.0) libcurl/7.19.7 OpenSSL/0.9.8r zlib/1.2.3",
      "host": "0.0.0.0:8080",
      "accept": "*/*"
    },
    "remoteAddress": "127.0.0.1",
    "remotePort": 59834
  },
  "msg": "start",
  "time": "2012-03-28T17:39:44.880Z",
  "v": 0
}
</code></pre>

<p>Here we logged the incoming request with <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">request.log.info({req: request}, 'start')</code>. The use of the "req" field triggers the <a href="https://github.com/trentm/node-bunyan/blob/master/lib/bunyan.js#L857-870">"req" serializer</a> <a href="https://github.com/trentm/hello-json-logging/blob/master/server.js#L24">registered at Logger creation</a>.</p>

<p>Next the <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">req.log.debug</code> in our handler:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>{                                                   // (2)
  "name": "helloapi",
  "hostname": "banana.local",
  "pid": 40442,
  "route": "SayHello",
  "req_id": "9496dfdd-4ec7-4b59-aae7-3fed57aed5ba",
  "level": 20,
  "msg": "caller is \"paul\"",
  "time": "2012-03-28T17:39:44.883Z",
  "v": 0
}
</code></pre>

<p>and the log of response in the "after" event:</p>

<pre style="overflow:auto;color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:5px;"><code>{                                                   // (3)
  "name": "helloapi",
  "hostname": "banana.local",
  "pid": 40442,
  "route": "SayHello",
  "req_id": "9496dfdd-4ec7-4b59-aae7-3fed57aed5ba",
  "level": 30,
  "res": {
    "statusCode": 200,
    "headers": {
      "access-control-allow-origin": "*",
      "access-control-allow-headers": "Accept, Accept-Version, Content-Length, Content-MD5, Content-Type, Date, X-Api-Version",
      "access-control-expose-headers": "X-Api-Version, X-Request-Id, X-Response-Time",
      "server": "Hello API",
      "x-request-id": "9496dfdd-4ec7-4b59-aae7-3fed57aed5ba",
      "access-control-allow-methods": "GET",
      "connection": "close",
      "content-length": 16,
      "content-md5": "Xmn3QcFXaIaKw9RPUARGBA==",
      "content-type": "application/json",
      "date": "Wed, 28 Mar 2012 17:39:44 GMT",
      "x-response-time": 5
    }
  },
  "msg": "finished",
  "time": "2012-03-28T17:39:44.886Z",
  "v": 0
}
</code></pre>

<p>Two useful details of note here:</p>

<ol>
<li><p>The last two log messages include <strong>a "req_id" field</strong> (added to the <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">req.log</code> logger by restify). Note that this is the same UUID as the "X-Request-Id" header in the <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">curl</code> response. This means that if you use <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">req.log</code> for logging in your API handlers you will get an easy way to collate all logging for particular requests.</p>

<p>If your's is an SOA system with many services, a best practice is to carry that X-Request-Id/req_id through your system to enable collating handling of a single top-level request.</p></li>
<li><p>The last two log messages include <strong>a "route" field</strong>. This tells you to which handler restify routed the request. While possibly useful for debugging, this can be very helpful for log-based monitoring of endpoints on a server.</p></li>
</ol>

<p>Recall that we also setup all logging to go the "hello.log" file. This was set at the TRACE level. Restify will log more detail of its operation at the trace level. See <a href="https://gist.github.com/1761772">my "hello.log"</a> for an example. The <code style="color:#999;background-color:#2f2f2f;border:1px solid #484848;padding:.2em .4em;">bunyan</code> tool does a decent job of <a href="https://gist.github.com/1761772#file_2.+cat+hello.log+pipe+bunyan">nicely formatting</a> multiline messages and "req"/"res" keys (with color, not shown in the gist).</p>

<p><em>This</em> is logging you can use effectively.</p>

<h1 style="margin:48px 0 24px;" id="other-tools">Other Tools</h1>

<p>Bunyan is just one of many options for logging in node.js-land. Others (that I know of) supporting JSON logging are <a href="https://github.com/flatiron/winston#readme">winston</a> and <a href="https://github.com/pquerna/node-logmagic/">logmagic</a>. Paul Querna has <a href="http://journal.paul.querna.org/articles/2011/12/26/log-for-machines-in-json/">an excellent post on using JSON for logging</a>, which shows logmagic usage and also touches on topics like the GELF logging format, log transporting, indexing and searching.</p>

<h1 style="margin:48px 0 24px;" id="final-thoughts">Final Thoughts</h1>

<p>Parsing challenges won't ever completely go away, but it can for your logs if you use JSON. Collating log records across logs from multiple nodes is facilitated by a common "time" field. Correlating logging across multiple services is enabled by carrying a common "req_id" (or equivalent) through all such logs.</p>

<p>Separate log files for a single service is an anti-pattern. The typical Apache example of separate access and error logs is legacy, not an example to follow. A JSON log provides the structure necessary for tooling to easily filter for log records of a particular type.</p>

<p>JSON logs bring possibilities. Feeding to tools like Splunk becomes easy. Ad hoc fields allow for a lightly spec'd comm channel from apps to other services: records with a "metric" could feed to <a href="http://codeascraft.etsy.com/2011/02/15/measure-anything-measure-everything/">statsd</a>, records with a "loggly: true" could feed to loggly.com.</p>

<p>Here I've described a very simple example of restify and bunyan usage for node.js-based API services with easy JSON logging. Restify provides a powerful framework for robust API services. Bunyan provides a light API for nice JSON logging and the beginnings of tooling to help consume Bunyan JSON logs.</p>

<p><strong>Update (29-Mar-2012):</strong> Fix styles somewhat for RSS readers.</p>
