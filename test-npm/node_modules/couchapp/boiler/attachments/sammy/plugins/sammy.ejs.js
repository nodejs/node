(function($) {

  // Embeddedjs is property of http://embeddedjs.com/
  // Sammy ejs plugin written Codeofficer @ http://www.codeofficer.com/

  var rsplit = function(string, regex) {
    var result = regex.exec(string),
        retArr = new Array(),
        first_idx, last_idx, first_bit;
    while (result != null) {
      first_idx = result.index;
      last_idx = regex.lastIndex;
      if ((first_idx) != 0) {
        first_bit = string.substring(0, first_idx);
        retArr.push(string.substring(0, first_idx));
        string = string.slice(first_idx);
      };
      retArr.push(result[0]);
      string = string.slice(result[0].length);
      result = regex.exec(string);
    };
    if (! string == '') {
      retArr.push(string);
    };
    return retArr;
  };

  var chop = function(string) {
    return string.substr(0, string.length - 1);
  };

  var extend = function(d, s) {
    for (var n in s) {
      if(s.hasOwnProperty(n))   d[n] = s[n];
    };
  };

  /* @Constructor*/
  EJS = function(options) {
    options = (typeof(options) == "string") ? {view: options} : options;
    this.set_options(options);
    if (options.precompiled) {
      this.template = {};
      this.template.process = options.precompiled;
      EJS.update(this.name, this);
      return;
    };
    if (options.element) {
      if (typeof(options.element) == 'string') {
        var name = options.element;
        options.element = document.getElementById(options.element);
        if (options.element == null) throw name + 'does not exist!';
      };
      if (options.element.value) {
        this.text = options.element.value;
      } else {
        this.text = options.element.innerHTML;
      };
      this.name = options.element.id;
      this.type = '[';
    } else if (options.url) {
      options.url = EJS.endExt(options.url, this.extMatch);
      this.name = this.name ? this.name : options.url;
      var url = options.url;
      //options.view = options.absolute_url || options.view || options.;
      var template = EJS.get(this.name /*url*/, this.cache);
      if (template) return template;
      if (template == EJS.INVALID_PATH) return null;
      try {
        this.text = EJS.request( url + (this.cache ? '' : '?' + Math.random() ));
      } catch(e) {};
      if (this.text == null) {
        throw( {type: 'EJS', message: 'There is no template at '+url}  );
      };
      //this.name = url;
    };
    var template = new EJS.Compiler(this.text, this.type);
    template.compile(options, this.name);
    EJS.update(this.name, this);
    this.template = template;
  };

  /* @Prototype*/
  EJS.prototype = {
  /**
   * Renders an object with extra view helpers attached to the view.
   * @param {Object} object data to be rendered
   * @param {Object} extra_helpers an object with additonal view helpers
   * @return {String} returns the result of the string
   */
    render : function(object, extra_helpers) {
      object = object || {};
      this._extra_helpers = extra_helpers;
      var v = new EJS.Helpers(object, extra_helpers || {});
      return this.template.process.call(object, object,v);
    },

    update : function(element, options) {
      if (typeof(element) == 'string') {
        element = document.getElementById(element);
      };
      if (options == null) {
        _template = this;
        return function(object) {
          EJS.prototype.update.call(_template, element, object);
        };
      };
      if (typeof(options) == 'string') {
        params = {};
        params.url = options;
        _template = this;
        params.onComplete = function(request) {
          var object = eval(request.responseText);
          EJS.prototype.update.call(_template, element, object);
        };
        EJS.ajax_request(params);
      } else {
        element.innerHTML = this.render(options);
      };
    },

    out : function() {
      return this.template.out;
    },

    /**
     * Sets options on this view to be rendered with.
     * @param {Object} options
     */
    set_options : function(options){
      this.type = options.type || EJS.type;
      this.cache = (options.cache != null) ? options.cache : EJS.cache;
      this.text = options.text || null;
      this.name =   options.name || null;
      this.ext = options.ext || EJS.ext;
      this.extMatch = new RegExp(this.ext.replace(/\./, '\.'));
    }
  };

  EJS.endExt = function(path, match) {
    if (!path) return null;
    match.lastIndex = 0;
    return path + (match.test(path) ? '' : this.ext);
  };

  /* @Static*/
  EJS.Scanner = function(source, left, right) {
    extend(this, {
      left_delimiter:  left +'%',
      right_delimiter: '%'+right,
      double_left: left+'%%',
      double_right: '%%'+right,
      left_equal: left+'%=',
      left_comment: left+'%#'
    });
    this.SplitRegexp = (left == '[') ? /(\[%%)|(%%\])|(\[%=)|(\[%#)|(\[%)|(%\]\n)|(%\])|(\n)/ : new RegExp('('+this.double_left+')|(%%'+this.double_right+')|('+this.left_equal+')|('+this.left_comment+')|('+this.left_delimiter+')|('+this.right_delimiter+'\n)|('+this.right_delimiter+')|(\n)');
    this.source = source;
    this.stag = null;
    this.lines = 0;
  };

  EJS.Scanner.to_text = function(input) {
    if (input == null || input === undefined) return '';
    if(input instanceof Date) return input.toDateString();
    if(input.toString) return input.toString();
    return '';
  };

  EJS.Scanner.prototype = {
    scan: function(block) {
      scanline = this.scanline;
      regex = this.SplitRegexp;
      if (! this.source == '') {
        var source_split = rsplit(this.source, /\n/);
        for (var i=0; i<source_split.length; i++) {
          var item = source_split[i];
          this.scanline(item, regex, block);
        };
      };
    },

    scanline: function(line, regex, block) {
      this.lines++;
      var line_split = rsplit(line, regex);
      for (var i=0; i<line_split.length; i++) {
        var token = line_split[i];
        if (token != null) {
          try {
            block(token, this);
          } catch(e) {
            throw {type: 'EJS.Scanner', line: this.lines};
          };
        };
      };
    }
  };

  EJS.Buffer = function(pre_cmd, post_cmd) {
    this.line = new Array();
    this.script = "";
    this.pre_cmd = pre_cmd;
    this.post_cmd = post_cmd;
    for (var i=0; i<this.pre_cmd.length; i++) {
      this.push(pre_cmd[i]);
    };
  };

  EJS.Buffer.prototype = {
    push: function(cmd) {
      this.line.push(cmd);
    },

    cr: function() {
      this.script = this.script + this.line.join('; ');
      this.line = new Array();
      this.script = this.script + "\n";
    },

    close: function() {
      if (this.line.length > 0) {
        for (var i=0; i<this.post_cmd.length; i++){
          this.push(pre_cmd[i]);
        };
        this.script = this.script + this.line.join('; ');
        line = null;
      };
    }
  };

  EJS.Compiler = function(source, left) {
    this.pre_cmd = ['var ___ViewO = [];'];
    this.post_cmd = new Array();
    this.source = ' ';
    if (source != null) {
      if (typeof(source) == 'string') {
        source = source.replace(/\r\n/g, "\n");
        source = source.replace(/\r/g,   "\n");
        this.source = source;
      } else if (source.innerHTML) {
        this.source = source.innerHTML;
      };
      if (typeof this.source != 'string') {
        this.source = "";
      };
    };
    left = left || '<';
    var right = '>';
    switch(left) {
      case '[':
        right = ']';
        break;
      case '<':
        break;
      default:
        throw left+' is not a supported deliminator';
        break;
    };
    this.scanner = new EJS.Scanner(this.source, left, right);
    this.out = '';
  };

  EJS.Compiler.prototype = {
    compile: function(options, name) {
      options = options || {};
      this.out = '';
      var put_cmd = "___ViewO.push(";
      var insert_cmd = put_cmd;
      var buff = new EJS.Buffer(this.pre_cmd, this.post_cmd);
      var content = '';
      var clean = function(content) {
        content = content.replace(/\\/g, '\\\\');
        content = content.replace(/\n/g, '\\n');
        content = content.replace(/"/g,   '\\"');
        return content;
      };
      this.scanner.scan(function(token, scanner) {
        if (scanner.stag == null)  {
          switch(token) {
            case '\n':
              content = content + "\n";
              buff.push(put_cmd + '"' + clean(content) + '");');
              buff.cr();
              content = '';
              break;
            case scanner.left_delimiter:
            case scanner.left_equal:
            case scanner.left_comment:
              scanner.stag = token;
              if (content.length > 0) {
                buff.push(put_cmd + '"' + clean(content) + '")');
              };
              content = '';
              break;
            case scanner.double_left:
              content = content + scanner.left_delimiter;
              break;
            default:
              content = content + token;
              break;
          };
        } else {
          switch(token) {
            case scanner.right_delimiter:
              switch(scanner.stag) {
                case scanner.left_delimiter:
                  if (content[content.length - 1] == '\n') {
                    content = chop(content);
                    buff.push(content);
                    buff.cr();
                  } else {
                    buff.push(content);
                  };
                  break;
                case scanner.left_equal:
                  buff.push(insert_cmd + "(EJS.Scanner.to_text(" + content + ")))");
                  break;
              };
              scanner.stag = null;
              content = '';
              break;
            case scanner.double_right:
              content = content + scanner.right_delimiter;
              break;
            default:
              content = content + token;
              break;
          };
        };
      });
      if (content.length > 0) {
        // Chould be content.dump in Ruby
        buff.push(put_cmd + '"' + clean(content) + '")');
      };
      buff.close();
      this.out = buff.script + ";";
      var to_be_evaled = '/*' + name + '*/this.process = function(_CONTEXT,_VIEW) { try { with(_VIEW) { with (_CONTEXT) {' + this.out + " return ___ViewO.join('');}}}catch(e){e.lineNumber=null;throw e;}};";
      try {
        eval(to_be_evaled);
      } catch(e) {
        if (typeof JSLINT != 'undefined') {
          JSLINT(this.out);
          for (var i = 0; i < JSLINT.errors.length; i++) {
            var error = JSLINT.errors[i];
            if (error.reason != "Unnecessary semicolon.") {
              error.line++;
              var e = new Error();
              e.lineNumber = error.line;
              e.message = error.reason;
              if (options.view) e.fileName = options.view;
              throw e;
            };
          };
        } else {
          throw e;
        };
      };
    }
  };

  //type, cache, folder
  /**
   * Sets default options for all views
   * @param {Object} options Set view with the following options
   * <table class="options">
          <tbody><tr><th>Option</th><th>Default</th><th>Description</th></tr>
          <tr>
            <td>type</td>
            <td>'<'</td>
            <td>type of magic tags.   Options are '&lt;' or '['
            </td>
          </tr>
          <tr>
            <td>cache</td>
            <td>true in production mode, false in other modes</td>
            <td>true to cache template.
            </td>
          </tr>
    </tbody></table>
   *
   */
  EJS.config = function(options){
    EJS.cache = (options.cache != null) ? options.cache : EJS.cache;
    EJS.type = (options.type != null) ? options.type : EJS.type;
    EJS.ext = (options.ext != null) ? options.ext : EJS.ext;
    var templates_directory = EJS.templates_directory || {}; //nice and private container
    EJS.templates_directory = templates_directory;
    EJS.get = function(path, cache) {
      if(cache == false) return null;
      if(templates_directory[path]) return templates_directory[path];
      return null;
    };
    EJS.update = function(path, template) {
      if (path == null) return;
      templates_directory[path] = template ;
    };
    EJS.INVALID_PATH =  -1;
  };
  EJS.config( {cache: true, type: '<', ext: '.ejs' } );

  /**
   * @constructor
   * By adding functions to EJS.Helpers.prototype, those functions will be available in the
   * views.
   * @init Creates a view helper.   This function is called internally.  You should never call it.
   * @param {Object} data The data passed to the view.  Helpers have access to it through this._data
   */
  EJS.Helpers = function(data, extras){
    this._data = data;
    this._extras = extras;
    extend(this, extras);
  };

  /* @prototype*/
  EJS.Helpers.prototype = {
    /**
     * Renders a new view.  If data is passed in, uses that to render the view.
     * @param {Object} options standard options passed to a new view.
     * @param {optional:Object} data
     * @return {String}
     */
    view: function(options, data, helpers) {
      if (!helpers) helpers = this._extras;
      if (!data) data = this._data;
      return new EJS(options).render(data, helpers);
    },
    /**
     * For a given value, tries to create a human representation.
     * @param {Object} input the value being converted.
     * @param {Object} null_text what text should be present if input == null or undefined, defaults to ''
     * @return {String}
     */
    to_text: function(input, null_text) {
      if (input == null || input === undefined) return null_text || '';
      if (input instanceof(Date)) return input.toDateString();
      if (input.toString) return input.toString().replace(/\n/g, '<br />').replace(/''/g, "'");
      return '';
    }
  };

  EJS.newRequest = function() {
    var factories = [function() { return new ActiveXObject("Msxml2.XMLHTTP"); },function() { return new XMLHttpRequest(); },function() { return new ActiveXObject("Microsoft.XMLHTTP"); }];
    for(var i = 0; i < factories.length; i++) {
      try {
        var request = factories[i]();
        if (request != null)  return request;
      } catch(e) { continue; }
    };
  };

  EJS.request = function(path) {
    var request = new EJS.newRequest();
    request.open("GET", path, false);
    try {
      request.send(null);
    } catch(e){
      return null;
    };
    if ( request.status == 404 || request.status == 2 ||(request.status == 0 && request.responseText == '') ) return null;
    return request.responseText;
  };

  EJS.ajax_request = function(params) {
    params.method = ( params.method ? params.method : 'GET');
    var request = new EJS.newRequest();
    request.onreadystatechange = function() {
      if (request.readyState == 4) {
        if (request.status == 200) {
          params.onComplete(request);
        } else {
          params.onComplete(request);
        };
      };
    };
    request.open(params.method, params.url);
    request.send(null);
  };

  EJS.Helpers.prototype.date_tag = function(name, value , html_options) {
    if(!(value instanceof(Date))) value = new Date();
    var month_names = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
    var years = [], months = [], days =[];
    var year = value.getFullYear();
    var month = value.getMonth();
    var day = value.getDate();
    for (var y = year - 15; y < year+15 ; y++) {
      years.push({value: y, text: y});
    };
    for (var m = 0; m < 12; m++) {
      months.push({value: (m), text: month_names[m]});
    };
    for (var d = 0; d < 31; d++) {
      days.push({value: (d+1), text: (d+1)});
    };
    var year_select = this.select_tag(name+'[year]', year, years, {id: name+'[year]'} );
    var month_select = this.select_tag(name+'[month]', month, months, {id: name+'[month]'});
    var day_select = this.select_tag(name+'[day]', day, days, {id: name+'[day]'});
    return year_select+month_select+day_select;
  };

  EJS.Helpers.prototype.form_tag = function(action, html_options) {
    html_options = html_options || {};
    html_options.action = action;
    if(html_options.multipart == true) {
      html_options.method = 'post';
      html_options.enctype = 'multipart/form-data';
    };
    return this.start_tag_for('form', html_options);
  };

  EJS.Helpers.prototype.form_tag_end = function() {
    return this.tag_end('form');
  };

  EJS.Helpers.prototype.hidden_field_tag   = function(name, value, html_options) {
    return this.input_field_tag(name, value, 'hidden', html_options);
  };

  EJS.Helpers.prototype.input_field_tag = function(name, value , inputType, html_options) {
    html_options = html_options || {};
    html_options.id   = html_options.id || name;
    html_options.value = value || '';
    html_options.type = inputType || 'text';
    html_options.name = name;
    return this.single_tag_for('input', html_options);
  };

  EJS.Helpers.prototype.is_current_page = function(url) {
    return (window.location.href == url || window.location.pathname == url ? true : false);
  };

  EJS.Helpers.prototype.link_to = function(name, url, html_options) {
    if(!name) var name = 'null';
    if(!html_options) var html_options = {};
    if(html_options.confirm){
      html_options.onclick = " var ret_confirm = confirm(\""+html_options.confirm+"\"); if(!ret_confirm){ return false;} ";
      html_options.confirm = null;
    };
    html_options.href = url;
    return this.start_tag_for('a', html_options)+name+ this.tag_end('a');
  };

  EJS.Helpers.prototype.submit_link_to = function(name, url, html_options) {
    if(!name) var name = 'null';
    if(!html_options) var html_options = {};
    html_options.onclick = html_options.onclick || '';
    if(html_options.confirm){
      html_options.onclick = " var ret_confirm = confirm(\""+html_options.confirm+"\"); if(!ret_confirm){ return false;} ";
      html_options.confirm = null;
    };
    html_options.value = name;
    html_options.type = 'submit';
    html_options.onclick = html_options.onclick + (url ? this.url_for(url) : '')+'return false;';
    return this.start_tag_for('input', html_options);
  };

  EJS.Helpers.prototype.link_to_if = function(condition, name, url, html_options, post, block) {
    return this.link_to_unless((condition == false), name, url, html_options, post, block);
  };

  EJS.Helpers.prototype.link_to_unless = function(condition, name, url, html_options, block) {
    html_options = html_options || {};
    if(condition) {
      if(block && typeof(block) == 'function') {
        return block(name, url, html_options, block);
      } else {
        return name;
      };
    } else {
      return this.link_to(name, url, html_options);
    };
  };

  EJS.Helpers.prototype.link_to_unless_current = function(name, url, html_options, block) {
    html_options = html_options || {};
    return this.link_to_unless(this.is_current_page(url), name, url, html_options, block);
  };


  EJS.Helpers.prototype.password_field_tag = function(name, value, html_options) {
    return this.input_field_tag(name, value, 'password', html_options);
  };

  EJS.Helpers.prototype.select_tag = function(name, value, choices, html_options) {
    html_options = html_options || {};
    html_options.id   = html_options.id  || name;
    html_options.value = value;
    html_options.name = name;
    var txt = '';
    txt += this.start_tag_for('select', html_options);
    for(var i = 0; i < choices.length; i++) {
      var choice = choices[i];
      var optionOptions = {value: choice.value};
      if(choice.value == value) optionOptions.selected ='selected';
      txt += this.start_tag_for('option', optionOptions )+choice.text+this.tag_end('option');
    };
    txt += this.tag_end('select');
    return txt;
  };

  EJS.Helpers.prototype.single_tag_for = function(tag, html_options) {
    return this.tag(tag, html_options, '/>');
  };

  EJS.Helpers.prototype.start_tag_for = function(tag, html_options)   {
    return this.tag(tag, html_options);
  };

  EJS.Helpers.prototype.submit_tag = function(name, html_options) {
    html_options = html_options || {};
    html_options.type = html_options.type   || 'submit';
    html_options.value = name || 'Submit';
    return this.single_tag_for('input', html_options);
  };

  EJS.Helpers.prototype.tag = function(tag, html_options, end) {
    if(!end) var end = '>';
    var txt = ' ';
    for(var attr in html_options) {
      if(html_options[attr] != null) {
        var value = html_options[attr].toString();
      } else {
        var value='';
      };
      // special case because "class" is a reserved word in IE
      if(attr == "Class") attr = "class";
      if( value.indexOf("'") != -1 ) {
        txt += attr+'=\"'+value+'\" ';
      } else {
        txt += attr+"='"+value+"' ";
      };
    };
    return '<' + tag + txt + end;
  };

  EJS.Helpers.prototype.tag_end = function(tag) {
    return '</'+tag+'>';
  };

  EJS.Helpers.prototype.text_area_tag = function(name, value, html_options) {
    html_options = html_options || {};
    html_options.id   = html_options.id  || name;
    html_options.name   = html_options.name  || name;
    value = value || '';
    if(html_options.size) {
      html_options.cols = html_options.size.split('x')[0];
      html_options.rows = html_options.size.split('x')[1];
      delete html_options.size;
    };
    html_options.cols = html_options.cols  || 50;
    html_options.rows = html_options.rows  || 4;
    return  this.start_tag_for('textarea', html_options)+value+this.tag_end('textarea');
  };

  EJS.Helpers.prototype.text_tag = EJS.Helpers.prototype.text_area_tag;

  EJS.Helpers.prototype.text_field_tag = function(name, value, html_options) {
    return this.input_field_tag(name, value, 'text', html_options);
  };

  EJS.Helpers.prototype.url_for = function(url) {
    return 'window.location="' + url + '";';
  };

  EJS.Helpers.prototype.img_tag = function(image_location, alt, options){
    options = options || {};
    options.src = image_location;
    options.alt = alt;
    return this.single_tag_for('img', options);
  };

  // -------------------------------------------------------------

  Sammy = Sammy || {};

  Sammy.EJS = function(app, method_alias) {

    // *Helper:* Uses simple templating to parse ERB like templates.
    //
    // ### Arguments
    //
    // * `template` A String template. '<% %>' tags are evaluated as Javascript and replaced with the elements in data.
    // * `data` An Object containing the replacement values for the template.
    //data is extended with the <tt>EventContext</tt> allowing you to call its methods within the template.
    // * `name` An optional String name to cache the template.
    //
    var template = function(template, data, name) {
      // use name for caching
      if (typeof name == 'undefined') name = template;
      return new EJS({text: template, name: name}).render(data);
    };

    // set the default method name/extension
    if (!method_alias) method_alias = 'ejs';

    // create the helper at the method alias
    app.helper(method_alias, template);

   };

})(jQuery);
