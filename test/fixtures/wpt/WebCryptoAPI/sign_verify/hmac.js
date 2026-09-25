function run_test() {
    runMacTests({
        importFormat: "raw",
        importAlgorithm: function(vector) {
            return {name: "HMAC", hash: vector.hash};
        },
        operationAlgorithm: function(vector) {
            return {name: "HMAC", hash: vector.hash};
        }
    });
}
