// META: global=window,worker

test(function() {
    const source = new EventSource("");
    assert_equals(source.url, self.location.toString());
}, "EventSource constructor with an empty url.");
