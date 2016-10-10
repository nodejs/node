![Log Driver][logdriver-logo]
=========
[![Build Status][travis-image]][travis-url] [![Coverage Status][coveralls-image]][coveralls-url] [![NPM Version][npm-image]][npm-url]

Logdriver is a node.js logger that only logs to stdout.

####You're going to want to log the output of stdout and stderr anyway, so you might as well put all your logging through stdout.  Logging libraries that don't write to stdout or stderr are missing absolutely critical output like the stack trace if/when your app dies.  

##There are some other nice advantages:
* When working on your app locally, logs just show up in stdout just like if you'd used console.log().  That's a heck of a lot simpler than tailing a log file.
* Logging transports can be externalized from your app entirely, and completely decoupled.  This means if you want to log to irc, you write an irc client script that reads from stdin, and you just pipe your app's output to that script.

```console
node yourapp.js 2>&1 | node ircloggerbot.js 
```
* You can still easily log to a file on a production server by piping your stdout and stderr to a file like so when you initialize your app:

```console
node yourapp.js 2>&1 >> somefile.log 
```

NB: If you're logging to a file, [Logrotate](http://linuxcommand.org/man_pages/logrotate8.html) is probably going to be your best friend.
* You can still easily log to syslog by piping your stdout and stderr to the 'logger' command like so:

```console
node yourapp.js 2>&1 | logger
```

##Usage:
Getting the default logger:
```javascript
var logger = require('log-driver').logger;
```

This logger has levels 'error', 'warn', 'info', 'debug', and 'trace'.
If you don't like those levels, change the default:

```javascript
var logger = require('log-driver')({
  levels: ['superimportant', 'checkthisout', 'whocares' ]
});
logger.whocares("brangelina in lover's quarrel!");
```

Specifying what log level to log at to make logs less chatty:
```javascript
var logger = require('log-driver')({ level: "info" });
logger.info("info test"); 
logger.warn("warn test"); 
logger.error("error test"); 
logger.trace("trace test"); 
```
output:
```console
[info] "2013-03-26T18:30:14.570Z"  'info test'
[warn] "2013-03-26T18:30:14.573Z"  'warn test'
[error] "2013-03-26T18:30:14.574Z"  'error test'
```
(notice the trace() call was omitted because it's less than the info
level.

Turning off all log output (sometimes nice for automated tests to keep
output clean):
```javascript
var logger = require('log-driver')({ level: false });
```

Using the same logger everywhere:
The last logger you created is always available this way:
```javascript
var logger = require('log-driver').logger;
```
This way, if you use only one logger in your application (like most
applications), you can just configure it once, and get it this way
everywhere else.

Don't like the logging format?  Just change it by passing a new
formatting function like so:
```javascript
var logger = require('log-driver')({ 
  format: function() {
    // let's do pure JSON:
    return JSON.stringify(arguments);
  }
});
```

![Log Driver](https://raw.github.com/cainus/logdriver/master/waltz.jpg)

[logdriver-logo]: https://raw.github.com/cainus/logdriver/master/logo.png

[travis-image]: https://travis-ci.org/cainus/logdriver.png?branch=master
[travis-url]: https://travis-ci.org/cainus/logdriver

[coveralls-image]: https://coveralls.io/repos/cainus/logdriver/badge.png?branch=master
[coveralls-url]: https://coveralls.io/repos/cainus/logdriver

[npm-image]: https://badge.fury.io/js/log-driver.png
[npm-url]: https://badge.fury.io/js/log-driver
