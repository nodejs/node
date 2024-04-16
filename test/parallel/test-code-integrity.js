'use strict'

// Flags: --expose-internals

const assert = require('node:assert');
const {describe, it, mock} = require('node:test');
const ci = require('codeintegrity');
const opt = require('internal/options');

const pre_execution = require('internal/process/pre_execution');

describe('when code integrity enforcement is enabled', () => {
    it('should throw an error if no manifest is provided', (t) => {
        t.mock.method(ci, ci.isCodeIntegrityForcedByOS.name, () => { return true });
        
        assert.throws(
            pre_execution.readPolicyFromDisk,
            {
                code: "ERR_MANIFEST_SYSTEM_CI_VIOLATION"
            }
        );
    });

    it('should throw an error if no policy signature is given', (t) => {
        t.mock.method(opt, opt.getOptionValue.name, (o) => { if (o == "--policy-signature") { return null } else { return o } });
        t.mock.method(ci, ci.isCodeIntegrityForcedByOS.name, () => { return true });

        assert.throws(
            pre_execution.readPolicyFromDisk,
            {
                code: "ERR_MANIFEST_SYSTEM_CI_VIOLATION"
            }
        );
    })

    it('should throw an error if the policy signature fails to validate', (t) => {
        t.mock.method(opt, opt.getOptionValue.name, (o) => { return o; });
        t.mock.method(ci, ci.isCodeIntegrityForcedByOS.name, () => { return true });
        t.mock.method(ci, ci.isFileTrustedBySystemCodeIntegrityPolicy.name, () => { return false });

        assert.throws(
            pre_execution.readPolicyFromDisk,
            {
                code: "ERR_MANIFEST_SYSTEM_CI_VIOLATION"
            }
        );
    })
});
