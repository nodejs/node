var server = require('./server')
  , events = require('events')
  , stream = require('stream')
  , assert = require('assert')
  , fs = require('fs')
  , request = require('../index')
  , path = require('path')
  , util = require('util')
  ;

var s = server.createServer(3453);

function ValidationStream(str) {
  this.str = str
  this.buf = ''
  this.on('data', function (data) {
    this.buf += data
  })
  this.on('end', function () {
    assert.equal(this.str, this.buf)
  })
  this.writable = true
}
util.inherits(ValidationStream, stream.Stream)
ValidationStream.prototype.write = function (chunk) {
  this.emit('data', chunk)
}
ValidationStream.prototype.end = function (chunk) {
  if (chunk) emit('data', chunk)
  this.emit('end')
}

s.listen(s.port, function () {
  counter = 0;

  var check = function () {
    counter = counter - 1
    if (counter === 0) {
      console.log('All tests passed.')
      setTimeout(function () {
        process.exit();
      }, 500)
    }
  }

  // Test pipeing to a request object
  s.once('/push', server.createPostValidator("mydata"));

  var mydata = new stream.Stream();
  mydata.readable = true

  counter++
  var r1 = request.put({url:'http://localhost:3453/push'}, function () {
    check();
  })
  mydata.pipe(r1)

  mydata.emit('data', 'mydata');
  mydata.emit('end');

  // Test pipeing to a request object with a json body
  s.once('/push-json', server.createPostValidator("{\"foo\":\"bar\"}", "application/json"));

  var mybodydata = new stream.Stream();
  mybodydata.readable = true

  counter++
  var r2 = request.put({url:'http://localhost:3453/push-json',json:true}, function () {
    check();
  })
  mybodydata.pipe(r2)

  mybodydata.emit('data', JSON.stringify({foo:"bar"}));
  mybodydata.emit('end');

  // Test pipeing from a request object.
  s.once('/pull', server.createGetResponse("mypulldata"));

  var mypulldata = new stream.Stream();
  mypulldata.writable = true

  counter++
  request({url:'http://localhost:3453/pull'}).pipe(mypulldata)

  var d = '';

  mypulldata.write = function (chunk) {
    d += chunk;
  }
  mypulldata.end = function () {
    assert.equal(d, 'mypulldata');
    check();
  };


  s.on('/cat', function (req, resp) {
    if (req.method === "GET") {
      resp.writeHead(200, {'content-type':'text/plain-test', 'content-length':4});
      resp.end('asdf')
    } else if (req.method === "PUT") {
      assert.equal(req.headers['content-type'], 'text/plain-test');
      assert.equal(req.headers['content-length'], 4)
      var validate = '';

      req.on('data', function (chunk) {validate += chunk})
      req.on('end', function () {
        resp.writeHead(201);
        resp.end();
        assert.equal(validate, 'asdf');
        check();
      })
    }
  })
  s.on('/pushjs', function (req, resp) {
    if (req.method === "PUT") {
      assert.equal(req.headers['content-type'], 'application/javascript');
      check();
    }
  })
  s.on('/catresp', function (req, resp) {
    request.get('http://localhost:3453/cat').pipe(resp)
  })
  s.on('/doodle', function (req, resp) {
    if (req.headers['x-oneline-proxy']) {
      resp.setHeader('x-oneline-proxy', 'yup')
    }
    resp.writeHead('200', {'content-type':'image/jpeg'})
    fs.createReadStream(path.join(__dirname, 'googledoodle.jpg')).pipe(resp)
  })
  s.on('/onelineproxy', function (req, resp) {
    var x = request('http://localhost:3453/doodle')
    req.pipe(x)
    x.pipe(resp)
  })

  counter++
  fs.createReadStream(__filename).pipe(request.put('http://localhost:3453/pushjs'))

  counter++
  request.get('http://localhost:3453/cat').pipe(request.put('http://localhost:3453/cat'))

  counter++
  request.get('http://localhost:3453/catresp', function (e, resp, body) {
    assert.equal(resp.headers['content-type'], 'text/plain-test');
    assert.equal(resp.headers['content-length'], 4)
    check();
  })

  var doodleWrite = fs.createWriteStream(path.join(__dirname, 'test.jpg'))

  counter++
  request.get('http://localhost:3453/doodle').pipe(doodleWrite)

  doodleWrite.on('close', function () {
    assert.deepEqual(fs.readFileSync(path.join(__dirname, 'googledoodle.jpg')), fs.readFileSync(path.join(__dirname, 'test.jpg')))
    check()
  })

  process.on('exit', function () {
    fs.unlinkSync(path.join(__dirname, 'test.jpg'))
  })

  counter++
  request.get({uri:'http://localhost:3453/onelineproxy', headers:{'x-oneline-proxy':'nope'}}, function (err, resp, body) {
    assert.equal(resp.headers['x-oneline-proxy'], 'yup')
    check()
  })

  s.on('/afterresponse', function (req, resp) {
    resp.write('d')
    resp.end()
  })

  counter++
  var afterresp = request.post('http://localhost:3453/afterresponse').on('response', function () {
    var v = new ValidationStream('d')
    afterresp.pipe(v)
    v.on('end', check)
  })
  
  s.on('/forward1', function (req, resp) {
   resp.writeHead(302, {location:'/forward2'})
    resp.end()
  })
  s.on('/forward2', function (req, resp) {
    resp.writeHead('200', {'content-type':'image/png'})
    resp.write('d')
    resp.end()
  })
  
  counter++
  var validateForward = new ValidationStream('d')
  validateForward.on('end', check)
  request.get('http://localhost:3453/forward1').pipe(validateForward)

  // Test pipe options
  s.once('/opts', server.createGetResponse('opts response'));

  var optsStream = new stream.Stream();
  optsStream.writable = true
  
  var optsData = '';
  optsStream.write = function (buf) {
    optsData += buf;
    if (optsData === 'opts response') {
      setTimeout(check, 10);
    }
  }

  optsStream.end = function () {
    assert.fail('end called')
  };

  counter++
  request({url:'http://localhost:3453/opts'}).pipe(optsStream, { end : false })
})
