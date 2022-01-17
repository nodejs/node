// Run for enough iterations that we're likely to catch edge-cases, like
// failing to set a reserved bit:
const iterations = 256;
// Track all the UUIDs generated during test run, bail if we ever collide:
const uuids = new Set()
function randomUUID() {
    const uuid = self.crypto.randomUUID();
    if (uuids.has(uuid)) {
        throw new Error(`uuid collision ${uuid}`)
    }
    uuids.add(uuid);
    return uuid;
}

// UUID is in namespace format (16 bytes separated by dashes):
test(function() {
    const UUIDRegex = /[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}/
    for (let i = 0; i < iterations; i++) {
        assert_true(UUIDRegex.test(randomUUID()));
    }
}, "namespace format");

// Set the 4 most significant bits of array[6], which represent the UUID
// version, to 0b0100:
test(function() {
    for (let i = 0; i < iterations; i++) {
        let value = parseInt(randomUUID().split('-')[2].slice(0, 2), 16);
        value &= 0b11110000;
        assert_true(value === 0b01000000);
    }
}, "version set");

// Set the 2 most significant bits of array[8], which represent the UUID
// variant, to 0b10:
test(function() {
    for (let i = 0; i < iterations; i++) {
        // Grab the byte representing array[8]:
        let value = parseInt(randomUUID().split('-')[3].slice(0, 2), 16);
        value &= 0b11000000
        assert_true(value === 0b10000000);
    }
}, "variant set");
