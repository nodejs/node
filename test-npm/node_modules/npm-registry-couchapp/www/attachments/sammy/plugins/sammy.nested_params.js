(function($) {

  Sammy = Sammy || {};

  function parseValue(value) {
    value = unescape(value);
    if (value === "true") {
      return true;
    } else if (value === "false") {
      return false;
    } else {
      return value;
    }
  };

  function parseNestedParam(params, field_name, field_value) {
    var match, name, rest;

    if (field_name.match(/^[^\[]+$/)) {
      // basic value
      params[field_name] = parseValue(field_value);
    } else if (match = field_name.match(/^([^\[]+)\[\](.*)$/)) {
      // array
      name = match[1];
      rest = match[2];

      if(params[name] && !$.isArray(params[name])) { throw('400 Bad Request'); }

      if (rest) {
        // array is not at the end of the parameter string
        match = rest.match(/^\[([^\]]+)\](.*)$/);
        if(!match) { throw('400 Bad Request'); }

        if (params[name]) {
          if(params[name][params[name].length - 1][match[1]]) {
            params[name].push(parseNestedParam({}, match[1] + match[2], field_value));
          } else {
            $.extend(true, params[name][params[name].length - 1], parseNestedParam({}, match[1] + match[2], field_value));
          }
        } else {
          params[name] = [parseNestedParam({}, match[1] + match[2], field_value)];
        }
      } else {
        // array is at the end of the parameter string
        if (params[name]) {
          params[name].push(parseValue(field_value));
        } else {
          params[name] = [parseValue(field_value)];
        }
      }
    } else if (match = field_name.match(/^([^\[]+)\[([^\[]+)\](.*)$/)) {
      // hash
      name = match[1];
      rest = match[2] + match[3];

      if (params[name] && $.isArray(params[name])) { throw('400 Bad Request'); }

      if (params[name]) {
        $.extend(true, params[name], parseNestedParam(params[name], rest, field_value));
      } else {
        params[name] = parseNestedParam({}, rest, field_value);
      }
    }
    return params;
  };

  // <tt>Sammy.NestedParams</tt> overrides the default form parsing behavior to provide
  // extended functionality for parsing Rack/Rails style form name/value pairs into JS
  // Objects. In fact it passes the same suite of tests as Rack's nested query parsing.
  // The code and tests were ported to JavaScript/Sammy by http://github.com/endor
  //
  // This allows you to translate a form with properly named inputs into a JSON object.
  //
  // ### Example
  //
  // Given an HTML form like so:
  //
  //     <form action="#/parse_me" method="post">
  //       <input type="text" name="obj[first]" />
  //       <input type="text" name="obj[second]" />
  //       <input type="text" name="obj[hash][first]" />
  //       <input type="text" name="obj[hash][second]" />
  //     </form>
  //
  // And a Sammy app like:
  //
  //     var app = $.sammy(function(app) {
  //       this.use(Sammy.NestedParams);
  //
  //       this.post('#/parse_me', function(context) {
  //         $.log(this.params);
  //       });
  //     });
  //
  // If you filled out the form with some values and submitted it, you would see something
  // like this in your log:
  //
  //     {
  //       'obj': {
  //         'first': 'value',
  //         'second': 'value',
  //         'hash': {
  //           'first': 'value',
  //           'second': 'value'
  //         }
  //       }
  //     }
  //
  // It supports creating arrays with [] and other niceities. Check out the tests for
  // full specs.
  //
  Sammy.NestedParams = function(app) {

    app._parseParamPair = parseNestedParam;

  };

})(jQuery);
