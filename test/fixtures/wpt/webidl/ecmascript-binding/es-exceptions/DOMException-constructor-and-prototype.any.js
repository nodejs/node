test(function() {
    assert_own_property(self, "DOMException", "property of global");

    var desc = Object.getOwnPropertyDescriptor(self, "DOMException");
    assert_false("get" in desc, "get");
    assert_false("set" in desc, "set");
    assert_true(desc.writable, "writable");
    assert_false(desc.enumerable, "enumerable");
    assert_true(desc.configurable, "configurable");
}, "existence and property descriptor of DOMException");

test(function() {
    assert_own_property(self.DOMException, "prototype", "prototype property");

    var desc = Object.getOwnPropertyDescriptor(self.DOMException, "prototype");
    assert_false("get" in desc, "get");
    assert_false("set" in desc, "set");
    assert_false(desc.writable, "writable");
    assert_false(desc.enumerable, "enumerable");
    assert_false(desc.configurable, "configurable");
}, "existence and property descriptor of DOMException.prototype");

test(function() {
    assert_own_property(self.DOMException.prototype, "constructor", "property of prototype");
    var desc = Object.getOwnPropertyDescriptor(self.DOMException.prototype, "constructor");
    assert_false("get" in desc, "get");
    assert_false("set" in desc, "set");
    assert_true(desc.writable, "writable");
    assert_false(desc.enumerable, "enumerable");
    assert_true(desc.configurable, "configurable");
    assert_equals(self.DOMException.prototype.constructor, self.DOMException, "equality with actual constructor");
}, "existence and property descriptor of DOMException.prototype.constructor");
