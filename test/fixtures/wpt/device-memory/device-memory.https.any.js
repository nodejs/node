test(function() {
    assert_equals(typeof navigator.deviceMemory, "number",
        "navigator.deviceMemory returns a number");
    assert_true(navigator.deviceMemory >= 0,
        "navigator.deviceMemory returns a positive value");
    assert_true([0.25, 0.5, 1, 2, 4, 8].includes(navigator.deviceMemory),
        "navigator.deviceMemory returns a power of 2 between 0.25 and 8");
}, "navigator.deviceMemory is a positive number, a power of 2, between 0.25 and 8");
