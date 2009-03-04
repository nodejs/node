#!/usr/bin/env ruby
require File.dirname(__FILE__) + "/common"

a = Time.now
out = `#{$node} #{$tf.path}`
b = Time.now
assert_equal("hello\nworld\n", out)
assert_less_than(b - a, 1) # startup time

__END__
log("hello");
timeout1 = setTimeout(function () { log("world"); }, 500);
timeout2 = setTimeout(function () { log("ryah"); }, 5000);
clearTimeout(timeout2)
