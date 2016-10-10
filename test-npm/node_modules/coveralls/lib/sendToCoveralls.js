var request = require('request');
var index = require('../index');

var sendToCoveralls = function(obj, cb){
  var urlBase = 'https://coveralls.io';
  if (process.env.COVERALLS_ENDPOINT) {
    urlBase = process.env.COVERALLS_ENDPOINT;
  }

  var str = JSON.stringify(obj);
  var url = urlBase + '/api/v1/jobs';
  
  if (index.options.stdout) {
    process.stdout.write(str);
    cb(null, { statusCode: 200 }, '');
  } else {
    request.post({url : url, form : { json : str}}, function(err, response, body){
      cb(err, response, body);
    });
  }
};

module.exports = sendToCoveralls;
