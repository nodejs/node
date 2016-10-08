(function($) {

  Sammy = Sammy || {};

  // Sammy.Title is a very simple plugin to easily set the document's title.
  // It supplies a helper for setting the title (`title()`) within routes,
  // and an app level method for setting the global title (`setTitle()`)
  Sammy.Title = function() {

    // setTitle allows setting a global title or a function that modifies the
    // title for each route/page.
    //
    // ### Example
    //
    //    // setting a title prefix
    //    $.sammy(function() {
    //
    //      this.setTitle('My App -');
    //
    //      this.get('#/', function() {
    //        this.title('Home'); // document's title == "My App - Home"
    //      });
    //    });
    //
    //    // setting a title with a function
    //    $.sammy(function() {
    //
    //      this.setTitle(function(title) {
    //        return [title, " /// My App"].join('');
    //      });
    //
    //      this.get('#/', function() {
    //        this.title('Home'); // document's title == "Home /// My App";
    //      });
    //    });
    //
    this.setTitle = function(title) {
      if (!$.isFunction(title)) {
        this.title_function = function(additional_title) {
          return [title, additional_title].join(' ');
        }
      } else {
        this.title_function = title;
      }
    };

    // *Helper* title() sets the document title, passing it through the function
    // defined by setTitle() if set.
    this.helper('title', function() {
      var new_title = $.makeArray(arguments).join(' ');
      if (this.app.title_function) {
        new_title = this.app.title_function(new_title);
      }
      document.title = new_title;
    });

  };

})(jQuery);
