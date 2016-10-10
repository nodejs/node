(function($) {

  Sammy = Sammy || {};

  // The DataLocationProxy is an optional location proxy prototype. As opposed to
  // the `HashLocationProxy` it gets its location from a jQuery.data attribute
  // tied to the application's element. You can set the name of the attribute by
  // passing a string as the second argument to the constructor. The default attribute
  // name is 'sammy-location'. To read more about location proxies, check out the
  // documentation for `Sammy.HashLocationProxy`
  //
  // An optional `href_attribute` can be passed, which specifies a DOM element
  // attribute that holds "links" to different locations in the app. When the
  // proxy is bound, clicks to element that have this attribute activate a
  // `setLocation()` using the contents of the `href_attribute`.
  //
  // ### Example
  //
  //      var app = $.sammy(function() {
  //        // set up the location proxy
  //        this.setLocationProxy(new Sammy.DataLocationProxy(this, 'location', 'rel'));
  //
  //        this.get('about', function() {
  //          this.partial('about.html');
  //        });
  //
  //      });
  //
  // In this scenario, if an element existed within the template:
  //
  //      <a href="/about" rel="about">About Us</a>
  //
  // Clicking on that link would not go to /about, but would set the apps location
  // to 'about' and trigger the route.
  Sammy.DataLocationProxy = function(app, data_name, href_attribute) {
    this.app            = app;
    this.data_name      = data_name || 'sammy-location';
    this.href_attribute = href_attribute;
  };

  Sammy.DataLocationProxy.prototype = {
    bind: function() {
      var proxy = this;
      this.app.$element().bind('setData', function(e, key, value) {
        if (key == proxy.data_name) {
          // jQuery unfortunately fires the event before it sets the value
          // work around it, by setting the value ourselves
          proxy.app.$element().each(function() {
            $.data(this, proxy.data_name, value);
          });
          proxy.app.trigger('location-changed');
        }
      });
      if (this.href_attribute) {
        this.app.$element().delegate('[' + this.href_attribute + ']', 'click', function(e) {
          e.preventDefault();
          proxy.setLocation($(this).attr(proxy.href_attribute));
        });
      }
    },

    unbind: function() {
      if (this.href_attribute) {
        this.app.$element().undelegate('[' + this.href_attribute + ']', 'click');
      }
      this.app.$element().unbind('setData');
    },

    getLocation: function() {
      return this.app.$element().data(this.data_name);
    },

    setLocation: function(new_location) {
      return this.app.$element().data(this.data_name, new_location);
    }
  };

})(jQuery);
