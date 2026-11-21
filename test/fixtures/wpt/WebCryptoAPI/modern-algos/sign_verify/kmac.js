function run_test() {
    function operationAlgorithm(vector) {
        var algorithm = {
            name: vector.algorithm,
            outputLength: vector.outputLength
        };
        if (vector.customization !== undefined) {
            algorithm.customization = vector.customization;
        }
        return algorithm;
    }

    runMacTests({
        importFormat: "raw-secret",
        importAlgorithm: function(vector) {
            return {name: vector.algorithm};
        },
        operationAlgorithm: operationAlgorithm,
        wrongVerificationAlgorithm: function(vector) {
            var algorithm = operationAlgorithm(vector);
            algorithm.outputLength = vector.outputLength === 256 ? 512 : 256;
            return algorithm;
        }
    });
}
