(function() {
  'use strict';
  var variants = {
    /**
     * Tests are executed in the absence of the global Promise constructor by
     * default in order to verify support for the Server browser engine.
     *
     * https://github.com/w3c/web-platform-tests/issues/6266
     */
    'default': {
      description: 'Global Promise constructor removed.',
      apply: function() {
        delete window.Promise;
      }
    },
    /**
     * This variant allows for testing functionality that is fundamentally
     * dependent on Promise support, e.g. the `promise_test` function
     */
    'keep-promise': {
      description: 'No modification of global environment.',
      apply: function() {}
    }
  };
  var match = window.location.search.match(/\?(.*)$/);
  var variantName = (match && match[1]) || 'default';

  if (!Object.hasOwnProperty.call(variants, variantName)) {
    window.location = 'javascript:"Unrecognized variant: ' + variantName + '";';
    document.close();
    return;
  }

  if (typeof test === 'function') {
    test(function() {
      assert_unreached('variants.js must be included before testharness.js');
    });
  }
  var variant = variants[variantName];

  var variantNode = document.createElement('div');
  variantNode.innerHTML = '<p>This testharness.js test was executed with ' +
    'the variant named, "' + variantName + '". ' + variant.description +
    '</p><p>Refer to the test harness README file for more information.</p>';
  function onReady() {
    if (document.readyState !== 'complete') {
      return;
    }

    document.body.insertBefore(variantNode, document.body.firstChild);
  }

  onReady();
  document.addEventListener('readystatechange', onReady);

  variant.apply();
}());
