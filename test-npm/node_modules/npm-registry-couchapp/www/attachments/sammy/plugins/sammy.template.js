(function($) {

  // Simple JavaScript Templating
  // John Resig - http://ejohn.org/ - MIT Licensed
  // adapted from: http://ejohn.org/blog/javascript-micro-templating/
  // originally $.srender by Greg Borenstein http://ideasfordozens.com in Feb 2009
  // modified for Sammy by Aaron Quint for caching templates by name
  var srender_cache = {};
  var srender = function(name, template, data) {
    // target is an optional element; if provided, the result will be inserted into it
    // otherwise the result will simply be returned to the caller
    if (srender_cache[name]) {
      fn = srender_cache[name];
    } else {
      if (typeof template == 'undefined') {
        // was a cache check, return false
        return false;
      }
      // Generate a reusable function that will serve as a template
      // generator (and which will be cached).
      fn = srender_cache[name] = new Function("obj",
      "var p=[],print=function(){p.push.apply(p,arguments);};" +

      // Introduce the data as local variables using with(){}
      "with(obj){p.push(\"" +

      // Convert the template into pure JavaScript
      template
        .replace(/[\r\t\n]/g, " ")
        .replace(/\"/g, '\\"')
        .split("<%").join("\t")
        .replace(/((^|%>)[^\t]*)/g, "$1\r")
        .replace(/\t=(.*?)%>/g, "\",$1,\"")
        .split("\t").join("\");")
        .split("%>").join("p.push(\"")
        .split("\r").join("")
        + "\");}return p.join('');");
    }

    if (typeof data != 'undefined') {
      return fn(data);
    } else {
      return fn;
    }
  };

  Sammy = Sammy || {};

  // <tt>Sammy.Template</tt> is a simple plugin that provides a way to create
  // and render client side templates. The rendering code is based on John Resig's
  // quick templates and Greg Borenstien's srender plugin.
  // This is also a great template/boilerplate for Sammy plugins.
  //
  // Templates use <% %> tags to denote embedded javascript.
  //
  // ### Examples
  //
  // Here is an example template (user.template):
  //
  //      <div class="user">
  //        <div class="user-name"><%= user.name %></div>
  //        <% if (user.photo_url) { %>
  //          <div class="photo"><img src="<%= user.photo_url %>" /></div>
  //        <% } %>
  //      </div>
  //
  // Given that is a publicly accesible file, you would render it like:
  //
  //       $.sammy(function() {
  //         // include the plugin
  //         this.use(Sammy.Template);
  //
  //         this.get('#/', function() {
  //           // the template is rendered in the current context.
  //           this.user = {name: 'Aaron Quint'};
  //           // partial calls template() because of the file extension
  //           this.partial('user.template');
  //         })
  //       });
  //
  // You can also pass a second argument to use() that will alias the template
  // method and therefore allow you to use a different extension for template files
  // in <tt>partial()</tt>
  //
  //      // alias to 'tpl'
  //      this.use(Sammy.Template, 'tpl');
  //
  //      // now .tpl files will be run through srender
  //      this.get('#/', function() {
  //        this.partial('myfile.tpl');
  //      });
  //
  Sammy.Template = function(app, method_alias) {

    // *Helper:* Uses simple templating to parse ERB like templates.
    //
    // ### Arguments
    //
    // * `template` A String template. '<% %>' tags are evaluated as Javascript and replaced with the elements in data.
    // * `data` An Object containing the replacement values for the template.
    //   data is extended with the <tt>EventContext</tt> allowing you to call its methods within the template.
    // * `name` An optional String name to cache the template.
    //
    var template = function(template, data, name) {
      // use name for caching
      if (typeof name == 'undefined') name = template;
      return srender(name, template, $.extend({}, this, data));
    };

    // set the default method name/extension
    if (!method_alias) method_alias = 'template';
    // create the helper at the method alias
    app.helper(method_alias, template);

  };

})(jQuery);
