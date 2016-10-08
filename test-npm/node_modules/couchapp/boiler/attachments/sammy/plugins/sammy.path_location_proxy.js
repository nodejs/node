(function($) {

  Sammy = Sammy || {};

  // `Sammy.PathLocationProxy` is a simple Location Proxy that just
  // gets and sets window.location. This allows you to use
  // Sammy to route on the full URL path instead of just the hash. It
  // will take a full refresh to get the app to change state.
  //
  // To read more about location proxies, check out the
  // documentation for `Sammy.HashLocationProxy`
  Sammy.PathLocationProxy = function(app) {
    this.app = app;
  };

  Sammy.PathLocationProxy.prototype = {
    bind: function() {},
    unbind: function() {},

    getLocation: function() {
      return [window.location.pathname, window.location.search].join('');
    },

    setLocation: function(new_location) {
      return window.location = new_location;
    }
  };

})(jQuery);
