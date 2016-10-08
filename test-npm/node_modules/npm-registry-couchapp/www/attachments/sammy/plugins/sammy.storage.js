(function($) {

  Sammy = Sammy || {};

  // Sammy.Store is an abstract adapter class that wraps the multitude of in
  // browser data storage into a single common set of methods for storing and
  // retreiving data. The JSON library is used (through the inclusion of the
  // Sammy.JSON) plugin, to automatically convert objects back and forth from
  // stored strings.
  //
  // Sammy.Store can be used directly, but within a Sammy.Application it is much
  // easier to use the `Sammy.Storage` plugin and its helper methods.
  //
  // Sammy.Store also supports the KVO pattern, by firing DOM/jQuery Events when
  // a key is set.
  //
  // ### Example
  //
  //      // create a new store named 'mystore', tied to the #main element, using HTML5 localStorage
  //      // Note: localStorage only works on browsers that support it
  //      var store = new Sammy.Store({name: 'mystore', element: '#element', type: 'local'});
  //      store.set('foo', 'bar');
  //      store.get('foo'); //=> 'bar'
  //      store.set('json', {obj: 'this is an obj'});
  //      store.get('json'); //=> {obj: 'this is an obj'}
  //      store.keys(); //=> ['foo','json']
  //      store.clear('foo');
  //      store.keys(); //=> ['json']
  //      store.clearAll();
  //      store.keys(); //=> []
  //
  // ### Arguments
  //
  // The constructor takes a single argument which is a Object containing these possible options.
  //
  // * `name` The name/namespace of this store. Stores are unique by name/type. (default 'store')
  // * `element` A selector for the element that the store is bound to. (default 'body')
  // * `type` The type of storage/proxy to use (default 'memory')
  //
  // Extra options are passed to the storage constructor.
  // Sammy.Store supports the following methods of storage:
  //
  // * `memory` Basic object storage
  // * `data` jQuery.data DOM Storage
  // * `cookie` Access to document.cookie. Limited to 2K
  // * `local` HTML5 DOM localStorage, browswer support is currently limited.
  // * `session` HTML5 DOM sessionStorage, browswer support is currently limited.
  //
  Sammy.Store = function(options) {
    var store = this;
    this.options  = options || {};
    this.name     = this.options.name || 'store';
    this.element  = this.options.element || 'body';
    this.$element = $(this.element);
    if ($.isArray(this.options.type)) {
      $.each(this.options.type, function(i, type) {
        if (Sammy.Store.isAvailable(type)) {
          store.type = type;
          return false;
        }
      });
    } else {
      this.type = this.options.type || 'memory';
    }
    this.meta_key = this.options.meta_key || '__keys__';
    this.storage  = new Sammy.Store[Sammy.Store.stores[this.type]](this.name, this.element, this.options);
  };

  Sammy.Store.stores = {
    'memory': 'Memory',
    'data': 'Data',
    'local': 'LocalStorage',
    'session': 'SessionStorage',
    'cookie': 'Cookie'
  };

  $.extend(Sammy.Store.prototype, {
    // Checks for the availability of the current storage type in the current browser/config.
    isAvailable: function() {
      if ($.isFunction(this.storage.isAvailable)) {
        return this.storage.isAvailable();
      } else {
        true;
      }
    },
    // Checks for the existance of `key` in the current store. Returns a boolean.
    exists: function(key) {
      return this.storage.exists(key);
    },
    // Sets the value of `key` with `value`. If `value` is an
    // object, it is turned to and stored as a string with `JSON.stringify`.
    // It also tries to conform to the KVO pattern triggering jQuery events on the
    // element that the store is bound to.
    //
    // ### Example
    //
    //      var store = new Sammy.Store({name: 'kvo'});
    //      $('body').bind('set-kvo-foo', function(e, data) {
    //        Sammy.log(data.key + ' changed to ' + data.value);
    //      });
    //      store.set('foo', 'bar'); // logged: foo changed to bar
    //
    set: function(key, value) {
      var string_value = (typeof value == 'string') ? value : JSON.stringify(value);
      key = key.toString();
      this.storage.set(key, string_value);
      if (key != this.meta_key) {
        this._addKey(key);
        this.$element.trigger('set-' + this.name, {key: key, value: value});
        this.$element.trigger('set-' + this.name + '-' + key, {key: key, value: value});
      };
      // always return the original value
      return value;
    },
    // Returns the set value at `key`, parsing with `JSON.parse` and
    // turning into an object if possible
    get: function(key) {
      var value = this.storage.get(key);
      if (typeof value == 'undefined' || value == null || value == '') {
        return value;
      }
      try {
        return JSON.parse(value);
      } catch(e) {
        return value;
      }
    },
    // Removes the value at `key` from the current store
    clear: function(key) {
      this._removeKey(key);
      return this.storage.clear(key);
    },
    // Clears all the values for the current store.
    clearAll: function() {
      var self = this;
      this.each(function(key, value) {
        self.clear(key);
      });
    },
    // Returns the all the keys set for the current store as an array.
    // Internally Sammy.Store keeps this array in a 'meta_key' for easy access.
    keys: function() {
      return this.get(this.meta_key) || [];
    },
    // Iterates over each key value pair passing them to the `callback` function
    //
    // ### Example
    //
    //      store.each(function(key, value) {
    //        Sammy.log('key', key, 'value', value);
    //      });
    //
    each: function(callback) {
      var i = 0,
          keys = this.keys(),
          returned;

      for (i; i < keys.length; i++) {
        returned = callback(keys[i], this.get(keys[i]));
        if (returned === false) { return false; }
      };
    },
    // Filters the store by a filter function that takes a key value.
    // Returns an array of arrays where the first element of each array
    // is the key and the second is the value of that key.
    //
    // ### Example
    //
    //      var store = new Sammy.Store;
    //      store.set('one', 'two');
    //      store.set('two', 'three');
    //      store.set('1', 'two');
    //      var returned = store.filter(function(key, value) {
    //        // only return
    //        return value === 'two';
    //      });
    //      // returned => [['one', 'two'], ['1', 'two']];
    //
    filter: function(callback) {
      var found = [];
      this.each(function(key, value) {
        if (callback(key, value)) {
          found.push([key, value]);
        }
        return true;
      });
      return found;
    },
    // Works exactly like filter except only returns the first matching key
    // value pair instead of all of them
    first: function(callback) {
      var found = false;
      this.each(function(key, value) {
        if (callback(key, value)) {
          found = [key, value];
          return false;
        }
      });
      return found;
    },
    // Returns the value at `key` if set, otherwise, runs the callback
    // and sets the value to the value returned in the callback.
    //
    // ### Example
    //
    //    var store = new Sammy.Store;
    //    store.exists('foo'); //=> false
    //    store.fetch('foo', function() {
    //      return 'bar!';
    //    }); //=> 'bar!'
    //    store.get('foo') //=> 'bar!'
    //    store.fetch('foo', function() {
    //      return 'baz!';
    //    }); //=> 'bar!
    //
    fetch: function(key, callback) {
      if (!this.exists(key)) {
        return this.set(key, callback.apply(this));
      } else {
        return this.get(key);
      }
    },
    // loads the response of a request to `path` into `key`.
    //
    // ### Example
    //
    // In /mytemplate.tpl:
    //
    //    My Template
    //
    // In app.js:
    //
    //    var store = new Sammy.Store;
    //    store.load('mytemplate', '/mytemplate.tpl', function() {
    //      s.get('mytemplate') //=> My Template
    //    });
    //
    load: function(key, path, callback) {
      var s = this;
      $.get(path, function(response) {
        s.set(key, response);
        if (callback) { callback.apply(this, [response]); }
      });
    },

    _addKey: function(key) {
      var keys = this.keys();
      if ($.inArray(key, keys) == -1) { keys.push(key); }
      this.set(this.meta_key, keys);
    },
    _removeKey: function(key) {
      var keys = this.keys();
      var index = $.inArray(key, keys);
      if (index != -1) { keys.splice(index, 1); }
      this.set(this.meta_key, keys);
    }
  });

  // Tests if the type of storage is available/works in the current browser/config.
  // Especially useful for testing the availability of the awesome, but not widely
  // supported HTML5 DOM storage
  Sammy.Store.isAvailable = function(type) {
    try {
      return Sammy.Store[Sammy.Store.stores[type]].prototype.isAvailable();
    } catch(e) {
      return false;
    }
  };

  // Memory ('memory') is the basic/default store. It stores data in a global
  // JS object. Data is lost on refresh.
  Sammy.Store.Memory = function(name, element) {
    this.name  = name;
    this.element = element;
    this.namespace = [this.element, this.name].join('.');
    Sammy.Store.Memory.store = Sammy.Store.Memory.store || {};
    Sammy.Store.Memory.store[this.namespace] = Sammy.Store.Memory.store[this.namespace] || {};
    this.store = Sammy.Store.Memory.store[this.namespace];
  };
  $.extend(Sammy.Store.Memory.prototype, {
    isAvailable: function() { return true; },
    exists: function(key) {
      return (typeof this.store[key] != "undefined");
    },
    set: function(key, value) {
      return this.store[key] = value;
    },
    get: function(key) {
      return this.store[key];
    },
    clear: function(key) {
      delete this.store[key];
    }
  });

  // Data ('data') stores objects using the jQuery.data() methods. This has the advantadge
  // of scoping the data to the specific element. Like the 'memory' store its data
  // will only last for the length of the current request (data is lost on refresh/etc).
  Sammy.Store.Data = function(name, element) {
    this.name = name;
    this.element = element;
    this.$element = $(element);
  };
  $.extend(Sammy.Store.Data.prototype, {
    isAvailable: function() { return true; },
    exists: function(key) {
      return (typeof this.$element.data(this._key(key)) != "undefined");
    },
    set: function(key, value) {
      return this.$element.data(this._key(key), value);
    },
    get: function(key) {
      return this.$element.data(this._key(key));
    },
    clear: function(key) {
      this.$element.removeData(this._key(key));
    },
    _key: function(key) {
      return ['store', this.name, key].join('.');
    }
  });

  // LocalStorage ('local') makes use of HTML5 DOM Storage, and the window.localStorage
  // object. The great advantage of this method is that data will persist beyond
  // the current request. It can be considered a pretty awesome replacement for
  // cookies accessed via JS. The great disadvantage, though, is its only available
  // on the latest and greatest browsers.
  //
  // For more info on DOM Storage:
  // [https://developer.mozilla.org/en/DOM/Storage]
  // [http://www.w3.org/TR/2009/WD-webstorage-20091222/]
  //
  Sammy.Store.LocalStorage = function(name, element) {
    this.name = name;
    this.element = element;
  };
  $.extend(Sammy.Store.LocalStorage.prototype, {
    isAvailable: function() {
      return ('localStorage' in window) && (window.location.protocol != 'file:');
    },
    exists: function(key) {
      return (this.get(key) != null);
    },
    set: function(key, value) {
      return window.localStorage.setItem(this._key(key), value);
    },
    get: function(key) {
      return window.localStorage.getItem(this._key(key));
    },
    clear: function(key) {
      window.localStorage.removeItem(this._key(key));;
    },
    _key: function(key) {
      return ['store', this.element, this.name, key].join('.');
    }
  });

  // .SessionStorage ('session') is similar to LocalStorage (part of the same API)
  // and shares similar browser support/availability. The difference is that
  // SessionStorage is only persistant through the current 'session' which is defined
  // as the length that the current window is open. This means that data will survive
  // refreshes but not close/open or multiple windows/tabs. For more info, check out
  // the `LocalStorage` documentation and links.
  Sammy.Store.SessionStorage = function(name, element) {
    this.name = name;
    this.element = element;
  };
  $.extend(Sammy.Store.SessionStorage.prototype, {
    isAvailable: function() {
      return ('sessionStorage' in window) &&
      (window.location.protocol != 'file:') &&
      ($.isFunction(window.sessionStorage.setItem));
    },
    exists: function(key) {
      return (this.get(key) != null);
    },
    set: function(key, value) {
      return window.sessionStorage.setItem(this._key(key), value);
    },
    get: function(key) {
      var value = window.sessionStorage.getItem(this._key(key));
      if (value && typeof value.value != "undefined") { value = value.value }
      return value;
    },
    clear: function(key) {
      window.sessionStorage.removeItem(this._key(key));;
    },
    _key: function(key) {
      return ['store', this.element, this.name, key].join('.');
    }
  });

  // .Cookie ('cookie') storage uses browser cookies to store data. JavaScript
  // has access to a single document.cookie variable, which is limited to 2Kb in
  // size. Cookies are also considered 'unsecure' as the data can be read easily
  // by other sites/JS. Cookies do have the advantage, though, of being widely
  // supported and persistent through refresh and close/open. Where available,
  // HTML5 DOM Storage like LocalStorage and SessionStorage should be used.
  //
  // .Cookie can also take additional options:
  //
  // * `expires_in` Number of seconds to keep the cookie alive (default 2 weeks).
  // * `path` The path to activate the current cookie for (default '/').
  //
  // For more information about document.cookie, check out the pre-eminint article
  // by ppk: [http://www.quirksmode.org/js/cookies.html]
  //
  Sammy.Store.Cookie = function(name, element, options) {
    this.name = name;
    this.element = element;
    this.options = options || {};
    this.path = this.options.path || '/';
    // set the expires in seconds or default 14 days
    this.expires_in = this.options.expires_in || (14 * 24 * 60 * 60);
  };
  $.extend(Sammy.Store.Cookie.prototype, {
    isAvailable: function() {
      return ('cookie' in document) && (window.location.protocol != 'file:');
    },
    exists: function(key) {
      return (this.get(key) != null);
    },
    set: function(key, value) {
      return this._setCookie(key, value);
    },
    get: function(key) {
      return this._getCookie(key);
    },
    clear: function(key) {
      this._setCookie(key, "", -1);
    },
    _key: function(key) {
      return ['store', this.element, this.name, key].join('.');
    },
    _getCookie: function(key) {
      var escaped = this._key(key).replace(/(\.|\*|\(|\)|\[|\])/g, '\\$1');
      var match = document.cookie.match("(^|;\\s)" + escaped + "=([^;]*)(;|$)")
      return (match ? match[2] : null);
    },
    _setCookie: function(key, value, expires) {
      if (!expires) { expires = (this.expires_in * 1000) }
      var date = new Date();
      date.setTime(date.getTime() + expires);
      var set_cookie = [
        this._key(key), "=", value,
        "; expires=", date.toGMTString(),
        "; path=", this.path
      ].join('');
      document.cookie = set_cookie;
    }
  });

  // Sammy.Storage is a plugin that provides shortcuts for creating and using
  // Sammy.Store objects. Once included it provides the `store()` app level
  // and helper methods. Depends on Sammy.JSON (or json2.js).
  Sammy.Storage = function(app) {
    this.use(Sammy.JSON);

    this.stores = this.stores || {};

    // `store()` creates and looks up existing `Sammy.Store` objects
    // for the current application. The first time used for a given `'name'`
    // initializes a `Sammy.Store` and also creates a helper under the store's
    // name.
    //
    // ### Example
    //
    //      var app = $.sammy(function() {
    //        this.use(Sammy.Storage);
    //
    //        // initializes the store on app creation.
    //        this.store('mystore', {type: 'cookie'});
    //
    //        this.get('#/', function() {
    //          // returns the Sammy.Store object
    //          this.store('mystore');
    //          // sets 'foo' to 'bar' using the shortcut/helper
    //          // equivilent to this.store('mystore').set('foo', 'bar');
    //          this.mystore('foo', 'bar');
    //          // returns 'bar'
    //          // equivilent to this.store('mystore').get('foo');
    //          this.mystore('foo');
    //          // returns 'baz!'
    //          // equivilent to:
    //          // this.store('mystore').fetch('foo!', function() {
    //          //   return 'baz!';
    //          // })
    //          this.mystore('foo!', function() {
    //            return 'baz!';
    //          });
    //
    //          this.clearMystore();
    //          // equivilent to:
    //          // this.store('mystore').clearAll()
    //        });
    //
    //      });
    //
    // ### Arguments
    //
    // * `name` The name of the store and helper. the name must be unique per application.
    // * `options` A JS object of options that can be passed to the Store constuctor on initialization.
    //
    this.store = function(name, options) {
      // if the store has not been initialized
      if (typeof this.stores[name] == 'undefined') {
        // create initialize the store
        var clear_method_name = "clear" + name.substr(0,1).toUpperCase() + name.substr(1);
        this.stores[name] = new Sammy.Store($.extend({
          name: name,
          element: this.element_selector
        }, options || {}));
        // app.name()
        this[name] = function(key, value) {
          if (typeof value == 'undefined') {
            return this.stores[name].get(key);
          } else if ($.isFunction(value)) {
            return this.stores[name].fetch(key, value);
          } else {
            return this.stores[name].set(key, value)
          }
        };
        // app.clearName();
        this[clear_method_name] = function() {
          return this.stores[name].clearAll();
        }
        // context.name()
        this.helper(name, function() {
          return this.app[name].apply(this.app, arguments);
        });
        // context.clearName();
        this.helper(clear_method_name, function() {
          return this.app[clear_method_name]();
        });
      }
      return this.stores[name];
    };

    this.helpers({
      store: function() {
        return this.app.store.apply(this.app, arguments);
      }
    });
  };

  // Sammy.Session is an additional plugin for creating a common 'session' store
  // for the given app. It is a very simple wrapper around `Sammy.Storage`
  // that provides a simple fallback mechanism for trying to provide the best
  // possible storage type for the session. This means, `LocalStorage`
  // if available, otherwise `Cookie`, otherwise `Memory`.
  // It provides the `session()` helper through `Sammy.Storage#store()`.
  //
  // See the `Sammy.Storage` plugin for full documentation.
  //
  Sammy.Session = function(app, options) {
    this.use(Sammy.Storage);
    // check for local storage, then cookie storage, then just use memory
    this.store('session', $.extend({type: ['local', 'cookie', 'memory']}, options));
  };

  // Sammy.Cache provides helpers for caching data within the lifecycle of a
  // Sammy app. The plugin provides two main methods on `Sammy.Application`,
  // `cache` and `clearCache`. Each app has its own cache store so that
  // you dont have to worry about collisions. As of 0.5 the original Sammy.Cache module
  // has been deprecated in favor of this one based on Sammy.Storage. The exposed
  // API is almost identical, but Sammy.Storage provides additional backends including
  // HTML5 Storage. `Sammy.Cache` will try to use these backends when available
  // (in this order) `LocalStorage`, `SessionStorage`, and `Memory`
  Sammy.Cache = function(app, options) {
    this.use(Sammy.Storage);
    // set cache_partials to true
    this.cache_partials = true;
    // check for local storage, then session storage, then just use memory
    this.store('cache', $.extend({type: ['local', 'session', 'memory']}, options));
  };

})(jQuery);
