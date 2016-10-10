var util = require('util');

var LogDriver = function(options){
  options = options || {};
  var logger = this;
  if (options.format){
    this.format = options.format;
  }
  this.levels = options.levels || ['error', 'warn', 'info', 'debug', 'trace'];
  if (options.level === false){
    this.level = false;  // don't log anything
  } else {
    this.level = options.level || this.levels[this.levels.length - 1];
    if (this.levels.indexOf(this.level) === -1){
      throw new Error("Log level '" +
                      this.level +
                      "' does not exist in level list '" + JSON.stringify() + "'");
    }
  }
  this.levels.forEach(function(level){
    if (logLevelShouldOutput(level, logger.level, logger.levels)){
      logger[level] = function(){
        var args = Array.prototype.slice.call(arguments);
        args.unshift(level);  // log level is added as the first parameter
        console.log(logger.format.apply(logger, args));
      };
    } else {
      logger[level] = function(){/* no-op, because this log level is ignored */};
    }
  });
};

var logLevelShouldOutput = function(logLevel, configuredLevel, levels){
  if (configuredLevel === false){
    return false;
  }
  return (levels.indexOf(logLevel) <= levels.indexOf(configuredLevel));
};

LogDriver.prototype.format = function(){
  var args = Array.prototype.slice.call(arguments, [1]); // change arguments to an array, but
                                                         // drop the first item (log level)
  var out = "[" + arguments[0] + "] " + JSON.stringify(new Date()) + " ";
  args.forEach(function(arg){
    out += " " + util.inspect(arg);
  });
  return out;
};

var defaultLogger = null;

var factory = function(options){
  defaultLogger = new LogDriver(options);
  factory.logger = defaultLogger;
  return defaultLogger;
};

factory();

module.exports = factory;
