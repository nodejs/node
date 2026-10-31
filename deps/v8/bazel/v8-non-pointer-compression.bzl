"""A transition to compile targets without pointer compression."""

def _v8_disable_pointer_compression(settings, attr):
    return {
        "//:v8_enable_pointer_compression": "False",
    }

v8_disable_pointer_compression = transition(
    implementation = _v8_disable_pointer_compression,
    inputs = [],
    outputs = ["//:v8_enable_pointer_compression"],
)
