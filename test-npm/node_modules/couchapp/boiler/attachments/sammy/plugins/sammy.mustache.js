(function($) {

if (!window.Mustache) {

  /*
    mustache.js — Logic-less templates in JavaScript

    See http://mustache.github.com/ for more info.
  */

  var Mustache = function() {
    var Renderer = function() {};

    Renderer.prototype = {
      otag: "{{",
      ctag: "}}",
      pragmas: {},
      buffer: [],
      pragmas_implemented: {
        "IMPLICIT-ITERATOR": true
      },
      context: {},

      render: function(template, context, partials, in_recursion) {
        // reset buffer & set context
        if(!in_recursion) {
          this.context = context;
          this.buffer = []; // TODO: make this non-lazy
        }

        // fail fast
        if(!this.includes("", template)) {
          if(in_recursion) {
            return template;
          } else {
            this.send(template);
            return;
          }
        }

        template = this.render_pragmas(template);
        var html = this.render_section(template, context, partials);
        if(in_recursion) {
          return this.render_tags(html, context, partials, in_recursion);
        }

        this.render_tags(html, context, partials, in_recursion);
      },

      /*
        Sends parsed lines
      */
      send: function(line) {
        if(line != "") {
          this.buffer.push(line);
        }
      },

      /*
        Looks for %PRAGMAS
      */
      render_pragmas: function(template) {
        // no pragmas
        if(!this.includes("%", template)) {
          return template;
        }

        var that = this;
        var regex = new RegExp(this.otag + "%([\\w-]+) ?([\\w]+=[\\w]+)?" +
              this.ctag);
        return template.replace(regex, function(match, pragma, options) {
          if(!that.pragmas_implemented[pragma]) {
            throw({message:
              "This implementation of mustache doesn't understand the '" +
              pragma + "' pragma"});
          }
          that.pragmas[pragma] = {};
          if(options) {
            var opts = options.split("=");
            that.pragmas[pragma][opts[0]] = opts[1];
          }
          return "";
          // ignore unknown pragmas silently
        });
      },

      /*
        Tries to find a partial in the curent scope and render it
      */
      render_partial: function(name, context, partials) {
        name = this.trim(name);
        if(!partials || partials[name] === undefined) {
          throw({message: "unknown_partial '" + name + "'"});
        }
        if(typeof(context[name]) != "object") {
          return this.render(partials[name], context, partials, true);
        }
        return this.render(partials[name], context[name], partials, true);
      },

      /*
        Renders inverted (^) and normal (#) sections
      */
      render_section: function(template, context, partials) {
        if(!this.includes("#", template) && !this.includes("^", template)) {
          return template;
        }

        var that = this;
        // CSW - Added "+?" so it finds the tighest bound, not the widest
        var regex = new RegExp(this.otag + "(\\^|\\#)\\s*(.+)\\s*" + this.ctag +
                "\n*([\\s\\S]+?)" + this.otag + "\\/\\s*\\2\\s*" + this.ctag +
                "\\s*", "mg");

        // for each {{#foo}}{{/foo}} section do...
        return template.replace(regex, function(match, type, name, content) {
          var value = that.find(name, context);
          if(type == "^") { // inverted section
            if(!value || that.is_array(value) && value.length === 0) {
              // false or empty list, render it
              return that.render(content, context, partials, true);
            } else {
              return "";
            }
          } else if(type == "#") { // normal section
            if(that.is_array(value)) { // Enumerable, Let's loop!
              return that.map(value, function(row) {
                return that.render(content, that.create_context(row),
                  partials, true);
              }).join("");
            } else if(that.is_object(value)) { // Object, Use it as subcontext!
              return that.render(content, that.create_context(value),
                partials, true);
            } else if(typeof value === "function") {
              // higher order section
              return value.call(context, content, function(text) {
                return that.render(text, context, partials, true);
              });
            } else if(value) { // boolean section
              return that.render(content, context, partials, true);
            } else {
              return "";
            }
          }
        });
      },

      /*
        Replace {{foo}} and friends with values from our view
      */
      render_tags: function(template, context, partials, in_recursion) {
        // tit for tat
        var that = this;

        var new_regex = function() {
          return new RegExp(that.otag + "(=|!|>|\\{|%)?([^\\/#\\^]+?)\\1?" +
            that.ctag + "+", "g");
        };

        var regex = new_regex();
        var tag_replace_callback = function(match, operator, name) {
          switch(operator) {
          case "!": // ignore comments
            return "";
          case "=": // set new delimiters, rebuild the replace regexp
            that.set_delimiters(name);
            regex = new_regex();
            return "";
          case ">": // render partial
            return that.render_partial(name, context, partials);
          case "{": // the triple mustache is unescaped
            return that.find(name, context);
          default: // escape the value
            return that.escape(that.find(name, context));
          }
        };
        var lines = template.split("\n");
        for(var i = 0; i < lines.length; i++) {
          lines[i] = lines[i].replace(regex, tag_replace_callback, this);
          if(!in_recursion) {
            this.send(lines[i]);
          }
        }

        if(in_recursion) {
          return lines.join("\n");
        }
      },

      set_delimiters: function(delimiters) {
        var dels = delimiters.split(" ");
        this.otag = this.escape_regex(dels[0]);
        this.ctag = this.escape_regex(dels[1]);
      },

      escape_regex: function(text) {
        // thank you Simon Willison
        if(!arguments.callee.sRE) {
          var specials = [
            '/', '.', '*', '+', '?', '|',
            '(', ')', '[', ']', '{', '}', '\\'
          ];
          arguments.callee.sRE = new RegExp(
            '(\\' + specials.join('|\\') + ')', 'g'
          );
        }
        return text.replace(arguments.callee.sRE, '\\$1');
      },

      /*
        find `name` in current `context`. That is find me a value
        from the view object
      */
      find: function(name, context) {
        name = this.trim(name);

        // Checks whether a value is thruthy or false or 0
        function is_kinda_truthy(bool) {
          return bool === false || bool === 0 || bool;
        }

        var value;
        if(is_kinda_truthy(context[name])) {
          value = context[name];
        } else if(is_kinda_truthy(this.context[name])) {
          value = this.context[name];
        }

        if(typeof value === "function") {
          return value.apply(context);
        }
        if(value !== undefined) {
          return value;
        }
        // silently ignore unkown variables
        return "";
      },

      // Utility methods

      /* includes tag */
      includes: function(needle, haystack) {
        return haystack.indexOf(this.otag + needle) != -1;
      },

      /*
        Does away with nasty characters
      */
      escape: function(s) {
        s = String(s === null ? "" : s);
        return s.replace(/&(?!\w+;)|["<>\\]/g, function(s) {
          switch(s) {
          case "&": return "&amp;";
          case "\\": return "\\\\";
          case '"': return '\"';
          case "<": return "&lt;";
          case ">": return "&gt;";
          default: return s;
          }
        });
      },

      // by @langalex, support for arrays of strings
      create_context: function(_context) {
        if(this.is_object(_context)) {
          return _context;
        } else {
          var iterator = ".";
          if(this.pragmas["IMPLICIT-ITERATOR"]) {
            iterator = this.pragmas["IMPLICIT-ITERATOR"].iterator;
          }
          var ctx = {};
          ctx[iterator] = _context;
          return ctx;
        }
      },

      is_object: function(a) {
        return a && typeof a == "object";
      },

      is_array: function(a) {
        return Object.prototype.toString.call(a) === '[object Array]';
      },

      /*
        Gets rid of leading and trailing whitespace
      */
      trim: function(s) {
        return s.replace(/^\s*|\s*$/g, "");
      },

      /*
        Why, why, why? Because IE. Cry, cry cry.
      */
      map: function(array, fn) {
        if (typeof array.map == "function") {
          return array.map(fn);
        } else {
          var r = [];
          var l = array.length;
          for(var i = 0; i < l; i++) {
            r.push(fn(array[i]));
          }
          return r;
        }
      }
    };

    return({
      name: "mustache.js",
      version: "0.3.1-dev",

      /*
        Turns a template and view into HTML
      */
      to_html: function(template, view, partials, send_fun) {
        var renderer = new Renderer();
        if(send_fun) {
          renderer.send = send_fun;
        }
        renderer.render(template, view, partials);
        if(!send_fun) {
          return renderer.buffer.join("\n");
        }
      }
    });
  }();

} // Ensure Mustache

  Sammy = Sammy || {};

  // <tt>Sammy.Mustache</tt> provides a quick way of using mustache style templates in your app.
  // The plugin itself includes the awesome mustache.js lib created and maintained by Jan Lehnardt
  // at http://github.com/janl/mustache.js
  //
  // Mustache is a clever templating system that relys on double brackets {{}} for interpolation.
  // For full details on syntax check out the original Ruby implementation created by Chris Wanstrath at
  // http://github.com/defunkt/mustache
  //
  // By default using Sammy.Mustache in your app adds the <tt>mustache()</tt> method to the EventContext
  // prototype. However, just like <tt>Sammy.Template</tt> you can change the default name of the method
  // by passing a second argument (e.g. you could use the ms() as the method alias so that all the template
  // files could be in the form file.ms instead of file.mustache)
  //
  // ### Example #1
  //
  // The template (mytemplate.ms):
  //
  //       <h1>\{\{title\}\}<h1>
  //
  //       Hey, {{name}}! Welcome to Mustache!
  //
  // The app:
  //
  //       var $.app = $.sammy(function() {
  //         // include the plugin and alias mustache() to ms()
  //         this.use(Sammy.Mustache, 'ms');
  //
  //         this.get('#/hello/:name', function() {
  //           // set local vars
  //           this.title = 'Hello!'
  //           this.name = this.params.name;
  //           // render the template and pass it through mustache
  //           this.partial('mytemplate.ms');
  //         });
  //
  //       });
  //
  // If I go to #/hello/AQ in the browser, Sammy will render this to the <tt>body</tt>:
  //
  //       <h1>Hello!</h1>
  //
  //       Hey, AQ! Welcome to Mustache!
  //
  //
  // ### Example #2 - Mustache partials
  //
  // The template (mytemplate.ms)
  //
  //       Hey, {{name}}! {{>hello_friend}}
  //
  //
  // The partial (mypartial.ms)
  //
  //       Say hello to your friend {{friend}}!
  //
  // The app:
  //
  //       var $.app = $.sammy(function() {
  //         // include the plugin and alias mustache() to ms()
  //         this.use(Sammy.Mustache, 'ms');
  //
  //         this.get('#/hello/:name/to/:friend', function() {
  //           var context = this;
  //
  //           // fetch mustache-partial first
  //           $.get('mypartial.ms', function(response){
  //             context.partials = response;
  //
  //             // set local vars
  //             context.name = this.params.name;
  //             context.hello_friend = {name: this.params.friend};
  //
  //             // render the template and pass it through mustache
  //             context.partial('mytemplate.ms');
  //           });
  //         });
  //
  //       });
  //
  // If I go to #/hello/AQ/to/dP in the browser, Sammy will render this to the <tt>body</tt>:
  //
  //       Hey, AQ! Say hello to your friend dP!
  //
  // Note: You dont have to include the mustache.js file on top of the plugin as the plugin
  // includes the full source.
  //
  Sammy.Mustache = function(app, method_alias) {

    // *Helper* Uses Mustache.js to parse a template and interpolate and work with the passed data
    //
    // ### Arguments
    //
    // * `template` A String template. {{}} Tags are evaluated and interpolated by Mustache.js
    // * `data` An Object containing the replacement values for the template.
    //   data is extended with the <tt>EventContext</tt> allowing you to call its methods within the template.
    // * `partials` An Object containing one or more partials (String templates
    //   that are called from the main template).
    //
    var mustache = function(template, data, partials) {
      data     = $.extend({}, this, data);
      partials = $.extend({}, data.partials, partials);
      return Mustache.to_html(template, data, partials);
    };

    // set the default method name/extension
    if (!method_alias) method_alias = 'mustache';
    app.helper(method_alias, mustache);

  };

})(jQuery);
