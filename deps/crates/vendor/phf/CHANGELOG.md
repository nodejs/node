# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 0.11.3 (2025-01-07)

### Chore

 - <csr-id-a96a4e29d63fb1ab3cc10e050571e733f5d2d0d1/> bump Cargo.toml version of phf and phf_macros

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 9 commits contributed to the release.
 - 562 days passed between releases.
 - 1 commit was understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Adjusting changelogs prior to release of phf_shared v0.11.3, phf_generator v0.11.3, phf_macros v0.11.3, phf v0.11.3, phf_codegen v0.11.3 ([`a95dade`](https://github.com/rust-phf/rust-phf/commit/a95dade6f69866b7871f85dd3fd42984df2f3d28))
    - Merge pull request #322 from JohnTitor/release-0.11.3 ([`dc64dd6`](https://github.com/rust-phf/rust-phf/commit/dc64dd6bace986a8858590455e08659d9ea4ae4b))
    - Reset version num ([`13581f8`](https://github.com/rust-phf/rust-phf/commit/13581f8e9eefe8b8b7cb1b1ad04f2d68d97b0ffd))
    - Merge pull request #315 from LunarLambda/master ([`695a0df`](https://github.com/rust-phf/rust-phf/commit/695a0df769f3c75150a67ed9bb316579b875289d))
    - Bump Cargo.toml version of phf and phf_macros ([`a96a4e2`](https://github.com/rust-phf/rust-phf/commit/a96a4e29d63fb1ab3cc10e050571e733f5d2d0d1))
    - Merge pull request #290 from thaliaarchi/eq-trait ([`f89fca4`](https://github.com/rust-phf/rust-phf/commit/f89fca430205ddcbd7f41fa7c4f4f2144ae62cdb))
    - Merge pull request #300 from JohnTitor/msrv-1.61 ([`323366d`](https://github.com/rust-phf/rust-phf/commit/323366d03966ddad2eaa3432df79c9da8339e319))
    - Bump MSRV to 1.61 ([`1795f7b`](https://github.com/rust-phf/rust-phf/commit/1795f7b66b16af0191f221dc957bc8a090c891ad))
    - Implement PartialEq and Eq for map and set types ([`6e5dc32`](https://github.com/rust-phf/rust-phf/commit/6e5dc322cd3fac4eea960a6f2778989ccf985f95))
</details>

## 0.11.2 (2023-06-24)

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 10 commits contributed to the release.
 - 319 days passed between releases.
 - 0 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **Uncategorized**
    - Release phf_shared v0.11.2, phf_generator v0.11.2, phf_macros v0.11.2, phf v0.11.2, phf_codegen v0.11.2 ([`c9c35fd`](https://github.com/rust-phf/rust-phf/commit/c9c35fd8ba3f1bc228388b0cef6e3814a02a72c0))
    - Update changelogs ([`a1e5072`](https://github.com/rust-phf/rust-phf/commit/a1e5072b8e84b108f06389a1d41ac868426a03f7))
    - Merge pull request #288 from JohnTitor/rm-phf-stats ([`8fd5b77`](https://github.com/rust-phf/rust-phf/commit/8fd5b7770d427aea5004d17ff585541d0856d40b))
    - Remove mentions to `PHF_STATS` ([`0b7a826`](https://github.com/rust-phf/rust-phf/commit/0b7a82689ceab9e0e364c1d1dbe3639d2e99320a))
    - Merge pull request #280 from jf2048/deref-bytestring ([`3776342`](https://github.com/rust-phf/rust-phf/commit/377634245c8c6f0569a2ed7b75d08366b54c8810))
    - Allow using dereferenced bytestring literal keys in phf_map! ([`8c0d057`](https://github.com/rust-phf/rust-phf/commit/8c0d0572da8c0b5e188e7fda4ab8bd4bcb97f720))
    - Merge pull request #276 from JohnTitor/playground-metadata ([`f8e9d27`](https://github.com/rust-phf/rust-phf/commit/f8e9d279c528cb6985badc3ca3a60117ef92d51b))
    - Add metadata for playground ([`7e212e3`](https://github.com/rust-phf/rust-phf/commit/7e212e345f41a16409776a59796dd9ab24d6527d))
    - Merge pull request #274 from ankane/license-files ([`21baa73`](https://github.com/rust-phf/rust-phf/commit/21baa73941a0694ec48f437c0c0a6abfcc2f32d2))
    - Include license files in crates ([`1229b2f`](https://github.com/rust-phf/rust-phf/commit/1229b2faa6b97542ab4850a1723b1723dea92814))
</details>

## 0.11.1 (2022-08-08)

<csr-id-92e7b433a4f62cc9b070cd1d678a6061d0906ee6/>

### Chore

 - <csr-id-92e7b433a4f62cc9b070cd1d678a6061d0906ee6/> point to local crates for now

### Documentation

 - <csr-id-6be1599d7a0df27fd1888c78d247f8810cb8f750/> state allowed key expressions in `phf_map`

### Bug Fixes

 - <csr-id-caf1ce71aed110fb44206ce2291154572ebfe9b7/> remove now-unnecessary `proc-macro-hack` crate usage
   Resolves <https://github.com/rust-phf/rust-phf/issues/255>.
   
   This resolves an issue with Windows Defender identifying `proc-macro-hack` as threats. It also sheds
   a depedency that is no longer necessary, now that the MSRV of this crate is 1.46 and
   `proc-macro-hack` is only useful for providing support for Rust versions 1.31 through 1.45. Per
   [upstream](https://github.com/dtolnay/proc-macro-hack):
   
   > **Note:** _As of Rust 1.45 this crate is superseded by native support for #\[proc\_macro\] in
   > expression position. Only consider using this crate if you care about supporting compilers between
   > 1.31 and 1.45._

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 310 commits contributed to the release.
 - 3 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 1 unique issue was worked on: [#249](https://github.com/rust-phf/rust-phf/issues/249)

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

 * **[#249](https://github.com/rust-phf/rust-phf/issues/249)**
    - Add `Map::new()` function and `Default` implementation to create new, empty map ([`baac7d0`](https://github.com/rust-phf/rust-phf/commit/baac7d065a71a388476b998ba55b1c0aedaa9d86))
 * **Uncategorized**
    - Release phf_shared v0.11.1, phf_generator v0.11.1, phf_macros v0.11.1, phf v0.11.1, phf_codegen v0.11.1 ([`3897b21`](https://github.com/rust-phf/rust-phf/commit/3897b21c6d38e5adcaf9110b4bb33c19f6b41977))
    - Merge pull request #264 from rust-phf/tweak-changelog ([`97f997d`](https://github.com/rust-phf/rust-phf/commit/97f997d2be827ca636a29046c78e2c09c5c62650))
    - Replace handmade changelog with generated one by `cargo-smart-release` ([`cb84cf6`](https://github.com/rust-phf/rust-phf/commit/cb84cf6636ab52823c53e70d6abeac8f648a3482))
    - Merge pull request #260 from JohnTitor/fix-repo-link ([`1407ebe`](https://github.com/rust-phf/rust-phf/commit/1407ebe536b39611db92d765ddec4de0e6c8a16e))
    - Add category to crates ([`32a72c3`](https://github.com/rust-phf/rust-phf/commit/32a72c3859997fd6b590e9ec092ae789d2acdf55))
    - Update repository links on Cargo.toml ([`1af3b0f`](https://github.com/rust-phf/rust-phf/commit/1af3b0fe1f8fdcae7ccc1bc8d51de309fb16a6bf))
    - Merge pull request #258 from JohnTitor/release-0.11.0 ([`c0b9ef9`](https://github.com/rust-phf/rust-phf/commit/c0b9ef98e798f807f94544aeb0fff429ef280efc))
    - Release 0.11.0 ([`d2efdc0`](https://github.com/rust-phf/rust-phf/commit/d2efdc08a7eb1d0d6c414b7b2ac41ce1fe1f9a43))
    - Merge pull request #257 from JohnTitor/edition-2021 ([`36ec885`](https://github.com/rust-phf/rust-phf/commit/36ec8854a9da4f295618e98d94aaf7150df2173e))
    - Make crates edition 2021 ([`b9d25da`](https://github.com/rust-phf/rust-phf/commit/b9d25da58b912d9927fbc41901631cd77836462b))
    - Merge pull request #256 from NZXTCorp/remove-proc-macro-hack ([`a85f070`](https://github.com/rust-phf/rust-phf/commit/a85f070d641317a04b81da053cc4040619652e69))
    - Remove now-unnecessary `proc-macro-hack` crate usage ([`caf1ce7`](https://github.com/rust-phf/rust-phf/commit/caf1ce71aed110fb44206ce2291154572ebfe9b7))
    - Point to local crates for now ([`92e7b43`](https://github.com/rust-phf/rust-phf/commit/92e7b433a4f62cc9b070cd1d678a6061d0906ee6))
    - Merge pull request #252 from JohnTitor/clippy-fixes ([`22570b8`](https://github.com/rust-phf/rust-phf/commit/22570b89476248d22c9d77f315fd98e048c49700))
    - Fix some Clippy warnings ([`71fd47c`](https://github.com/rust-phf/rust-phf/commit/71fd47ca27a8b1fe24b2eec75efd17ddfe11835f))
    - Merge pull request #251 from JohnTitor/weak-deps ([`2e1167c`](https://github.com/rust-phf/rust-phf/commit/2e1167c2046cd20aed1a906b4e23b40303cf0c00))
    - Make "unicase + macros" features work ([`11bb242`](https://github.com/rust-phf/rust-phf/commit/11bb2426f0237b1ecea8c8038630b1231ede4871))
    - Merge pull request #245 from JohnTitor/phf-0.10.1 ([`bed0153`](https://github.com/rust-phf/rust-phf/commit/bed01538ae576876f11189d541875d228acef9e8))
    - Prepare 0.10.1 release ([`4cc8344`](https://github.com/rust-phf/rust-phf/commit/4cc8344fad640ed71d75f557ce1a3b6eded321c3))
    - Merge pull request #244 from reitermarkus/serialize-map ([`a43e0e1`](https://github.com/rust-phf/rust-phf/commit/a43e0e19459201bac496030b9a7e30267c0e6dd4))
    - Allow serializing `Map`. ([`b6c682e`](https://github.com/rust-phf/rust-phf/commit/b6c682e81ea537b967ba055a0e464d24f5ea795c))
    - Merge pull request #243 from birkenfeld/patch-1 ([`815c17c`](https://github.com/rust-phf/rust-phf/commit/815c17cfa80a5087f91d24d56c7dae600a0df4c0))
    - State allowed key expressions in `phf_map` ([`6be1599`](https://github.com/rust-phf/rust-phf/commit/6be1599d7a0df27fd1888c78d247f8810cb8f750))
    - Merge pull request #240 from JohnTitor/docs-update ([`da98b9e`](https://github.com/rust-phf/rust-phf/commit/da98b9e80fdb22cd6d48a4a42489840afe603756))
    - Remove some stuff which is now unnecessary ([`6941e82`](https://github.com/rust-phf/rust-phf/commit/6941e825d09a98c1ea29a08ecd5fd605611584a4))
    - Refine doc comments ([`d8cfc43`](https://github.com/rust-phf/rust-phf/commit/d8cfc436059a1c2c3ede1afb0f9ec2333c046fc6))
    - Merge pull request #234 from JohnTitor/fix-ci ([`eba4cc2`](https://github.com/rust-phf/rust-phf/commit/eba4cc28d92c1db95cc430985a0fbc9ca63d1307))
    - Fix CI failure ([`d9b5ff2`](https://github.com/rust-phf/rust-phf/commit/d9b5ff23367d2bbcc385ff8243c7d972f45d459c))
    - Merge pull request #230 from JohnTitor/release-0.10 ([`3ea14b2`](https://github.com/rust-phf/rust-phf/commit/3ea14b2166553ad6e7b9afe7244144f5d661b6c6))
    - Prepare for release 0.10.0 ([`588ac25`](https://github.com/rust-phf/rust-phf/commit/588ac25dd5c0afccea084e6f94867328a6a30454))
    - Merge pull request #228 from JohnTitor/release-0.9.1 ([`d527f9d`](https://github.com/rust-phf/rust-phf/commit/d527f9d016adafe7d2930e37710291030b432838))
    - Prepare for v0.9.1 ([`9b71978`](https://github.com/rust-phf/rust-phf/commit/9b719789149ef195ef5eba093b7e73255fbef8dc))
    - Merge pull request #226 from bhgomes/iterator-traits ([`012be08`](https://github.com/rust-phf/rust-phf/commit/012be08aa1bc23092539bf617317243e672c75b1))
    - Add trait implementations to iterators mirroring std::collections ([`e47e4dc`](https://github.com/rust-phf/rust-phf/commit/e47e4dce434fd8d0ee80a3c57880f6b2465eed90))
    - Merge pull request #224 from bhgomes/const-fns ([`65deaf7`](https://github.com/rust-phf/rust-phf/commit/65deaf745b5175b6b8e645b6c66e53fc55bb3a85))
    - Remove Slice type and fix some docs ([`99d3533`](https://github.com/rust-phf/rust-phf/commit/99d353390f8124a283da9202fd4d163e68bc1949))
    - Add len/is_empty const-fns ([`f474922`](https://github.com/rust-phf/rust-phf/commit/f4749220eec2fccef35a66de323c01704a0eeda1))
    - Merge pull request #223 from JohnTitor/minor-cleanup ([`c746106`](https://github.com/rust-phf/rust-phf/commit/c746106ad05917ad62f244504727b07e07c3e075))
    - Minor cleanups ([`8868d08`](https://github.com/rust-phf/rust-phf/commit/8868d088e2fed36fcd7741e9a1c5bf68bef4f46e))
    - Merge pull request #222 from JohnTitor/precisify-msrv ([`50f8a0d`](https://github.com/rust-phf/rust-phf/commit/50f8a0d3d3f4cc7e15146e29e0559ba057a25a4d))
    - Precisify MSRV ([`63886f6`](https://github.com/rust-phf/rust-phf/commit/63886f6eb0d53d5bf44a10c713066b090686b8e2))
    - Merge pull request #219 from JohnTitor/release-0.9.0 ([`307969f`](https://github.com/rust-phf/rust-phf/commit/307969ff3bb8cae320e648890a9525920035944b))
    - Prepare 0.9.0 release ([`2ca46c4`](https://github.com/rust-phf/rust-phf/commit/2ca46c4f9c9083c128fcc6add33dc5986638940f))
    - Cleanup cargo metadata ([`a9e4b0a`](https://github.com/rust-phf/rust-phf/commit/a9e4b0a1e84825004fa66e938b870f83d3147d0d))
    - Merge pull request #218 from JohnTitor/cleanup ([`76f9072`](https://github.com/rust-phf/rust-phf/commit/76f907239af9b0cca7dac4e6d702cedc72f6f371))
    - Run rustfmt ([`dd86c6c`](https://github.com/rust-phf/rust-phf/commit/dd86c6c103f25021b52144085b8fab0a94582bef))
    - Fix some clippy warnings ([`9adc370`](https://github.com/rust-phf/rust-phf/commit/9adc370ead7fbcc36cd0c74f495ab7631e0c9754))
    - Cleanup docs ([`ddecc3a`](https://github.com/rust-phf/rust-phf/commit/ddecc3aa97aec6d9e9d6e59c57bc598d476335c1))
    - Merge pull request #217 from JohnTitor/rename-feature ([`ff77659`](https://github.com/rust-phf/rust-phf/commit/ff77659a001c08f1f069a17cc5d2ff6fdd51569c))
    - Rename `unicase_support` to `unicase` ([`b47174b`](https://github.com/rust-phf/rust-phf/commit/b47174bb9ebbd68e41316e1aa39c6541a45356a6))
    - Merge pull request #197 from benesch/uncased ([`8b44f0c`](https://github.com/rust-phf/rust-phf/commit/8b44f0c4caf1a431426ff8dbae68f0693d6cef63))
    - Add support for uncased ([`2a6087f`](https://github.com/rust-phf/rust-phf/commit/2a6087fcaf99b445ff6013f693f7c4fe5d6f7387))
    - Merge pull request #211 from skyfloogle/ordered-phfborrow ([`6ec8afb`](https://github.com/rust-phf/rust-phf/commit/6ec8afb6d85121d2edb023fcf3626308a4b3dad4))
    - Replace `std::borrow::Borrow` with `PhfBorrow` for ordered maps and sets ([`f43a9cf`](https://github.com/rust-phf/rust-phf/commit/f43a9cf4aa2aefc9e743727697ec65a0ba6cc29e))
    - Merge pull request #174 from abonander/169-drop-borrow ([`3c087d4`](https://github.com/rust-phf/rust-phf/commit/3c087d4782be496e7955d2b51d5883c4ce64ccd3))
    - Replace uses of `std::borrow::Borrow` with new `PhfBorrow` trait ([`b2f3a9c`](https://github.com/rust-phf/rust-phf/commit/b2f3a9c6a95ebabc2b0ae7ed1ec3ee7d72418e85))
    - Merge pull request #205 from skyfloogle/ordered-stuff ([`9ae1678`](https://github.com/rust-phf/rust-phf/commit/9ae1678f2507d6d26a1b780385a2e17bdfbb0b5c))
    - Add back ordered_map, ordered_set ([`0ab0108`](https://github.com/rust-phf/rust-phf/commit/0ab01081e4bd8f40bc18ab554c95f217220228d5))
    - Merge pull request #208 from JohnTitor/simplify-workspace ([`a47ac36`](https://github.com/rust-phf/rust-phf/commit/a47ac36b16dd8798659be3e24f74051cd1ed760d))
    - Use `[patch.crates-io]` section instead of path key ([`f47515b`](https://github.com/rust-phf/rust-phf/commit/f47515bce5c433214dbecee262a7a6f14e6a74d4))
    - Merge pull request #194 from pickfire/patch-1 ([`caec346`](https://github.com/rust-phf/rust-phf/commit/caec346b07cf04cc7850e4aeeca077856b79256a))
    - Merge pull request #190 from rjsberry/phf-shared-no-default-features ([`8dce12c`](https://github.com/rust-phf/rust-phf/commit/8dce12c4716cb7eeaedd5c7f5143b9c0450cedc2))
    - Fix style in doc ([`a285906`](https://github.com/rust-phf/rust-phf/commit/a28590675293af7c8faf866c1d847b7ed6876048))
    - Fix building with no_std ([`db4ce56`](https://github.com/rust-phf/rust-phf/commit/db4ce56082aafeb1aeee7e079d2bb4ae97ae58be))
    - Merge pull request #180 from abonander/master ([`81c7cc5`](https://github.com/rust-phf/rust-phf/commit/81c7cc5b48649108428671d3b8ad151f6fbdb359))
    - Release v0.8.0 ([`4060288`](https://github.com/rust-phf/rust-phf/commit/4060288dc2c1ebe3b0630e4016ed51935bb0c863))
    - Merge pull request #171 from abonander/170-removals ([`0d00821`](https://github.com/rust-phf/rust-phf/commit/0d0082178568036736bb6d51cb91f95ca5a616c3))
    - Remove ordered_map, ordered_set, phf_builder ([`8ae2bb8`](https://github.com/rust-phf/rust-phf/commit/8ae2bb886841a69a4fc482f439e2374f2373ab15))
    - Merge pull request #168 from abonander/167-std-default ([`a932094`](https://github.com/rust-phf/rust-phf/commit/a93209486f5874515da0483002e8669b2dbf95e6))
    - Switch optional `core` feature to default `std` feature ([`645e23d`](https://github.com/rust-phf/rust-phf/commit/645e23dda30ac1b99af39f201a74211e7ac3251a))
    - Merge pull request #164 from abonander/perf-improvements ([`70129c6`](https://github.com/rust-phf/rust-phf/commit/70129c6fbcdf428ce9f1014eea935301ac70e410))
    - Use two separate hashes and full 32-bit displacements ([`9b70bd9`](https://github.com/rust-phf/rust-phf/commit/9b70bd94f8b0b74f156e75ccefbd4a4c7ba29728))
    - Merge pull request #149 from danielhenrymantilla/proc-macro-hack ([`ae649cd`](https://github.com/rust-phf/rust-phf/commit/ae649cd67d9ce1452092ee739971d8ee232505ee))
    - Made macros work in stable ([`4fc0d1a`](https://github.com/rust-phf/rust-phf/commit/4fc0d1a8c3bcc3950082b614d8bfa4a0f63d6962))
    - Merge branch 'master' into patch-1 ([`cd0d7ce`](https://github.com/rust-phf/rust-phf/commit/cd0d7ce1194252dcaca3153988ba2a4effa66b4f))
    - Merge pull request #152 from abonander/unicase-upgrade ([`27f7c2c`](https://github.com/rust-phf/rust-phf/commit/27f7c2c85efde7aeb3c5409985f2d605aff8e05b))
    - Convert to 2018 edition ([`9ff66ab`](https://github.com/rust-phf/rust-phf/commit/9ff66ab36a23c7170cc775773f042a06de426c3b))
    - Merge pull request #145 from cetra3/empty_hash ([`2d3176b`](https://github.com/rust-phf/rust-phf/commit/2d3176b384112db5ca3fea08f1973ffc8a7c729b))
    - Fix & include tests for empty maps ([`83fd51c`](https://github.com/rust-phf/rust-phf/commit/83fd51c3095cbcd22b87c4d26ee22eb27a4e98d0))
    - Release v0.7.24 ([`1287414`](https://github.com/rust-phf/rust-phf/commit/1287414b1302d2d717c5f4be81accf4c12ccad48))
    - Docs for new macro setup ([`364ed47`](https://github.com/rust-phf/rust-phf/commit/364ed47c9f4401655fe7b897ce3e01e46706c286))
    - Fix feature name ([`e3a7442`](https://github.com/rust-phf/rust-phf/commit/e3a744255582aba8c743543503c9ad4c980a1ac3))
    - Reexport macros through phf crate ([`588fd1a`](https://github.com/rust-phf/rust-phf/commit/588fd1a785492afa5ad76db0556097e32e24387d))
    - Release v0.7.23 ([`a050b6f`](https://github.com/rust-phf/rust-phf/commit/a050b6f2a6b825bf0824339266ab9545340420d4))
    - Release 0.7.22 ([`ab88405`](https://github.com/rust-phf/rust-phf/commit/ab884054fa17eef915db2bdb5259c7aa71fbfea6))
    - Release v0.7.21 ([`6c7e2d9`](https://github.com/rust-phf/rust-phf/commit/6c7e2d9ce17ff1b87507925bdbe87e6e682ed3e4))
    - Typo ([`8d23b15`](https://github.com/rust-phf/rust-phf/commit/8d23b15361094b23c4eabacdb12f2dda386cc8e0))
    - Link to docs.rs ([`61142c5`](https://github.com/rust-phf/rust-phf/commit/61142c5aa168cff1bf53a6961ddc12012b49e1bb))
    - Release v0.7.20 ([`f631f50`](https://github.com/rust-phf/rust-phf/commit/f631f50abfaf6ea3d6fc8caaada47975b6df3a62))
    - Merge branch 'release' ([`ea7e256`](https://github.com/rust-phf/rust-phf/commit/ea7e2562706663632a0af65ae9fa94e5cf78c4ea))
    - Merge branch 'release-v0.7.19' into release ([`81a4806`](https://github.com/rust-phf/rust-phf/commit/81a4806b05f14fb49aa972de27a42926a542ec44))
    - Release v0.7.19 ([`0a98dd1`](https://github.com/rust-phf/rust-phf/commit/0a98dd1865d12a3fa4cc27bdb38fa1e7374940d9))
    - Merge branch 'release' ([`ecab54b`](https://github.com/rust-phf/rust-phf/commit/ecab54b8a028c88938f220dbb0a684e017bab62f))
    - Merge branch 'release-v0.7.18' into release ([`dfa970b`](https://github.com/rust-phf/rust-phf/commit/dfa970b229cc32cfb2da1692aa94ad8a266e704a))
    - Release v0.7.18 ([`3f71765`](https://github.com/rust-phf/rust-phf/commit/3f717650f4331f5dbb9d7a3f878228fcf1138729))
    - Merge branch 'release' ([`5f08563`](https://github.com/rust-phf/rust-phf/commit/5f0856327731107d9fada1b0318f6f15f32957c2))
    - Merge branch 'release-v0.7.17' into release ([`e073dd2`](https://github.com/rust-phf/rust-phf/commit/e073dd262d1b4c95234222ee5048fc883b9c7301))
    - Release v0.7.17 ([`21ecf72`](https://github.com/rust-phf/rust-phf/commit/21ecf72101715e4754db95a64ecd7de5a37b7f14))
    - Merge branch 'release' ([`839f06d`](https://github.com/rust-phf/rust-phf/commit/839f06d5a10c1300353b8f3c972990624695b668))
    - Merge branch 'release-v0.7.16' into release ([`6f5575c`](https://github.com/rust-phf/rust-phf/commit/6f5575c9b12d3619ea17c0825a613fcac12820f4))
    - Release v0.7.16 ([`8bf29c1`](https://github.com/rust-phf/rust-phf/commit/8bf29c10a878c83d73cc40385f0e96cb9cc95afa))
    - Merge branch 'release' ([`b4ec398`](https://github.com/rust-phf/rust-phf/commit/b4ec398f415e5cac2cd4d794b1889788e644447f))
    - Merge branch 'release-v0.7.15' into release ([`6bbc9e2`](https://github.com/rust-phf/rust-phf/commit/6bbc9e249b9a84e2019432b7d3b178851d2d776e))
    - Release v0.7.15 ([`20f896e`](https://github.com/rust-phf/rust-phf/commit/20f896e6975cabb9cf9883b08eaa5b3da8597f11))
    - Merge branch 'release' ([`7c692d4`](https://github.com/rust-phf/rust-phf/commit/7c692d42970bf6cb2540f6b2d3c88d63b3fd1f7a))
    - Merge branch 'release-v0.7.14' into release ([`ea8dd65`](https://github.com/rust-phf/rust-phf/commit/ea8dd652c292746a20bf3a680e9f925f6f0530b1))
    - Release v0.7.14 ([`fee66fc`](https://github.com/rust-phf/rust-phf/commit/fee66fc20e33f2b119f830a8926f3b6e52abcf09))
    - Merge pull request #82 from Ryman/unicase ([`909fac5`](https://github.com/rust-phf/rust-phf/commit/909fac5d4414a7d366432de078bcc6f78a25c230))
    - Add an impl of PhfHash for UniCase ([`d761144`](https://github.com/rust-phf/rust-phf/commit/d761144daf92ce6aed83165aa840a1ae72bd0bb2))
    - Drop all rust features ([`888f623`](https://github.com/rust-phf/rust-phf/commit/888f6234cd4e26e08b1f2d3716e4d4e0b95d0196))
    - Introduce a Slice abstraction for buffers ([`0cc3844`](https://github.com/rust-phf/rust-phf/commit/0cc38449c21f29bd9348e28c5719d650e16159cf))
    - Merge branch 'release' ([`d9351e1`](https://github.com/rust-phf/rust-phf/commit/d9351e1488bd42d1a4453e4a465177fb1c781fdc))
    - Merge branch 'release-v0.7.13' into release ([`b582e4e`](https://github.com/rust-phf/rust-phf/commit/b582e4ecec23be992ba915fc7873c0d5598f388a))
    - Release v0.7.13 ([`4769a6d`](https://github.com/rust-phf/rust-phf/commit/4769a6d2ce1d392da06e4b3cb833a1cdccb1f1aa))
    - Merge branch 'release' ([`5659a9d`](https://github.com/rust-phf/rust-phf/commit/5659a9db39bc5ee2179b264fce4cba4384d6d025))
    - Merge branch 'release-v0.7.12' into release ([`2f0a5de`](https://github.com/rust-phf/rust-phf/commit/2f0a5de9f01d9d22c774d8d85daec2a047a462e8))
    - Release v0.7.12 ([`9b75ee5`](https://github.com/rust-phf/rust-phf/commit/9b75ee5ed14060c45a5785fba0387be09e698624))
    - Merge pull request #75 from aidanhs/aphs-fix-ord-set-doc ([`ae5ee38`](https://github.com/rust-phf/rust-phf/commit/ae5ee38cad084144775d89fe38d8fdda33224697))
    - Fix ordered set `index` documentation ([`44e495f`](https://github.com/rust-phf/rust-phf/commit/44e495f634b1588ab148333cc582557f7877177f))
    - Merge branch 'release' ([`87ffab8`](https://github.com/rust-phf/rust-phf/commit/87ffab863aaeefb5ac2164da62f0407122d8057e))
    - Merge branch 'release-v0.7.11' into release ([`7260d04`](https://github.com/rust-phf/rust-phf/commit/7260d04413349bacab484afb74f9a496335278e1))
    - Release v0.7.11 ([`a004227`](https://github.com/rust-phf/rust-phf/commit/a0042277b181ec95fcbf29751b9a453f4f962ebb))
    - Merge branch 'release' ([`1579bec`](https://github.com/rust-phf/rust-phf/commit/1579bec1448c7b833f5965fe39d4ef2df66c982c))
    - Merge branch 'release-v0.7.10' into release ([`25cea13`](https://github.com/rust-phf/rust-phf/commit/25cea133fb4eec938bdfa74f04adbc8d94e30d4e))
    - Release v0.7.10 ([`c43154b`](https://github.com/rust-phf/rust-phf/commit/c43154b2661dc09620a7879c16f37b47d6ec03ae))
    - Merge branch 'release' ([`2c67ce5`](https://github.com/rust-phf/rust-phf/commit/2c67ce5a4129cd543178bf015f021a3bb83b6895))
    - Merge branch 'release-v0.7.9' into release ([`87206e1`](https://github.com/rust-phf/rust-phf/commit/87206e1c7b8d4089370dc168402ded0c0700a447))
    - Release v0.7.9 ([`b7d29df`](https://github.com/rust-phf/rust-phf/commit/b7d29dfe0df288b2da74de195f764eace1c8e443))
    - Merge branch 'release' ([`cd33902`](https://github.com/rust-phf/rust-phf/commit/cd339023e90ac1ce6971fa81badea65fb1f2b086))
    - Merge branch 'release-v0.7.8' into release ([`8bc23a0`](https://github.com/rust-phf/rust-phf/commit/8bc23a023908a038d668b6f7d8e94ee416995285))
    - Release v0.7.8 ([`aad0b9b`](https://github.com/rust-phf/rust-phf/commit/aad0b9b658fb970e3df60b066961aafca1a17c44))
    - Merge branch 'release' ([`dccff69`](https://github.com/rust-phf/rust-phf/commit/dccff69384729e3d4972174ce62d8f9db9429485))
    - Merge branch 'release-v0.7.7' into release ([`2d988b7`](https://github.com/rust-phf/rust-phf/commit/2d988b7dfb04d949246adc047f6b195263612246))
    - Release v0.7.7 ([`c9e7a93`](https://github.com/rust-phf/rust-phf/commit/c9e7a93f4d6f85a72651aba6187e4c956d8c1167))
    - Run through rustfmt ([`58e2223`](https://github.com/rust-phf/rust-phf/commit/58e222380b7fc9609a055cb5a6110ba04e47d677))
    - Merge branch 'release' ([`776046c`](https://github.com/rust-phf/rust-phf/commit/776046c961456dee9e16a6b6574d336c66e259f8))
    - Merge branch 'release-v0.7.6' into release ([`2ea7d5c`](https://github.com/rust-phf/rust-phf/commit/2ea7d5cab5e9e54952ca618b43ec3583a33a4847))
    - Release v0.7.6 ([`5bcd5c9`](https://github.com/rust-phf/rust-phf/commit/5bcd5c95215f5aa29e133cb2912662085a8158f0))
    - Fix core feature build ([`751c94b`](https://github.com/rust-phf/rust-phf/commit/751c94b208ded3b4d8ccff495513e4a55cb8fde0))
    - Use libstd debug builders ([`fd71c31`](https://github.com/rust-phf/rust-phf/commit/fd71c31288d72920a72eb73a69bc7325e7b1ba48))
    - Simplify no_std logic a bit ([`70f2ed9`](https://github.com/rust-phf/rust-phf/commit/70f2ed93d2e64b822bf2a23fde0ee848e8785bd1))
    - Merge pull request #68 from gz/master ([`44006f7`](https://github.com/rust-phf/rust-phf/commit/44006f74efca95d4f049bbf25df6321977c39577))
    - Reinstantiate no_std cargo feature flag. ([`7c3f757`](https://github.com/rust-phf/rust-phf/commit/7c3f757cdc83b4035d81f0d521b4b80b9080155e))
    - Merge branch 'release' ([`1f770df`](https://github.com/rust-phf/rust-phf/commit/1f770df1290b586a8d641ecb0bbd105080afc0ea))
    - Merge branch 'release-v0.7.5' into release ([`bb65b8c`](https://github.com/rust-phf/rust-phf/commit/bb65b8cca30ef9d4518e3083558019a972873efa))
    - Release v0.7.5 ([`fda44f5`](https://github.com/rust-phf/rust-phf/commit/fda44f550401c1bd4aad29bb2c07030b86761028))
    - Merge branch 'release' ([`269b5dc`](https://github.com/rust-phf/rust-phf/commit/269b5dc41ebf82f423393d5219e8107e9c911a03))
    - Merge branch 'release-v0.7.4' into release ([`7c093e8`](https://github.com/rust-phf/rust-phf/commit/7c093e83ffe5192d9cdcd5402b6abb7800ffafb3))
    - Release v0.7.4 ([`c7c0d3c`](https://github.com/rust-phf/rust-phf/commit/c7c0d3c294126157f0275a05b7c3a65c419234a1))
    - Merge pull request #62 from SimonSapin/string-cache ([`6f59718`](https://github.com/rust-phf/rust-phf/commit/6f5971869e5864cae653ec3606d17b554c343ef8))
    - Add hash() and get_index() to phf_shared. ([`d3b2ea0`](https://github.com/rust-phf/rust-phf/commit/d3b2ea0f0a9bd9cb79da90d8795f1905c3df1f5f))
    - Update PhfHash to mirror std::hash::Hash ([`96ef156`](https://github.com/rust-phf/rust-phf/commit/96ef156baae669b233673d6be2b96617ad48551e))
    - Release v0.7.3 ([`77ea239`](https://github.com/rust-phf/rust-phf/commit/77ea23917e908b10c4c5c463671a8409292f8661))
    - Merge pull request #59 from alexcrichton/update ([`6bd5a93`](https://github.com/rust-phf/rust-phf/commit/6bd5a939bda52281b0fa9844df1c42f1ce0220be))
    - Remove prelude imports ([`98183e1`](https://github.com/rust-phf/rust-phf/commit/98183e132a28b46af7bf72edd218549218d00776))
    - Release v0.7.2 ([`642b69d`](https://github.com/rust-phf/rust-phf/commit/642b69d0100a4ee7ec6e430ef1351bd1f28f9a4a))
    - Merge pull request #55 from SimonSapin/indexing ([`0cc37b2`](https://github.com/rust-phf/rust-phf/commit/0cc37b2f9e46e3c597373a8dfa669cc62acf5253))
    - Add `index` methods to `OrderedMap` and `OrderedSet`. ([`d2af00d`](https://github.com/rust-phf/rust-phf/commit/d2af00d4e32412d6f6b7597786976c1a0b642956))
    - Release v0.7.1 ([`9cb9de9`](https://github.com/rust-phf/rust-phf/commit/9cb9de911ad4e16964f0def29780dde1630c3619))
    - Release v0.7.0 ([`555a690`](https://github.com/rust-phf/rust-phf/commit/555a690561673597aee068650ac884bbcc2e31cf))
    - Stabilize phf ([`e215273`](https://github.com/rust-phf/rust-phf/commit/e2152739cbdd471116d88bb4a9cea4cdfede1e42))
    - Drop debug_builders feature ([`0b68ea5`](https://github.com/rust-phf/rust-phf/commit/0b68ea538639ebbdae032c9c3abefe547a60e982))
    - Release v0.6.19 ([`5810d30`](https://github.com/rust-phf/rust-phf/commit/5810d30ef2162f33cfb4da99c65b7344c7f2913b))
    - Clean up debug impls ([`7e32f39`](https://github.com/rust-phf/rust-phf/commit/7e32f399e150739c9cea3b9acd958d885d796372))
    - Merge pull request #53 from kmcallister/rustup ([`7f0392a`](https://github.com/rust-phf/rust-phf/commit/7f0392ad5ed9bb88a95d931f9c92e66a83aa039a))
    - Upgrade to rustc 1.0.0-dev (d8be84eb4 2015-03-29) (built 2015-03-29) ([`7d74f1f`](https://github.com/rust-phf/rust-phf/commit/7d74f1ff5eaa6a2963b97cdd7683e449681ff9aa))
    - Release v0.6.18 ([`36efc72`](https://github.com/rust-phf/rust-phf/commit/36efc721478d097fba1e5458cbdd9f288637abae))
    - Fix for upstream changes ([`eabadcf`](https://github.com/rust-phf/rust-phf/commit/eabadcf7e8af351ba8f07d86746e35adc8c5812e))
    - Release v0.6.17 ([`271ccc2`](https://github.com/rust-phf/rust-phf/commit/271ccc27d885363d4d8c549f75624d08c48e56c5))
    - Release v0.6.15 ([`ede14df`](https://github.com/rust-phf/rust-phf/commit/ede14df1e574674852b09bcafff4ad549ebfd4ae))
    - Release v0.6.14 ([`cf64ebb`](https://github.com/rust-phf/rust-phf/commit/cf64ebb8f769c9f12c9a03d05713dde6b8caf371))
    - Release v0.6.13 ([`4fdb533`](https://github.com/rust-phf/rust-phf/commit/4fdb5331fd9978ca3e180a06fb2e34627f50fb77))
    - Fix warnings and use debug builders ([`4d28684`](https://github.com/rust-phf/rust-phf/commit/4d28684b72333e911e23b898b5780947d49822a5))
    - Release v0.6.12 ([`59ca586`](https://github.com/rust-phf/rust-phf/commit/59ca58637206c9806c13cc24cb35cb7d0ce9d23f))
    - Release v0.6.11 ([`e1e6d3b`](https://github.com/rust-phf/rust-phf/commit/e1e6d3b40a6babddd0989406f2b4e952443ff52e))
    - Release v0.6.10 ([`fc45373`](https://github.com/rust-phf/rust-phf/commit/fc45373b34a461664f532c5108f3d2625172c128))
    - Add documentation for phf_macros ([`8eca797`](https://github.com/rust-phf/rust-phf/commit/8eca79711f33d04ad773a023581b6bd0a6f1efdc))
    - Move tests to phf_macros ([`40dbc32`](https://github.com/rust-phf/rust-phf/commit/40dbc328456003484716021cc317156967f1b2c1))
    - Remove core feature ([`d4c189a`](https://github.com/rust-phf/rust-phf/commit/d4c189a2b060df33e7c97d6c1f0f430b68fc23b5))
    - Release v0.6.9 ([`822f4e3`](https://github.com/rust-phf/rust-phf/commit/822f4e3fb127dc02d36d802803d71aa5b98bed3c))
    - Fix for upstream changes ([`f014882`](https://github.com/rust-phf/rust-phf/commit/f01488236a8e944f1b12b4bc441d55c10fc47aa1))
    - Release v0.6.8 ([`cd637ca`](https://github.com/rust-phf/rust-phf/commit/cd637cafb6d37b1901b6c119a7d26f253e9a288e))
    - Merge pull request #49 from kmcallister/rustup ([`ee54b59`](https://github.com/rust-phf/rust-phf/commit/ee54b59ff1eb87b10aa2df60b25887fcb0afa765))
    - Upgrade to rustc 1.0.0-nightly (6c065fc8c 2015-02-17) (built 2015-02-18) ([`cbd9a41`](https://github.com/rust-phf/rust-phf/commit/cbd9a41bdf3771eceeb1d4701e1d598b1321cdad))
    - .map(|t| t.clone()) -> .cloned() ([`044f690`](https://github.com/rust-phf/rust-phf/commit/044f6903cca0a3d656e4a738cc02b1d29d80c996))
    - Add example to root module docs ([`fbbb530`](https://github.com/rust-phf/rust-phf/commit/fbbb53094e52efa19ff225d3d3ef2cbc00b4a7af))
    - Release v0.6.7 ([`bfc36c9`](https://github.com/rust-phf/rust-phf/commit/bfc36c979225f652cdb72f3b1f2a25e77b50ab8c))
    - Release v0.6.6 ([`b09a174`](https://github.com/rust-phf/rust-phf/commit/b09a174a166c7744c5989bedc6ba68340f6f7fd1))
    - Fix for upstream changse ([`9bd8705`](https://github.com/rust-phf/rust-phf/commit/9bd870597fb26a109a4f33926a299729c00aea10))
    - Release v0.6.5 ([`271e784`](https://github.com/rust-phf/rust-phf/commit/271e7848f35b31d6ce9fc9268de173738464bfc8))
    - Fix for upstream changes ([`3db7cef`](https://github.com/rust-phf/rust-phf/commit/3db7cef414e4de28eb6c18938c275a3aafbdafa4))
    - Fix doc URLs ([`e1c53fc`](https://github.com/rust-phf/rust-phf/commit/e1c53fc3d79d896ec65677ed88eda2140468e124))
    - Move docs to this repo and auto build them ([`f8ef160`](https://github.com/rust-phf/rust-phf/commit/f8ef160480e2d4ce72fa7afb6ebce70e45acbc76))
    - Release v0.6.4 ([`6866c1b`](https://github.com/rust-phf/rust-phf/commit/6866c1bf5ad5091bc969f1356884aa86c27458cb))
    - Release v0.6.3 ([`b0c5e3c`](https://github.com/rust-phf/rust-phf/commit/b0c5e3cb69742f81160ea80a3ba1782a0b4e01a2))
    - Release v0.6.2 ([`d9ddf45`](https://github.com/rust-phf/rust-phf/commit/d9ddf45b15ba812b0d3acedffb08e901742e56c4))
    - Implement IntoIterator ([`2f63ded`](https://github.com/rust-phf/rust-phf/commit/2f63ded4b37f91215754545b828ca14a1aad2d32))
    - Link to libstd by default ([`24555b1`](https://github.com/rust-phf/rust-phf/commit/24555b19e6b54656633cc4ceac91864f14c20471))
    - Release v0.6.1 ([`ca0e9f6`](https://github.com/rust-phf/rust-phf/commit/ca0e9f6b9c737f3d11bcad2f4624bb5603a8170e))
    - Fix for upstream changes ([`69ca376`](https://github.com/rust-phf/rust-phf/commit/69ca376dc8daa094ab16f1fcbadb65f83a75939b))
    - Fix for stability changes ([`f7fb510`](https://github.com/rust-phf/rust-phf/commit/f7fb510dfe67f11522a2d214bd14d21f910bfd7b))
    - More sed fixes ([`81b54b2`](https://github.com/rust-phf/rust-phf/commit/81b54b22f2c87914a737fc4c650f95809ff1383e))
    - Release v0.6.0 ([`09d6870`](https://github.com/rust-phf/rust-phf/commit/09d687053caf4d321f72907528573b3334fae3c2))
    - Rename phf_mac to phf_macros ([`c50d107`](https://github.com/rust-phf/rust-phf/commit/c50d1077b1d53fccd703021911a7100b8937bbc7))
    - More fixes for bad sed ([`28af2aa`](https://github.com/rust-phf/rust-phf/commit/28af2aa411cc418025c8d04fd838db5cda6a792b))
    - Fix silly sed error ([`39e098a`](https://github.com/rust-phf/rust-phf/commit/39e098a7fb333cc046f4506f4c20cbc0d079c12f))
    - Show -> Debug ([`384ead4`](https://github.com/rust-phf/rust-phf/commit/384ead41f21d0cb2c46f3b6628e5ba9ee00f79c0))
    - Release v0.5.0 ([`8683be2`](https://github.com/rust-phf/rust-phf/commit/8683be260effe5605243ef230bad6154ef4e5e20))
    - Add type to Show implementations ([`c5a4f31`](https://github.com/rust-phf/rust-phf/commit/c5a4f3112e09d84332305bd7daff3a93691c7b3c))
    - Merge pull request #41 from alexcrichton/update ([`79772f4`](https://github.com/rust-phf/rust-phf/commit/79772f414fb18cedc33bf4ee95a9dcdbf9c0caad))
    - Remove unused features ([`88700a2`](https://github.com/rust-phf/rust-phf/commit/88700a2068c0901db8454119e3bcae5953d5b8a2))
    - Remove fmt::String impls for structures ([`5135f02`](https://github.com/rust-phf/rust-phf/commit/5135f029157d13bde463740e75140f9c4403edaa))
    - Release v0.4.9 ([`28cbe70`](https://github.com/rust-phf/rust-phf/commit/28cbe704e0f96495c2527ad93c5e67315c245908))
    - Fix for upstream changes ([`0b22188`](https://github.com/rust-phf/rust-phf/commit/0b22188f5767a0a125d01ed8b176ce19fef95cad))
    - Release v0.4.8 ([`bb858f1`](https://github.com/rust-phf/rust-phf/commit/bb858f11dd88579d47b0089121f8d551731464ab))
    - Merge pull request #38 from chris-morgan/master ([`668f986`](https://github.com/rust-phf/rust-phf/commit/668f986705ba3a6385b47b851878250ce954a6dc))
    - Release v0.4.7 ([`d83f551`](https://github.com/rust-phf/rust-phf/commit/d83f551a874a24b2a4308804e7cbca32a1aa2494))
    - Fix for upstream changes ([`c3ae5ac`](https://github.com/rust-phf/rust-phf/commit/c3ae5ac94cfa11404b420d45229c3a0d0d8a4535))
    - Release v0.4.6 ([`360bf81`](https://github.com/rust-phf/rust-phf/commit/360bf81ad3aafced75dc64a49e58a867d5239264))
    - Release v0.4.5 ([`ab4786c`](https://github.com/rust-phf/rust-phf/commit/ab4786c09b55e46658f2a66092caf6a782d056a6))
    - Fix for upstream changes ([`6963a16`](https://github.com/rust-phf/rust-phf/commit/6963a16a7619c3aa4a14ed880334e5712deae20e))
    - Release v0.4.4 ([`f678635`](https://github.com/rust-phf/rust-phf/commit/f678635378555b7d086014b0466aea12a3ae5701))
    - Fix for upstream changes ([`2b4863f`](https://github.com/rust-phf/rust-phf/commit/2b4863fcb5827d5bd89cc278d2a3052b6b3ee20e))
    - Release v0.4.3 ([`4f5902c`](https://github.com/rust-phf/rust-phf/commit/4f5902c222a81da009bf7955bc96568c73b46b13))
    - Fix for weird type inference breakage ([`3c36bfb`](https://github.com/rust-phf/rust-phf/commit/3c36bfbdd6ebfc1e544cbd38473f48e91406d965))
    - Release v0.4.2 ([`69d92b8`](https://github.com/rust-phf/rust-phf/commit/69d92b869fab51a31fda6126003edadd9e832b32))
    - Merge pull request #37 from alexcrichton/update ([`b9f0a43`](https://github.com/rust-phf/rust-phf/commit/b9f0a43500499fc08170690bdc6624f289e35841))
    - Update to rust master ([`4a0d48d`](https://github.com/rust-phf/rust-phf/commit/4a0d48d165d78d1b3e8f791503e220a032d26d24))
    - Release v0.4.1 ([`0fba837`](https://github.com/rust-phf/rust-phf/commit/0fba8374fd6fb1b10d9d456ae4b1310b00e9d9ca))
    - Release v0.4.0 ([`49dbb36`](https://github.com/rust-phf/rust-phf/commit/49dbb3636621c0436e771a4e0ebfe7342b676616))
    - Fix for upstream changes and drop xxhash ([`fc2539f`](https://github.com/rust-phf/rust-phf/commit/fc2539f7893ef0f833a8c13ec77ba317bd8bf43e))
    - Release v0.3.0 ([`0a80b06`](https://github.com/rust-phf/rust-phf/commit/0a80b06ecde77b33cec8c956c67704613fdd313e))
    - Fix for unboxed closure changes ([`d96a1e5`](https://github.com/rust-phf/rust-phf/commit/d96a1e5c7107eceb5cda147eb2ac3691ec534f68))
    - Rename Set and OrderedSet iterators ([`9103fc5`](https://github.com/rust-phf/rust-phf/commit/9103fc564121d90aa24adf1014ad82bc09119e0f))
    - Merge pull request #32 from sp3d/master ([`fc4829a`](https://github.com/rust-phf/rust-phf/commit/fc4829a292663e4e30a23a4ba1de693d154cd611))
    - Add support for [u8, ..N] keys ([`e26947c`](https://github.com/rust-phf/rust-phf/commit/e26947cc264266bcbc85b8cf5c46b2019d654c72))
    - Bump to 0.2 ([`4546f51`](https://github.com/rust-phf/rust-phf/commit/4546f51fccbd56ddf1214fe232db8926d9f471de))
    - Remove uneeded feature ([`98dde65`](https://github.com/rust-phf/rust-phf/commit/98dde65406865890af53618b7517ca8fcb2da5ad))
    - Alter entries iterator behavior ([`14627f5`](https://github.com/rust-phf/rust-phf/commit/14627f5696156b09bcc1150bee0318fa3c5c6c0f))
    - Bump to 0.1.0 ([`43d9a50`](https://github.com/rust-phf/rust-phf/commit/43d9a50e6240716d68dadd9d037f22b2f7df4b58))
    - Merge pull request #31 from jamesrhurst/exactsize ([`d20c311`](https://github.com/rust-phf/rust-phf/commit/d20c311e0e519c0ace07c0d2085d6d35e64a5ba8))
    - Make publishable on crates.io ([`4ad2bb2`](https://github.com/rust-phf/rust-phf/commit/4ad2bb27be35015b3f37ec7025c46df9170b3ef9))
    - ExactSize is now ExactSizeIterator ([`6a7cc6e`](https://github.com/rust-phf/rust-phf/commit/6a7cc6eb9ec08b103b6b62fa39bdb3229f3cdbe4))
    - Use repository packages ([`6e3a54d`](https://github.com/rust-phf/rust-phf/commit/6e3a54d1fee637c59e86b06ee5af67ab01039338))
    - Add license and descriptions ([`ff7dad4`](https://github.com/rust-phf/rust-phf/commit/ff7dad4cb8ad84d8fe05df2f1f32d959971eaa1c))
    - Update to use BorrowFrom ([`2f3c605`](https://github.com/rust-phf/rust-phf/commit/2f3c6053c2d754974a94aa45a49b8cce10ae88ba))
    - Merge pull request #28 from cgaebel/master ([`cc0d031`](https://github.com/rust-phf/rust-phf/commit/cc0d031772c1068781eaf64878ac2cd93499d6cf))
    - S/kv/entry/ ([`bf62eb8`](https://github.com/rust-phf/rust-phf/commit/bf62eb878981115492fbac99ff4d9f6c99858f72))
    - Merge pull request #27 from cgaebel/master ([`f6ce09a`](https://github.com/rust-phf/rust-phf/commit/f6ce09a25c4468b76a48fe4e1070436121084786))
    - More code review ([`aec5aab`](https://github.com/rust-phf/rust-phf/commit/aec5aab3a95bb96bd32b560598851dfc2f322fad))
    - Code review ([`88d54c2`](https://github.com/rust-phf/rust-phf/commit/88d54c2b875830bb00170421f3ea7d74eefe3f2b))
    - Added key+value equivalents for the map getters. ([`7ced000`](https://github.com/rust-phf/rust-phf/commit/7ced00017886acfe740ea70ba10b4d4cb9cf780f))
    - Switch from find to get ([`88abf6c`](https://github.com/rust-phf/rust-phf/commit/88abf6c8b081439c8cb1458289790d0ee8f4d04a))
    - Fix some deprecation warnings ([`af2dd53`](https://github.com/rust-phf/rust-phf/commit/af2dd53e131e950f29bb089e48bc9f42f621a9d7))
    - Update for collections traits removal ([`f585e4c`](https://github.com/rust-phf/rust-phf/commit/f585e4c88f1cd327e0b409c60deb51cd3f3d6b15))
    - Remove deprecated reexports ([`b697d13`](https://github.com/rust-phf/rust-phf/commit/b697d132b04f282bf489adde6cfe996adf8634fd))
    - Hide deprecated reexports from docs ([`d120067`](https://github.com/rust-phf/rust-phf/commit/d12006775117350d9c47e636aa3d4ba64e3a3454))
    - Add deprecated reexports ([`5752604`](https://github.com/rust-phf/rust-phf/commit/5752604bfa3d0aaad43dc4b1c50e986c6ee078e4))
    - Fix doc header size ([`8f5c0f0`](https://github.com/rust-phf/rust-phf/commit/8f5c0f0b491868a3811b434321f871892eab02c1))
    - Fix docs ([`eadea0b`](https://github.com/rust-phf/rust-phf/commit/eadea0b2c2cb9e76d0be9a209819c75a41434719))
    - Convert PhfOrderedSet to new naming conventions ([`de193c7`](https://github.com/rust-phf/rust-phf/commit/de193c767502a587d8bf4b81b6c5fb821e4a6b29))
    - Switch over PhfOrderedMap to new naming scheme ([`f17bae1`](https://github.com/rust-phf/rust-phf/commit/f17bae1c34380b0566207df8e54807f3773109ce))
    - Convert PhfSet to new naming conventions ([`b2416db`](https://github.com/rust-phf/rust-phf/commit/b2416db396bc0e35fd64fd23c367f26b5fe78f5a))
    - Move and rename PhfMap stuff ([`7fc934a`](https://github.com/rust-phf/rust-phf/commit/7fc934a23e7e25fd12014a123eea8f7707928338))
    - Update for Equiv DST changes ([`719de47`](https://github.com/rust-phf/rust-phf/commit/719de47be5881b070cdf948668ae3c71dcea51f6))
    - Clean up warnings ([`b44065b`](https://github.com/rust-phf/rust-phf/commit/b44065b78dd31d2931d5d4427b608ae907e841a9))
    - Fix docs ([`83a8116`](https://github.com/rust-phf/rust-phf/commit/83a8116c71bf1cbf28d51d269b4c214e13748509))
    - Drop libstd requirement ([`dd3d0f1`](https://github.com/rust-phf/rust-phf/commit/dd3d0f1fedc19bbea2795bb63b9ce623618f4e31))
    - Remove unneeded import ([`15cc179`](https://github.com/rust-phf/rust-phf/commit/15cc17901777ef3e8f9a7a95f15f11e5dd29eb57))
    - Update docs location ([`49647cd`](https://github.com/rust-phf/rust-phf/commit/49647cdd0c170be43956822cc31968ac96cd31b4))
    - Misc cleanup ([`2fe6940`](https://github.com/rust-phf/rust-phf/commit/2fe6940182240e39ecd283eef00c5eff1b343a08))
    - Use XXHash instead of SipHash ([`bd10658`](https://github.com/rust-phf/rust-phf/commit/bd10658648539a13553bd9ea8853f490ee424cc8))
    - Use slice operators ([`a1b5030`](https://github.com/rust-phf/rust-phf/commit/a1b503023f516753fcd95061b1b303d21bb44a91))
    - Fix warnings in tests ([`4bf6f82`](https://github.com/rust-phf/rust-phf/commit/4bf6f824795de3c587f554119cf8d6f88c438e53))
    - Remove old crate_name attributes ([`35701e2`](https://github.com/rust-phf/rust-phf/commit/35701e2591d78d76707453376fc32b3a53de08c0))
    - Fix typo ([`68458d3`](https://github.com/rust-phf/rust-phf/commit/68458d3255af0f58510c3b502dcff4d83af19ae8))
    - Rephrase order guarantees ([`3c2661d`](https://github.com/rust-phf/rust-phf/commit/3c2661d8a421d9f9ddccdcbc51a3386480fdf59d))
    - Update examples ([`85a3b28`](https://github.com/rust-phf/rust-phf/commit/85a3b28ea9ee24f080ff02d1db390284691714a9))
    - Minor cleanup ([`2f75f5f`](https://github.com/rust-phf/rust-phf/commit/2f75f5fed1579c1f26c42f8a263977fcec50f749))
    - Merge pull request #12 from kmcallister/find-index ([`d7ae880`](https://github.com/rust-phf/rust-phf/commit/d7ae8800202cd20cf057b865d4023b28fe80c8cc))
    - Provide find_index{,_equiv} on PhfOrdered{Set,Map} ([`b16d440`](https://github.com/rust-phf/rust-phf/commit/b16d4400556f7cae3e7dcca8ba091af5459090de))
    - Update for lifetime changes ([`af0a11c`](https://github.com/rust-phf/rust-phf/commit/af0a11c92bd531c9677bef31f6a6d8c4b59ad29b))
    - Add back crate_name for rustdoc ([`92ec57a`](https://github.com/rust-phf/rust-phf/commit/92ec57aca33e1dfeda7a6cadb0b0fd08ddc23808))
    - More cleanup ([`20dea1d`](https://github.com/rust-phf/rust-phf/commit/20dea1d778a9e5226b6ffe2b11ed37a23878863a))
    - One more where clause ([`d6e5d77`](https://github.com/rust-phf/rust-phf/commit/d6e5d774a5ab6e796da0eb5e0cf062d0f0aebec0))
    - Switch to where clause syntax ([`13b9389`](https://github.com/rust-phf/rust-phf/commit/13b93899b5679d425fdfff7695003bc52d4c8f0b))
    - Cargo update ([`2a650ef`](https://github.com/rust-phf/rust-phf/commit/2a650efcdb9f013906cdf097e7a569c38d38487e))
    - Re-disable in-crate tests ([`9c4d247`](https://github.com/rust-phf/rust-phf/commit/9c4d247cb824689791e81942fd586e36899b35aa))
    - Properly support cross compiled builds ([`b2220d9`](https://github.com/rust-phf/rust-phf/commit/b2220d9a428049fb9c52b51c16d8f6b15cd02487))
    - Reenable tests for phf crate for docs ([`3ab5bd1`](https://github.com/rust-phf/rust-phf/commit/3ab5bd117af17cc8d91816b5911a65376f2a8f7f))
    - Update for pattern changes ([`f79814a`](https://github.com/rust-phf/rust-phf/commit/f79814a6abfa3bc5d739825643ea4ecee0a3aa8a))
    - Move test to tests dir ([`c9ca9b1`](https://github.com/rust-phf/rust-phf/commit/c9ca9b118f77e0581887c0bde09e78f9f7f00d0f))
    - Add more _equiv methods ([`61eea75`](https://github.com/rust-phf/rust-phf/commit/61eea759b53785fd8233a565de0765ce66fb824d))
    - Elide lifetimes ([`20a1e83`](https://github.com/rust-phf/rust-phf/commit/20a1e838c01017d74ef48cdb40e30eaf32de834a))
    - Impl Index for PhfMap and PhfOrderedMap ([`3995dbc`](https://github.com/rust-phf/rust-phf/commit/3995dbc443f33571e15c18c45b38862a515a88c0))
    - Switch Travis to using cargo ([`95f3c90`](https://github.com/rust-phf/rust-phf/commit/95f3c9074392b7782d28e6a94e79dfc303066ea2))
    - Rename module ([`25aeba6`](https://github.com/rust-phf/rust-phf/commit/25aeba6aeeb9f14ebabf11cd368f22840d40a245))
    - Rename phf_shared to phf ([`6372fa4`](https://github.com/rust-phf/rust-phf/commit/6372fa437f01de39cc80120f9d9ed48cee0f0b1f))
    - Turn off tests for main crates ([`6718b60`](https://github.com/rust-phf/rust-phf/commit/6718b60a55939992b7d4c5c00f57a4a81f38e5ac))
    - Pull shared code into a module ([`19c4f8d`](https://github.com/rust-phf/rust-phf/commit/19c4f8d420d3a9ff8e3ace0256198f5db9fccae0))
    - Move iterator maps to construction time ([`a8bb815`](https://github.com/rust-phf/rust-phf/commit/a8bb8156d513d0e15c476baac13a8d153f740958))
    - Implement more iterator traits for PhfMap iters ([`4b48972`](https://github.com/rust-phf/rust-phf/commit/4b4897284da11b59b4122c4b0c10b23064ca380c))
    - Add support for remaining literals ([`55ededf`](https://github.com/rust-phf/rust-phf/commit/55ededfc9ccbd3b01690e289adfc4d5e05a4064d))
    - Byte and char key support ([`789990e`](https://github.com/rust-phf/rust-phf/commit/789990ede8def8c333a305437899a953ed6f9a62))
    - Support binary literal keys! ([`6bfb12b`](https://github.com/rust-phf/rust-phf/commit/6bfb12bf3b0bffb66e44b8a5326051b58d697543))
    - Parameterize the key type of PhfOrdered* ([`f6ce641`](https://github.com/rust-phf/rust-phf/commit/f6ce641e5676be8d70e961f020d79fc3d6dcfb74))
    - Parameterize the key type of PhfMap and Set ([`cb4ed93`](https://github.com/rust-phf/rust-phf/commit/cb4ed93175b656f442802c27e039add8e2b86723))
    - Update for crate_id removal ([`a0ab8d7`](https://github.com/rust-phf/rust-phf/commit/a0ab8d7f517305c77cdb1d51076ff4b3e31923e5))
    - Split to two separate Cargo packages ([`4ff3544`](https://github.com/rust-phf/rust-phf/commit/4ff35445a4b376009d0f365bd761c2c27c174c4c))
</details>

