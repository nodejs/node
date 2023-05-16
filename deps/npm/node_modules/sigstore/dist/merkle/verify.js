"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifyInclusion = void 0;
/*
Copyright 2022 GitHub, Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
// Implementation largely copied from
// https://github.com/transparency-dev/merkle/blob/main/proof/verify.go#L46
// Verifies the correctness of the inclusion proof for the given leaf hash
// and index relative to the tree of the given size and root hash.
function verifyInclusion(hasher, index, size, leafHash, proof, root) {
    const calcroot = rootFromInclusionProof(hasher, index, size, leafHash, proof);
    return calcroot.equals(root);
}
exports.verifyInclusion = verifyInclusion;
// Calculates the expected root hash for a tree of the given size, provided a
// leaf index and hash with corresponding inclusion proof.
function rootFromInclusionProof(hasher, index, size, leafHash, proof) {
    if (index >= size) {
        throw new Error('index exceeds size of tree');
    }
    if (leafHash.length !== hasher.size()) {
        throw new Error('leafHash has unexpected size');
    }
    const { inner, border } = decompInclProof(index, size);
    if (proof.length != inner + border) {
        throw new Error('invalid proof length');
    }
    let hash = chainInner(hasher, leafHash, proof.slice(0, inner), index);
    hash = chainBorderRight(hasher, hash, proof.slice(inner));
    return hash;
}
// Breaks down inclusion proof for a leaf at the specified index in a tree of
// the specified size. The split point is where paths to the index leaf and
// the (size - 1) leaf diverge. Returns lengths of the bottom and upper proof
// parts.
function decompInclProof(index, size) {
    const inner = innerProofSize(index, size);
    const border = onesCount(index >> BigInt(inner));
    return { inner, border };
}
// Computes a subtree hash for an node on or below the tree's right border.
// Assumes the provided proof hashes are ordered from lower to higher levels
// and seed is the initial hash of the node specified by the index.
function chainInner(hasher, seed, proof, index) {
    return proof.reduce((acc, h, i) => {
        if ((index >> BigInt(i)) & BigInt(1)) {
            return hasher.hashChildren(h, acc);
        }
        else {
            return hasher.hashChildren(acc, h);
        }
    }, seed);
}
// Computes a subtree hash for nodes along the tree's right border.
function chainBorderRight(hasher, seed, proof) {
    return proof.reduce((acc, h) => hasher.hashChildren(h, acc), seed);
}
function innerProofSize(index, size) {
    return (index ^ (size - BigInt(1))).toString(2).length;
}
// Counts the number of ones in the binary representation of the given number.
// https://en.wikipedia.org/wiki/Hamming_weight
function onesCount(x) {
    return x.toString(2).split('1').length - 1;
}
