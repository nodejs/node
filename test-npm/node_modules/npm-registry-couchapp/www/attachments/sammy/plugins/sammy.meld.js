(function($) {

  Sammy = Sammy || {};

  // `Sammy.Meld` is a simple templating engine that uses the power of jQuery's
  // DOM manipulation to easily meld JSON data and HTML templates very quickly.
  //
  // The template can either be a string (i.e. loaded from a remote template)
  // or a DOM Element/jQuery object. This allows you to have templates be DOM
  // elements as the initial document load.
  //
  // ### Example
  //
  // The simplest case is a nested `<div>` whose class name is tied to a
  // property of a JS object.
  //
  // Template:
  //
  //        <div class="post">
  //          <div class="title"></div>
  //          <div class="entry"></div>
  //          <div class="author">
  //            <span class="name"></span>
  //          </div>
  //        </div>
  //
  // Data:
  //
  //        {
  //          "post": {
  //            "title": "My Post",
  //            "entry": "My Entry",
  //            "author": {
  //              "name": "@aq"
  //            }
  //          }
  //        }
  //
  // Result:
  //
  //        <div class="post">
  //          <div class="title">My Post</div>
  //          <div class="entry">My Entry</div>
  //          <div class="author">
  //            <span class="name">@aq</span>
  //          </div>
  //        </div>
  //
  // Templates can be much more complex, and more deeply nested.
  // More examples can be found in `test/fixtures/meld/`
  //
  // If you don't think the lookup by classes is semantic for you, you can easily
  // switch the method of lookup by defining a selector function in the options
  //
  // For example:
  //
  //      meld($('.post'), post_data, {
  //        selector: function(k) {
  //          return '[data-key=' + k + ']';
  //        }
  //      });
  //
  // Would look for template nodes like `<div data-key='entry'>`
  //
  Sammy.Meld = function(app, method_alias) {
    var default_options = {
      selector: function(k) { return '.' + k; },
      remove_false: true
    };

    var meld = function(template, data, options) {
      var $template = $(template);

      options = $.extend(default_options, options || {});

      if (typeof data === 'string') {
        $template.html(data);
      } else {
        $.each(data, function(key, value) {
          var selector = options.selector(key),
              $sub = $template.filter(selector),
              $container,
              $item,
              is_list = false,
              subindex = $template.index($sub);

          if ($sub.length === 0) { $sub = $template.find(selector); }
          if ($sub.length > 0) {
            if ($.isArray(value)) {
              $container = $('<div/>');
              if ($sub.is('ol, ul')) {
                is_list = true;
                $item   = $sub.children('li:first');
                if ($item.length == 0) { $item = $('<li/>'); }
              } else if ($sub.children().length == 1) {
                is_list = true;
                $item = $sub.children(':first').clone();
              } else {
                $item = $sub.clone();
              }
              for (var i = 0; i < value.length; i++) {
                $container.append(meld($item.clone(), value[i], options));
              }
              if (is_list) {
                $sub.html($container.html());
              } else if ($sub[0] == $template[0]) {
                $template = $($container.html());
              } else if (subindex >= 0) {
                var args = [subindex, 1];
                args = args.concat($container.children().get());
                $template.splice.apply($template, args);
              }
            } else if (options.remove_false && value === false) {
              $template.splice(subindex, 1);
            } else if (typeof value === 'object') {
              if ($sub.is(':empty')) {
                $sub.attr(value, true);
              } else {
                $sub.html(meld($sub.html(), value, options));
              }
            } else {
              $sub.html(value.toString());
            }
          } else {
            $template.attr({key: value}, true);
          }
        });
      }
      var dom = $template;
      return dom;
    };

    // set the default method name/extension
    if (!method_alias) method_alias = 'meld';
    // create the helper at the method alias
    app.helper(method_alias, meld);

  };

})(jQuery);
