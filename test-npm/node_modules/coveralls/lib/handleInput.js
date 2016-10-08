var index = require('../index');
var logger = require('./logger')();

function handleInput(input, cb, userOptions) {
  logger.debug(input);
  logger.debug('user options ' + userOptions);
	index.getOptions(function(err, options){

    if (err){
      logger.error("error from getOptions");
      cb(err);
      return;
    }
    logger.debug(options);

    index.convertLcovToCoveralls(input, options, function(err, postData){
      if (err){
        logger.error("error from convertLcovToCoveralls");
        cb(err);
        return;
      }
      logger.info("sending this to coveralls.io: ", JSON.stringify(postData));
      index.sendToCoveralls(postData, function(err, response, body){
        if (err){
          cb(err);
          return;
        }
        if (response.statusCode >= 400){
          cb("Bad response: " + response.statusCode + " " + body);
          return;
        }
        logger.debug(response.statusCode);
        logger.debug(body);
        cb(null);
      });
    });
  }, userOptions);
}

module.exports = handleInput;
