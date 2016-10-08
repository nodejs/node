(function($) {

  Sammy = Sammy || {};

  function getStringContent(object, content) {
    if (typeof content === 'undefined') {
      return '';
    } else if ($.isFunction(content)) {
      content = content.apply(object);
    }
    return content.toString();
  };

  function simple_element(tag, attributes, content) {
    var html = "<";
    html += tag;
    if (typeof attributes != 'undefined') {
      $.each(attributes, function(key, value) {
        if (value != null) {
          html += " " + key + "='";
          html += getStringContent(attributes, value).replace(/\'/g, "\'");
          html += "'";
        }
      });
    }
    if (content === false) {
      html += ">";
    } else if (typeof content != 'undefined') {
      html += ">";
      html += getStringContent(this, content);
      html += "</" + tag + ">";
    } else {
      html += " />";
    }
    return html;
  };

  // Sammy.FormBuilder is based very closely on the Rails FormBuilder classes.
  // Its goal is to make it easy to create HTML forms for creating and editing
  // JavaScript objects. It eases the process by auto-populating existing values
  // into form inputs and creating input names suitable for parsing by
  // Sammy.NestedParams and other backend frameworks.
  //
  // You initialize a Sammy.FormBuilder by passing the 'name' of the object and
  // the object itself. Once initialized you create form elements with the object's
  // prototype methods. Each of these methods returns a string of HTML suitable for
  // appending through a template or directly with jQuery.
  //
  // ### Example
  //
  //      var item = {
  //        name: 'My Item',
  //        price: '$25.50',
  //        meta: {
  //          id: '123'
  //        }
  //      };
  //      var form = new Sammy.FormBuilder('item', item);
  //      form.text('name');
  //      //=> <input type='text' name='item[form]' value='My Item' />
  //
  // Nested attributes can be accessed/referred to by a 'keypath' which is
  // basically a string representation of the dot notation.
  //
  //      form.hidden('meta.id');
  //      //=> <input type='hidden' name='item[meta][id]' value='123' />
  //
  Sammy.FormBuilder = function(name, object) {
    this.name   = name;
    this.object = object;
  };

  $.extend(Sammy.FormBuilder.prototype, {

    // creates the open form tag with the object attributes
    open: function(attributes) {
      return simple_element('form', $.extend({'method': 'post', 'action': '#/' + this.name + 's'}, attributes), false);
    },

    // closes the form
    close: function() {
      return '</form>';
    },

    // creates a label for `keypath` with the text `content
    // with an optional `attributes` object
    label: function(keypath, content, attributes) {
      var attrs = {'for': this._attributesForKeyPath(keypath).name};
      return simple_element('label', $.extend(attrs, attributes), content);
    },

    // creates a hidden input for `keypath` with an optional `attributes` object
    hidden: function(keypath, attributes) {
      attributes = $.extend({type: 'hidden'}, this._attributesForKeyPath(keypath), attributes);
      return simple_element('input', attributes);
    },

    // creates a text input for `keypath` with an optional `attributes` object
    text: function(keypath, attributes) {
      attributes = $.extend({type: 'text'}, this._attributesForKeyPath(keypath), attributes);
      return simple_element('input', attributes);
    },

    // creates a textarea for `keypath` with an optional `attributes` object
    textarea: function(keypath, attributes) {
      var current;
      attributes = $.extend(this._attributesForKeyPath(keypath), attributes);
      current = attributes.value;
      delete attributes['value'];
      return simple_element('textarea', attributes, current);
    },

    // creates a password input for `keypath` with an optional `attributes` object
    password: function(keypath, attributes) {
      return this.text(keypath, $.extend({type: 'password'}, attributes));
    },

    // creates a select element for `keypath` with the option elements
    // specified by an array in `options`. If `options` is an array of arrays,
    // the first element in each subarray becomes the text of the option and the
    // second becomes the value.
    //
    // ### Example
    //
    //     var options = [
    //       ['Small', 's'],
    //       ['Medium', 'm'],
    //       ['Large', 'l']
    //     ];
    //     form.select('size', options);
    //     //=> <select name='item[size]'><option value='s'>Small</option> ...
    //
    //
    select: function(keypath, options, attributes) {
      var option_html = "", selected;
      attributes = $.extend(this._attributesForKeyPath(keypath), attributes);
      selected = attributes.value;
      delete attributes['value'];
      $.each(options, function(i, option) {
        var value, text, option_attrs;
        if ($.isArray(option)) {
          value = option[1], text = option[0];
        } else {
          value = option, text = option;
        }
        option_attrs = {value: getStringContent(this.object, value)};
        // select the correct option
        if (value === selected) { option_attrs.selected = 'selected'; }
        option_html += simple_element('option', option_attrs, text);
      });
      return simple_element('select', attributes, option_html);
    },

    // creates a radio input for keypath with the value `value`. Multiple
    // radios can be created with different value, if `value` equals the
    // current value of the key of the form builder's object the attribute
    // checked='checked' will be added.
    radio: function(keypath, value, attributes) {
      var selected;
      attributes = $.extend(this._attributesForKeyPath(keypath), attributes);
      selected = attributes.value;
      attributes.value = getStringContent(this.object, value);
      if (selected == attributes.value) {
        attributes.checked = 'checked';
      }
      return simple_element('input', $.extend({type:'radio'}, attributes));
    },

    // creates a checkbox input for keypath with the value `value`. Multiple
    // checkboxes can be created with different value, if `value` equals the
    // current value of the key of the form builder's object the attribute
    // checked='checked' will be added.
    //
    // By default `checkbox()` also generates a hidden element whose value is
    // the inverse of the value given. This is known hack to get around a common
    // gotcha where browsers and jQuery itself does not include 'unchecked'
    // elements in the list of submittable inputs. This ensures that a value
    // should always be passed to Sammy and hence the server. You can disable
    // the creation of the hidden element by setting the `hidden_element` attribute
    // to `false`
    checkbox: function(keypath, value, attributes) {
      var content = "";
      if (!attributes) { attributes = {}; }
      if (attributes.hidden_element !== false) {
        content += this.hidden(keypath, {'value': !value});
      }
      delete attributes['hidden_element'];
      content += this.radio(keypath, value, $.extend({type: 'checkbox'}, attributes));
      return content;
    },

    // creates a submit input for `keypath` with an optional `attributes` object
    submit: function(attributes) {
      return simple_element('input', $.extend({'type': 'submit'}, attributes));
    },

    _attributesForKeyPath: function(keypath) {
      var builder    = this,
          keys       = $.isArray(keypath) ? keypath : keypath.split(/\./),
          name       = builder.name,
          value      = builder.object,
          class_name = builder.name;

      $.each(keys, function(i, key) {
        if ((typeof value === 'undefined') || value == '') {
          value = ''
        } else if (typeof key == 'number' || key.match(/^\d+$/)) {
          value = value[parseInt(key, 10)];
        } else {
          value = value[key];
        }
        name += "[" + key + "]";
        class_name += "-" + key;
      });
      return {'name': name,
              'value': getStringContent(builder.object, value),
              'class': class_name};
    }
  });

  // Sammy.Form is a Sammy plugin that adds form building helpers to a
  // Sammy.Application
  Sammy.Form = function(app) {

    app.helpers({
      // simple_element is a simple helper for creating HTML tags.
      //
      // ### Arguments
      //
      // * `tag` the HTML tag to generate e.g. input, p, etc/
      // * `attributes` an object representing the attributes of the element as
      //   key value pairs. e.g. {class: 'element-class'}
      // * `content` an optional string representing the content for the
      //   the element. If ommited, the element becomes self closing
      //
      simple_element: simple_element,

      // formFor creates a Sammy.Form builder object with the passed `name`
      // and `object` and passes it as an argument to the `content_callback`.
      // This is a shortcut for creating FormBuilder objects for use within
      // templates.
      //
      // ### Example
      //
      //      // in item_form.template
      //
      //      <% formFor('item', item, function(f) { %>
      //        <%= f.open({action: '#/items'}) %>
      //        <p>
      //          <%= f.label('name') %>
      //          <%= f.text('name') %>
      //        </p>
      //        <p>
      //          <%= f.submit() %>
      //        </p>
      //        <%= f.close() %>
      //      <% }); %>
      //
      formFor: function(name, object, content_callback) {
        var builder;
        // define a form with just a name
        if ($.isFunction(object)) {
          content_callback = object;
          object = this[name];
        }
        builder = new Sammy.FormBuilder(name, object),
        content_callback.apply(this, [builder]);
        return builder;
      }
    });

  };

})(jQuery);
