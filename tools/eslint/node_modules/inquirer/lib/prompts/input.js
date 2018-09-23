/**
 * `input` type prompt
 */

var _ = require("lodash");
var util = require("util");
var chalk = require("chalk");
var Base = require("./base");
var utils = require("../utils/utils");
var observe = require("../utils/events");


/**
 * Module exports
 */

module.exports = Prompt;


/**
 * Constructor
 */

function Prompt() {
  return Base.apply( this, arguments );
}
util.inherits( Prompt, Base );


/**
 * Start the Inquiry session
 * @param  {Function} cb      Callback when prompt is done
 * @return {this}
 */

Prompt.prototype._run = function( cb ) {
  this.done = cb;

  // Once user confirm (enter key)
  var events = observe(this.rl);
  var submit = events.line.map( this.filterInput.bind(this) );

  var validation = this.handleSubmitEvents( submit );
  validation.success.forEach( this.onEnd.bind(this) );
  validation.error.forEach( this.onError.bind(this) );

  events.keypress.takeUntil( validation.success ).forEach( this.onKeypress.bind(this) );

  // Init
  this.render();

  return this;
};


/**
 * Render the prompt to screen
 * @return {Prompt} self
 */

Prompt.prototype.render = function() {
  var message = this.getQuestion();
  utils.writeMessage( this, message );

  return this;
};


/**
 * When user press `enter` key
 */

Prompt.prototype.filterInput = function( input ) {
  if ( !input ) {
    return this.opt.default != null ? this.opt.default : "";
  }
  return input;
};

Prompt.prototype.onEnd = function( state ) {
  this.filter( state.value, function( filteredValue ) {
    this.status = "answered";

    // Re-render prompt
    this.clean(1).render();

    // Render answer
    this.write( chalk.cyan(filteredValue) + "\n" );

    this.done( state.value );
  }.bind(this));
};

Prompt.prototype.onError = function( state ) {
  this.error( state.isValid ).clean().render();
};

/**
 * When user press a key
 */

Prompt.prototype.onKeypress = function() {
  this.cacheCursorPos();
  this.clean().render().write( this.rl.line );
  this.restoreCursorPos();
};
