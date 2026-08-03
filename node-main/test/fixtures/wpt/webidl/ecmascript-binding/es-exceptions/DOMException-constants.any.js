// META: global=window,dedicatedworker,shadowrealm

'use strict';

test(function() {
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=27732
  var constants = [
    "INDEX_SIZE_ERR",
    "DOMSTRING_SIZE_ERR",
    "HIERARCHY_REQUEST_ERR",
    "WRONG_DOCUMENT_ERR",
    "INVALID_CHARACTER_ERR",
    "NO_DATA_ALLOWED_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "NOT_FOUND_ERR",
    "NOT_SUPPORTED_ERR",
    "INUSE_ATTRIBUTE_ERR",
    "INVALID_STATE_ERR",
    "SYNTAX_ERR",
    "INVALID_MODIFICATION_ERR",
    "NAMESPACE_ERR",
    "INVALID_ACCESS_ERR",
    "VALIDATION_ERR",
    "TYPE_MISMATCH_ERR",
    "SECURITY_ERR",
    "NETWORK_ERR",
    "ABORT_ERR",
    "URL_MISMATCH_ERR",
    "QUOTA_EXCEEDED_ERR",
    "TIMEOUT_ERR",
    "INVALID_NODE_TYPE_ERR",
    "DATA_CLONE_ERR"
  ]
  var objects = [
    [DOMException, "DOMException constructor object"],
    [DOMException.prototype, "DOMException prototype object"]
  ]
  constants.forEach(function(name, i) {
    objects.forEach(function(o) {
      var object = o[0], description = o[1];
      test(function() {
        assert_equals(object[name], i + 1, name)
        assert_own_property(object, name)
        var pd = Object.getOwnPropertyDescriptor(object, name)
        assert_false("get" in pd, "get")
        assert_false("set" in pd, "set")
        assert_false(pd.writable, "writable")
        assert_true(pd.enumerable, "enumerable")
        assert_false(pd.configurable, "configurable")
      }, "Constant " + name + " on " + description)
    })
  })
})
