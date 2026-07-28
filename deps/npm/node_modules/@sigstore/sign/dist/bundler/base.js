"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BaseBundleBuilder = void 0;
// BaseBundleBuilder is a base class for BundleBuilder implementations. It
// provides a the basic wokflow for signing and witnessing an artifact.
// Subclasses must implement the `package` method to assemble a valid bundle
// with the generated signature and verification material.
class BaseBundleBuilder {
    signer;
    witnesses;
    constructor(options) {
        this.signer = options.signer;
        this.witnesses = options.witnesses;
    }
    // Executes the signing/witnessing process for the given artifact.
    async create(artifact) {
        const signature = await this.prepare(artifact).then((blob) => this.signer.sign(blob));
        const bundle = await this.package(artifact, signature);
        // Invoke all of the witnesses in parallel
        const verificationMaterials = await Promise.all(this.witnesses.map((witness) => witness.testify(bundle.content, publicKey(signature.key))));
        // Collect the verification material from all of the witnesses
        const tlogEntryList = [];
        const timestampList = [];
        verificationMaterials.forEach(({ tlogEntries, rfc3161Timestamps }) => {
            tlogEntryList.push(...(tlogEntries ?? []));
            timestampList.push(...(rfc3161Timestamps ?? []));
        });
        // Merge the collected verification material into the bundle
        bundle.verificationMaterial.tlogEntries = tlogEntryList;
        bundle.verificationMaterial.timestampVerificationData = {
            rfc3161Timestamps: timestampList,
        };
        return bundle;
    }
    // Override this function to apply any pre-signing transformations to the
    // artifact. The returned buffer will be signed by the signer. The default
    // implementation simply returns the artifact data.
    async prepare(artifact) {
        return artifact.data;
    }
}
exports.BaseBundleBuilder = BaseBundleBuilder;
// Extracts the public key from a KeyMaterial. Returns either the public key
// or the certificate, depending on the type of key material.
function publicKey(key) {
    switch (key.$case) {
        case 'publicKey':
            return key.publicKey;
        case 'x509Certificate':
            return key.certificate;
    }
}
