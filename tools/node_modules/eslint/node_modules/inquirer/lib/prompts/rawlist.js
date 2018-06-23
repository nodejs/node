'use strict';
/**
 * `rawlist` type prompt
 */

var _ = require('lodash');
var chalk = require('chalk');
var Base = require('./base');
var Separator = require('../objects/separator');
var observe = require('../utils/events');
var Paginator = require('../utils/paginator');

class RawListPrompt extends Base {
  constructor(questions, rl, answers) {
    super(questions, rl, answers);

    if (!this.opt.choices) {
      this.throwParamError('choices');
    }

    this.opt.validChoices = this.opt.choices.filter(Separator.exclude);

    this.selected = 0;
    this.rawDefault = 0;

    _.extend(this.opt, {
      validate: function(val) {
        return val != null;
      }
    });

    var def = this.opt.default;
    if (_.isNumber(def) && def >= 0 && def < this.opt.choices.realLength) {
      this.selected = def;
      this.rawDefault = def;
    } else if (!_.isNumber(def) && def != null) {
      let index = _.findIndex(this.opt.choices.realChoices, ({ value }) => value === def);
      let safeIndex = Math.max(index, 0);
      this.selected = safeIndex;
      this.rawDefault = safeIndex;
    }

    // Make sure no default is set (so it won't be printed)
    this.opt.default = null;

    this.paginator = new Paginator();
  }

  /**
   * Start the Inquiry session
   * @param  {Function} cb      Callback when prompt is done
   * @return {this}
   */

  _run(cb) {
    this.done = cb;

    // Once user confirm (enter key)
    var events = observe(this.rl);
    var submit = events.line.map(this.getCurrentValue.bind(this));

    var validation = this.handleSubmitEvents(submit);
    validation.success.forEach(this.onEnd.bind(this));
    validation.error.forEach(this.onError.bind(this));

    events.keypress.takeUntil(validation.success).forEach(this.onKeypress.bind(this));

    // Init the prompt
    this.render();

    return this;
  }

  /**
   * Render the prompt to screen
   * @return {RawListPrompt} self
   */

  render(error) {
    // Render question
    var message = this.getQuestion();
    var bottomContent = '';

    if (this.status === 'answered') {
      message += chalk.cyan(this.answer);
    } else {
      var choicesStr = renderChoices(this.opt.choices, this.selected);
      message += this.paginator.paginate(choicesStr, this.selected, this.opt.pageSize);
      message += '\n  Answer: ';
    }

    message += this.rl.line;

    if (error) {
      bottomContent = '\n' + chalk.red('>> ') + error;
    }

    this.screen.render(message, bottomContent);
  }

  /**
   * When user press `enter` key
   */

  getCurrentValue(index) {
    if (index == null || index === '') {
      index = this.rawDefault;
    } else {
      index -= 1;
    }

    var choice = this.opt.choices.getChoice(index);
    return choice ? choice.value : null;
  }

  onEnd(state) {
    this.status = 'answered';
    this.answer = state.value;

    // Re-render prompt
    this.render();

    this.screen.done();
    this.done(state.value);
  }

  onError() {
    this.render('Please enter a valid index');
  }

  /**
   * When user press a key
   */

  onKeypress() {
    var index = this.rl.line.length ? Number(this.rl.line) - 1 : 0;

    if (this.opt.choices.getChoice(index)) {
      this.selected = index;
    } else {
      this.selected = undefined;
    }

    this.render();
  }
}

/**
 * Function for rendering list choices
 * @param  {Number} pointer Position of the pointer
 * @return {String}         Rendered content
 */

function renderChoices(choices, pointer) {
  var output = '';
  var separatorOffset = 0;

  choices.forEach(function(choice, i) {
    output += '\n  ';

    if (choice.type === 'separator') {
      separatorOffset++;
      output += ' ' + choice;
      return;
    }

    var index = i - separatorOffset;
    var display = index + 1 + ') ' + choice.name;
    if (index === pointer) {
      display = chalk.cyan(display);
    }
    output += display;
  });

  return output;
}

module.exports = RawListPrompt;
