"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TrustedMetadataStore = void 0;
const models_1 = require("@tufjs/models");
const error_1 = require("./error");
class TrustedMetadataStore {
    constructor(rootData) {
        this.trustedSet = {};
        // Client workflow 5.1: record fixed update start time
        this.referenceTime = new Date();
        // Client workflow 5.2: load trusted root metadata
        this.loadTrustedRoot(rootData);
    }
    get root() {
        if (!this.trustedSet.root) {
            throw new ReferenceError('No trusted root metadata');
        }
        return this.trustedSet.root;
    }
    get timestamp() {
        return this.trustedSet.timestamp;
    }
    get snapshot() {
        return this.trustedSet.snapshot;
    }
    get targets() {
        return this.trustedSet.targets;
    }
    getRole(name) {
        return this.trustedSet[name];
    }
    updateRoot(bytesBuffer) {
        const data = JSON.parse(bytesBuffer.toString('utf8'));
        const newRoot = models_1.Metadata.fromJSON(models_1.MetadataKind.Root, data);
        if (newRoot.signed.type != models_1.MetadataKind.Root) {
            throw new error_1.RepositoryError(`Expected 'root', got ${newRoot.signed.type}`);
        }
        // Client workflow 5.4: check for arbitrary software attack
        this.root.verifyDelegate(models_1.MetadataKind.Root, newRoot);
        // Client workflow 5.5: check for rollback attack
        if (newRoot.signed.version != this.root.signed.version + 1) {
            throw new error_1.BadVersionError(`Expected version ${this.root.signed.version + 1}, got ${newRoot.signed.version}`);
        }
        // Check that new root is signed by self
        newRoot.verifyDelegate(models_1.MetadataKind.Root, newRoot);
        // Client workflow 5.7: set new root as trusted root
        this.trustedSet.root = newRoot;
        return newRoot;
    }
    updateTimestamp(bytesBuffer) {
        if (this.snapshot) {
            throw new error_1.RuntimeError('Cannot update timestamp after snapshot');
        }
        if (this.root.signed.isExpired(this.referenceTime)) {
            throw new error_1.ExpiredMetadataError('Final root.json is expired');
        }
        const data = JSON.parse(bytesBuffer.toString('utf8'));
        const newTimestamp = models_1.Metadata.fromJSON(models_1.MetadataKind.Timestamp, data);
        if (newTimestamp.signed.type != models_1.MetadataKind.Timestamp) {
            throw new error_1.RepositoryError(`Expected 'timestamp', got ${newTimestamp.signed.type}`);
        }
        // Client workflow 5.4.2: check for arbitrary software attack
        this.root.verifyDelegate(models_1.MetadataKind.Timestamp, newTimestamp);
        if (this.timestamp) {
            // Prevent rolling back timestamp version
            // Client workflow 5.4.3.1: check for rollback attack
            if (newTimestamp.signed.version < this.timestamp.signed.version) {
                throw new error_1.BadVersionError(`New timestamp version ${newTimestamp.signed.version} is less than current version ${this.timestamp.signed.version}`);
            }
            //  Keep using old timestamp if versions are equal.
            if (newTimestamp.signed.version === this.timestamp.signed.version) {
                throw new error_1.EqualVersionError(`New timestamp version ${newTimestamp.signed.version} is equal to current version ${this.timestamp.signed.version}`);
            }
            // Prevent rolling back snapshot version
            // Client workflow 5.4.3.2: check for rollback attack
            const snapshotMeta = this.timestamp.signed.snapshotMeta;
            const newSnapshotMeta = newTimestamp.signed.snapshotMeta;
            if (newSnapshotMeta.version < snapshotMeta.version) {
                throw new error_1.BadVersionError(`New snapshot version ${newSnapshotMeta.version} is less than current version ${snapshotMeta.version}`);
            }
        }
        // expiry not checked to allow old timestamp to be used for rollback
        // protection of new timestamp: expiry is checked in update_snapshot
        this.trustedSet.timestamp = newTimestamp;
        // Client workflow 5.4.4: check for freeze attack
        this.checkFinalTimestamp();
        return newTimestamp;
    }
    updateSnapshot(bytesBuffer, trusted = false) {
        if (!this.timestamp) {
            throw new error_1.RuntimeError('Cannot update snapshot before timestamp');
        }
        if (this.targets) {
            throw new error_1.RuntimeError('Cannot update snapshot after targets');
        }
        // Snapshot cannot be loaded if final timestamp is expired
        this.checkFinalTimestamp();
        const snapshotMeta = this.timestamp.signed.snapshotMeta;
        // Verify non-trusted data against the hashes in timestamp, if any.
        // Trusted snapshot data has already been verified once.
        // Client workflow 5.5.2: check against timestamp role's snaphsot hash
        if (!trusted) {
            snapshotMeta.verify(bytesBuffer);
        }
        const data = JSON.parse(bytesBuffer.toString('utf8'));
        const newSnapshot = models_1.Metadata.fromJSON(models_1.MetadataKind.Snapshot, data);
        if (newSnapshot.signed.type != models_1.MetadataKind.Snapshot) {
            throw new error_1.RepositoryError(`Expected 'snapshot', got ${newSnapshot.signed.type}`);
        }
        // Client workflow 5.5.3: check for arbitrary software attack
        this.root.verifyDelegate(models_1.MetadataKind.Snapshot, newSnapshot);
        // version check against meta version (5.5.4) is deferred to allow old
        // snapshot to be used in rollback protection
        // Client workflow 5.5.5: check for rollback attack
        if (this.snapshot) {
            Object.entries(this.snapshot.signed.meta).forEach(([fileName, fileInfo]) => {
                const newFileInfo = newSnapshot.signed.meta[fileName];
                if (!newFileInfo) {
                    throw new error_1.RepositoryError(`Missing file ${fileName} in new snapshot`);
                }
                if (newFileInfo.version < fileInfo.version) {
                    throw new error_1.BadVersionError(`New version ${newFileInfo.version} of ${fileName} is less than current version ${fileInfo.version}`);
                }
            });
        }
        this.trustedSet.snapshot = newSnapshot;
        // snapshot is loaded, but we raise if it's not valid _final_ snapshot
        // Client workflow 5.5.4 & 5.5.6
        this.checkFinalSnapsnot();
        return newSnapshot;
    }
    updateDelegatedTargets(bytesBuffer, roleName, delegatorName) {
        if (!this.snapshot) {
            throw new error_1.RuntimeError('Cannot update delegated targets before snapshot');
        }
        // Targets cannot be loaded if final snapshot is expired or its version
        // does not match meta version in timestamp.
        this.checkFinalSnapsnot();
        const delegator = this.trustedSet[delegatorName];
        if (!delegator) {
            throw new error_1.RuntimeError(`No trusted ${delegatorName} metadata`);
        }
        // Extract metadata for the delegated role from snapshot
        const meta = this.snapshot.signed.meta?.[`${roleName}.json`];
        if (!meta) {
            throw new error_1.RepositoryError(`Missing ${roleName}.json in snapshot`);
        }
        // Client workflow 5.6.2: check against snapshot role's targets hash
        meta.verify(bytesBuffer);
        const data = JSON.parse(bytesBuffer.toString('utf8'));
        const newDelegate = models_1.Metadata.fromJSON(models_1.MetadataKind.Targets, data);
        if (newDelegate.signed.type != models_1.MetadataKind.Targets) {
            throw new error_1.RepositoryError(`Expected 'targets', got ${newDelegate.signed.type}`);
        }
        // Client workflow 5.6.3: check for arbitrary software attack
        delegator.verifyDelegate(roleName, newDelegate);
        // Client workflow 5.6.4: Check against snapshot role’s targets version
        const version = newDelegate.signed.version;
        if (version != meta.version) {
            throw new error_1.BadVersionError(`Version ${version} of ${roleName} does not match snapshot version ${meta.version}`);
        }
        // Client workflow 5.6.5: check for a freeze attack
        if (newDelegate.signed.isExpired(this.referenceTime)) {
            throw new error_1.ExpiredMetadataError(`${roleName}.json is expired`);
        }
        this.trustedSet[roleName] = newDelegate;
    }
    // Verifies and loads data as trusted root metadata.
    // Note that an expired initial root is still considered valid.
    loadTrustedRoot(bytesBuffer) {
        const data = JSON.parse(bytesBuffer.toString('utf8'));
        const root = models_1.Metadata.fromJSON(models_1.MetadataKind.Root, data);
        if (root.signed.type != models_1.MetadataKind.Root) {
            throw new error_1.RepositoryError(`Expected 'root', got ${root.signed.type}`);
        }
        root.verifyDelegate(models_1.MetadataKind.Root, root);
        this.trustedSet['root'] = root;
    }
    checkFinalTimestamp() {
        // Timestamp MUST be loaded
        if (!this.timestamp) {
            throw new ReferenceError('No trusted timestamp metadata');
        }
        // Client workflow 5.4.4: check for freeze attack
        if (this.timestamp.signed.isExpired(this.referenceTime)) {
            throw new error_1.ExpiredMetadataError('Final timestamp.json is expired');
        }
    }
    checkFinalSnapsnot() {
        // Snapshot and timestamp MUST be loaded
        if (!this.snapshot) {
            throw new ReferenceError('No trusted snapshot metadata');
        }
        if (!this.timestamp) {
            throw new ReferenceError('No trusted timestamp metadata');
        }
        // Client workflow 5.5.6: check for freeze attack
        if (this.snapshot.signed.isExpired(this.referenceTime)) {
            throw new error_1.ExpiredMetadataError('snapshot.json is expired');
        }
        // Client workflow 5.5.4: check against timestamp role’s snapshot version
        const snapshotMeta = this.timestamp.signed.snapshotMeta;
        if (this.snapshot.signed.version !== snapshotMeta.version) {
            throw new error_1.BadVersionError("Snapshot version doesn't match timestamp");
        }
    }
}
exports.TrustedMetadataStore = TrustedMetadataStore;
