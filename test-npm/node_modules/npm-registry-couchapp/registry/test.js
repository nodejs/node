var http = require("http"),
    url = require("url"),
    sys = require("sys");

function request (path, method, headers, body, callback) {
  if (!headers) {
    headers = {'Content-Type':'application/json', "Accept":'application/json', "Host":"jsregistry:5984"};
  }
  if (!method) {
    method = "GET"
  }
  if (body) {
    if (typeof body !== "string") {
      body = JSON.stringify(body)
    }
  }
  
  var client = http.createClient(5984, "jsregistry");
  var request = client.request(method, path, headers);  
  request.addListener("response", function (response) {
    var buffer = ''
    response.addListener("data", function (chunk) {
      buffer += chunk;
    })
    response.addListener("end", function () {
      callback(response, buffer);
    })
  })
  request.write(body)
  request.close()
  
}

function requestQueue (args, doneCallback) {
  var runner = function (args, i) {
    var validation = args[i].pop();
    args[i].push(function (request, response) {
      validation(request, response);
      if (i<(args.length - 1)) {
        runner(args, i + 1)
      } else if (doneCallback) {
        doneCallback();
      }
    })
    request.apply(request, args[i])
  }
  runner(args, 0);
}

// Test new module creation

function assertStatus (code) {
  var c = code;
  return function (response, body) {
    if (response.statusCode != c) {
      sys.puts("Status is not "+c+" it is "+response.statusCode+'. '+body)
      throw "Status is not "+c+" it is "+response.statusCode+'. '+body;
    } else {
      sys.puts(body);
    }
  }
}

// request('/foo', "PUT", undefined, {id:"foo", description:"new module"}, function () {sys.puts('done')})

var sha = require('./deps/sha1'),
    base64 = require('./deps/base64');
    
var userDoc = {_id:"org.couchdb.user:testuser", 
               name:"testuser", 
               password_sha: sha.hex_sha1('testing' + 'pepper'), 
               salt:"pepper", type:"user", roles:[]}
var auth = {'Content-Type':'application/json', 
            accept:'application/json', 
            authorization:'Basic ' + base64.encode('testuser:testing'),
            host:"jsregistry:5984"}

requestQueue([
  ["/adduser/org.couchdb.user:testuser", "PUT", undefined, userDoc, assertStatus(201)],
  // ["/session", "POST", undefined, "user=testuser&password=testing", function (response, body) {
  //   sys.puts('s', body, 'd')
  //   auth.cookie = response.headers['set-cookie'];
  // }],
  ["/foo", "PUT", auth, {_id:"foo", description:"new module"}, assertStatus(201)],
  ["/foo/0.1.0", "PUT", auth, 
    {_id:"foo", description:"new module", dist:{tarball:"http://path/to/tarball"}}, assertStatus(201)],
  ["/foo/stable", "PUT", auth, "0.1.0", assertStatus(201)],
  ["/foo", "GET", undefined, "0.1.0", assertStatus(200)],
  ["/foo/0.1.0", "GET", undefined, "0.1.0", assertStatus(200)],
  ["/foo/stable", "GET", undefined, "0.1.0", assertStatus(200)],
  ], function () {sys.puts('done')})
