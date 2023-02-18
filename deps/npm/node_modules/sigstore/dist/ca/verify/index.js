"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifySigningCertificate = void 0;
const chain_1 = require("./chain");
const sct_1 = require("./sct");
const signer_1 = require("./signer");
function verifySigningCertificate(bundle, trustedRoot, options) {
    // Check that a trusted certificate chain can be found for the signing
    // certificate in the bundle
    const trustedChain = (0, chain_1.verifyChain)(bundle.verificationMaterial.content.x509CertificateChain.certificates, trustedRoot.certificateAuthorities);
    // Unless disabled, verify the SCTs in the signing certificate
    if (options.ctlogOptions.disable === false) {
        (0, sct_1.verifySCTs)(trustedChain, trustedRoot.ctlogs, options.ctlogOptions);
    }
    // Verify the signing certificate against the provided identities
    // if provided
    if (options.signers) {
        (0, signer_1.verifySignerIdentity)(trustedChain[0], options.signers.certificateIdentities);
    }
}
exports.verifySigningCertificate = verifySigningCertificate;
