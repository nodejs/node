
[Expresso](http://github.com/visionmedia/expresso) is a JavaScript [TDD](http://en.wikipedia.org/wiki/Test-driven_development) framework written for [nodejs](http://nodejs.org). Expresso is extremely fast, and is packed with features such as additional assertion methods, code coverage reporting, CI support, and more.

## Features

  - light-weight
  - intuitive async support
  - intuitive test runner executable
  - test coverage support and reporting via [node-jscoverage](http://github.com/visionmedia/node-jscoverage)
  - uses and extends the core _assert_ module
  - `assert.eql()` alias of `assert.deepEqual()`
  - `assert.response()` http response utility
  - `assert.includes()`
  - `assert.isNull()`
  - `assert.isUndefined()`
  - `assert.isNotNull()`
  - `assert.isDefined()`
  - `assert.match()`
  - `assert.length()`

## Installation

To install both expresso _and_ node-jscoverage run
the command below, which will first compile node-jscoverage:

    $ make install

To install expresso alone without coverage reporting run:

    $ make install-expresso

Install via npm:

	$ npm install expresso

## Examples

To define tests we simply export several functions:

	exports['test String#length'] = function(){
		assert.equal(6, 'foobar'.length);
	};

Alternatively for large numbers of tests you may want to
export your own object containing the tests, however this
is essentially the as above:

    module.exports = {
      	'test String#length': function(){
        	assert.equal(6, 'foobar'.length);
      	}
    };

If you prefer not to use quoted keys:

	exports.testsStringLength = function(){
		assert.equal(6, 'foobar'.length);
	};

The argument passed to each callback is _beforeExit_,
which is typically used to assert that callbacks have been
invoked.

    exports.testAsync = function(beforeExit){
		var n = 0;
      	setTimeout(function(){
        	++n;
        	assert.ok(true);
      	}, 200);
      	setTimeout(function(){
        	++n;
        	assert.ok(true);
      	}, 200);
		beforeExit(function(){
			assert.equal(2, n, 'Ensure both timeouts are called');
		});
    };

## Assert Utilities

### assert.isNull(val[, msg])

Asserts that the given _val_ is _null_.

    assert.isNull(null);

### assert.isNotNull(val[, msg])

Asserts that the given _val_ is not _null_.

    assert.isNotNull(undefined);
    assert.isNotNull(false);

### assert.isUndefined(val[, msg])

Asserts that the given _val_ is _undefined_.

    assert.isUndefined(undefined);

### assert.isDefined(val[, msg])

Asserts that the given _val_ is not _undefined_.

    assert.isDefined(null);
    assert.isDefined(false);

### assert.match(str, regexp[, msg])

Asserts that the given _str_ matches _regexp_.

    assert.match('foobar', /^foo(bar)?/);
    assert.match('foo', /^foo(bar)?/);

### assert.length(val, n[, msg])

Assert that the given _val_ has a length of _n_.

    assert.length([1,2,3], 3);
    assert.length('foo', 3);

### assert.type(obj, type[, msg])

Assert that the given _obj_ is typeof _type_.

    assert.type(3, 'number');

### assert.eql(a, b[, msg])

Assert that object _b_ is equal to object _a_. This is an
alias for the core _assert.deepEqual()_ method which does complex
comparisons, opposed to _assert.equal()_ which uses _==_.

    assert.eql('foo', 'foo');
    assert.eql([1,2], [1,2]);
    assert.eql({ foo: 'bar' }, { foo: 'bar' });

### assert.includes(obj, val[, msg])

Assert that _obj_ is within _val_. This method supports _Array_s
and _Strings_s.

    assert.includes([1,2,3], 3);
    assert.includes('foobar', 'foo');
    assert.includes('foobar', 'bar');

### assert.response(server, req, res|fn[, msg|fn])

Performs assertions on the given _server_, which should _not_ call
listen(), as this is handled internally by expresso and the server 
is killed after all responses have completed. This method works with
any _http.Server_ instance, so _Connect_ and _Express_ servers will work
as well.

The _req_ object may contain:

  - _url_ request url
  - _timeout_ timeout in milliseconds
  - _method_ HTTP method
  - _data_ request body
  - _headers_ headers object

The _res_ object may be a callback function which
receives the response for assertions, or an object
which is then used to perform several assertions
on the response with the following properties:

  - _body_ assert response body (regexp or string)
  - _status_ assert response status code
  - _header_ assert that all given headers match (unspecified are ignored, use a regexp or string)

When providing _res_ you may then also pass a callback function
as the fourth argument for additional assertions.

Below are some examples:

    assert.response(server, {
	  	url: '/', timeout: 500
    }, {
		body: 'foobar'
    });

    assert.response(server, {
        url: '/',
        method: 'GET'
    },{
        body: '{"name":"tj"}',
        status: 200,
        headers: {
            'Content-Type': 'application/json; charset=utf8',
			'X-Foo': 'bar'
        }
    });
    
    assert.response(server, {
        url: '/foo',
        method: 'POST',
        data: 'bar baz'
    },{
        body: '/foo bar baz',
        status: 200
    }, 'Test POST');

    assert.response(server, {
        url: '/foo',
        method: 'POST',
        data: 'bar baz'
    },{
        body: '/foo bar baz',
        status: 200
    }, function(res){
		// All done, do some more tests if needed
	});

    assert.response(server, {
        url: '/'
    }, function(res){
        assert.ok(res.body.indexOf('tj') >= 0, 'Test assert.response() callback');
    });


## expresso(1)

To run a single test suite (file) run:

    $ expresso test/a.test.js

To run several suites we may simply append another:

    $ expresso test/a.test.js test/b.test.js

We can also pass a whitelist of tests to run within all suites:

    $ expresso --only "foo()" --only "bar()"

Or several with one call:

    $ expresso --only "foo(), bar()"

Globbing is of course possible as well:

    $ expresso test/*

When expresso is called without any files, _test/*_ is the default,
so the following is equivalent to the command above:

    $ expresso

If you wish to unshift a path to `require.paths` before
running tests, you may use the `-I` or `--include` flag.

    $ expresso --include lib test/*

The previous example is typically what I would recommend, since expresso
supports test coverage via [node-jscoverage](http://github.com/visionmedia/node-jscoverage) (bundled with expresso),
so you will need to expose an instrumented version of you library.

To instrument your library, simply run [node-jscoverage](http://github.com/visionmedia/node-jscoverage),
passing the _src_ and _dest_ directories:

    $ node-jscoverage lib lib-cov

Now we can run our tests again, using the _lib-cov_ directory that has been
instrumented with coverage statements:

    $ expresso -I lib-cov test/*

The output will look similar to below, depending on your test coverage of course :)

![node coverage](http://dl.dropbox.com/u/6396913/cov.png)

To make this process easier expresso has the _-c_ or _--cov_ which essentially
does the same as the two commands above. The following two commands will
run the same tests, however one will auto-instrument, and unshift _lib-cov_,
and the other will run tests normally:

    $ expresso -I lib test/*
    $ expresso -I lib --cov test/*

Currently coverage is bound to the _lib_ directory, however in the
future `--cov` will most likely accept a path.

## Async Exports

Sometimes it is useful to postpone running of tests until a callback or event has fired, currently the _exports.foo = function(){};_ syntax is supported for this:
    
	setTimeout(function(){
	    exports['test async exports'] = function(){
	        assert.ok('wahoo');
	    };
	}, 100);
