#!/usr/bin/env ruby
require File.dirname(__FILE__) + "/common"

prog = IO.popen("#{$node} #{$tf.path}")
a = Time.now
assert_equal("hello\n", prog.readpartial(100))
b = Time.now
assert_equal("world\n", prog.read(100))
c = Time.now

assert_less_than(b - a, 0.5) # startup time
assert_less_than(1.5, c - b)

__END__
log("hello");
setTimeout(function () { log("world"); }, 1500);
