# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 0.11.3 (2025-01-07)

### Other

 - <csr-id-9ed628fe2ca954fdc49a93331188a99b58c9363a/> include LICENSE and changelog files in published crates
   The restrictive "include" directive is only present in the phf_macros
   crate but in none of the others. This commit brings phf_macros crate
   in line with other crates in this workspace.

### Bug Fixes

 - <csr-id-b54c740086a96da85056b0df28174122bd73d5b0/> Add isize/usize as valid key types
   These types already implement PhfHash as of #262, but were not supported
   as valid key expressions in `phf_map!` and co.

### Chore

 - <csr-id-a96a4e29d63fb1ab3cc10e050571e733f5d2d0d1/> bump Cargo.toml version of phf and phf_macros

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 10 commits contributed to the release.
 - 562 days passed between releases.
 - 3 commits were understood as [conventional](https://www.conventionalcommits.org).
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
    - Add isize/usize as valid key types ([`b54c740`](https://github.com/rust-phf/rust-phf/commit/b54c740086a96da85056b0df28174122bd73d5b0))
    - Merge pull request #300 from JohnTitor/msrv-1.61 ([`323366d`](https://github.com/rust-phf/rust-phf/commit/323366d03966ddad2eaa3432df79c9da8339e319))
    - Bump MSRV to 1.61 ([`1795f7b`](https://github.com/rust-phf/rust-phf/commit/1795f7b66b16af0191f221dc957bc8a090c891ad))
    - Merge pull request #293 from decathorpe/master ([`e03f456`](https://github.com/rust-phf/rust-phf/commit/e03f4562957afd89d8d95a19d563eda9f0db7e8c))
    - Include LICENSE and changelog files in published crates ([`9ed628f`](https://github.com/rust-phf/rust-phf/commit/9ed628fe2ca954fdc49a93331188a99b58c9363a))
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
    - Merge pull request #280 from jf2048/deref-bytestring ([`3776342`](https://github.com/rust-phf/rust-phf/commit/377634245c8c6f0569a2ed7b75d08366b54c8810))
    - Merge pull request #284 from nickelc/deps/syn2 ([`5ec8936`](https://github.com/rust-phf/rust-phf/commit/5ec8936369ca9eb6392a4aeb878d9bfef88d0d17))
    - Update `syn` to 2.0 ([`8e3e3e5`](https://github.com/rust-phf/rust-phf/commit/8e3e3e554433a2bcb6bf84805b1d03a49780d8c3))
    - Allow using dereferenced bytestring literal keys in phf_map! ([`8c0d057`](https://github.com/rust-phf/rust-phf/commit/8c0d0572da8c0b5e188e7fda4ab8bd4bcb97f720))
    - Merge pull request #274 from ankane/license-files ([`21baa73`](https://github.com/rust-phf/rust-phf/commit/21baa73941a0694ec48f437c0c0a6abfcc2f32d2))
    - Include license files in crates ([`1229b2f`](https://github.com/rust-phf/rust-phf/commit/1229b2faa6b97542ab4850a1723b1723dea92814))
    - Merge pull request #271 from DavidS/bump-dep ([`ea8df2c`](https://github.com/rust-phf/rust-phf/commit/ea8df2caad5b20f927be1f0174dfa4e68e8a95f6))
    - Fix missed dependency bump in phf_macros ([`b7fd8f1`](https://github.com/rust-phf/rust-phf/commit/b7fd8f183f266cf7f0bf0ca8e89b03453f3f35b7))
</details>

## 0.11.1 (2022-08-08)

<csr-id-d40d663ca96f668bcd6f86cc691085629111c0b5/>
<csr-id-8225c4b90d6ee71483304e71342c269fca86a044/>

### Chore

 - <csr-id-d40d663ca96f668bcd6f86cc691085629111c0b5/> upgrade syn/proc-macro

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

### Other

 - <csr-id-8225c4b90d6ee71483304e71342c269fca86a044/> Update code for changes in Rust
   LitBinary is now LitByteStr

### Commit Statistics

<csr-read-only-do-not-edit/>

 - 232 commits contributed to the release.
 - 3 commits were understood as [conventional](https://www.conventionalcommits.org).
 - 0 issues like '(#ID)' were seen in commit messages

### Commit Details

<csr-read-only-do-not-edit/>

<details><summary>view details</summary>

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
    - Merge pull request #251 from JohnTitor/weak-deps ([`2e1167c`](https://github.com/rust-phf/rust-phf/commit/2e1167c2046cd20aed1a906b4e23b40303cf0c00))
    - Make "unicase + macros" features work ([`11bb242`](https://github.com/rust-phf/rust-phf/commit/11bb2426f0237b1ecea8c8038630b1231ede4871))
    - Merge pull request #241 from JohnTitor/extract-macro-tests ([`7b0a313`](https://github.com/rust-phf/rust-phf/commit/7b0a3130a55176d2570300f92cb7ddca6c23da83))
    - Extract `phf_macros` tests as a separated crate ([`8cf694d`](https://github.com/rust-phf/rust-phf/commit/8cf694d76e0991b4e24ecdc5d2a88bb74713d9cd))
    - Merge pull request #240 from JohnTitor/docs-update ([`da98b9e`](https://github.com/rust-phf/rust-phf/commit/da98b9e80fdb22cd6d48a4a42489840afe603756))
    - Remove some stuff which is now unnecessary ([`6941e82`](https://github.com/rust-phf/rust-phf/commit/6941e825d09a98c1ea29a08ecd5fd605611584a4))
    - Refine doc comments ([`d8cfc43`](https://github.com/rust-phf/rust-phf/commit/d8cfc436059a1c2c3ede1afb0f9ec2333c046fc6))
    - Merge pull request #234 from JohnTitor/fix-ci ([`eba4cc2`](https://github.com/rust-phf/rust-phf/commit/eba4cc28d92c1db95cc430985a0fbc9ca63d1307))
    - Fix CI failure ([`d9b5ff2`](https://github.com/rust-phf/rust-phf/commit/d9b5ff23367d2bbcc385ff8243c7d972f45d459c))
    - Fix `phf` dev dep version ([`3cc6f05`](https://github.com/rust-phf/rust-phf/commit/3cc6f05cb07933af4cf886645d1170bdcb306b6b))
    - Merge pull request #230 from JohnTitor/release-0.10 ([`3ea14b2`](https://github.com/rust-phf/rust-phf/commit/3ea14b2166553ad6e7b9afe7244144f5d661b6c6))
    - Prepare for release 0.10.0 ([`588ac25`](https://github.com/rust-phf/rust-phf/commit/588ac25dd5c0afccea084e6f94867328a6a30454))
    - Fix publish failure ([`fbb18f9`](https://github.com/rust-phf/rust-phf/commit/fbb18f925018fa621ce8a8d334f6746ae0f1d072))
    - Merge pull request #228 from JohnTitor/release-0.9.1 ([`d527f9d`](https://github.com/rust-phf/rust-phf/commit/d527f9d016adafe7d2930e37710291030b432838))
    - Prepare for v0.9.1 ([`9b71978`](https://github.com/rust-phf/rust-phf/commit/9b719789149ef195ef5eba093b7e73255fbef8dc))
    - Merge pull request #224 from bhgomes/const-fns ([`65deaf7`](https://github.com/rust-phf/rust-phf/commit/65deaf745b5175b6b8e645b6c66e53fc55bb3a85))
    - Remove Slice type and fix some docs ([`99d3533`](https://github.com/rust-phf/rust-phf/commit/99d353390f8124a283da9202fd4d163e68bc1949))
    - Merge pull request #223 from JohnTitor/minor-cleanup ([`c746106`](https://github.com/rust-phf/rust-phf/commit/c746106ad05917ad62f244504727b07e07c3e075))
    - Minor cleanups ([`8868d08`](https://github.com/rust-phf/rust-phf/commit/8868d088e2fed36fcd7741e9a1c5bf68bef4f46e))
    - Merge pull request #222 from JohnTitor/precisify-msrv ([`50f8a0d`](https://github.com/rust-phf/rust-phf/commit/50f8a0d3d3f4cc7e15146e29e0559ba057a25a4d))
    - Bless tests ([`dab668c`](https://github.com/rust-phf/rust-phf/commit/dab668ccc8b638548cd78678de8427ed5e765b21))
    - Merge pull request #220 from JohnTitor/fix-release-process ([`29f9100`](https://github.com/rust-phf/rust-phf/commit/29f910079b75623420a19f3bd91a341821e02118))
    - Fix the release failure ([`647f331`](https://github.com/rust-phf/rust-phf/commit/647f331d43dcf2b61625cccffbd31f95ad076d05))
    - Downgrade `phf` dev-dep version for now ([`7dd8a1b`](https://github.com/rust-phf/rust-phf/commit/7dd8a1b410fea96820bfe489f53f1c6fd9d64ba5))
    - Merge pull request #219 from JohnTitor/release-0.9.0 ([`307969f`](https://github.com/rust-phf/rust-phf/commit/307969ff3bb8cae320e648890a9525920035944b))
    - Prepare 0.9.0 release ([`2ca46c4`](https://github.com/rust-phf/rust-phf/commit/2ca46c4f9c9083c128fcc6add33dc5986638940f))
    - Cleanup cargo metadata ([`a9e4b0a`](https://github.com/rust-phf/rust-phf/commit/a9e4b0a1e84825004fa66e938b870f83d3147d0d))
    - Merge pull request #218 from JohnTitor/cleanup ([`76f9072`](https://github.com/rust-phf/rust-phf/commit/76f907239af9b0cca7dac4e6d702cedc72f6f371))
    - Fix test ([`ffa7e41`](https://github.com/rust-phf/rust-phf/commit/ffa7e41a767dd6021a7f42f012dab0befe6d0932))
    - Run rustfmt check on CI ([`1adfb30`](https://github.com/rust-phf/rust-phf/commit/1adfb305704cbced7c63e58b99bd53847298dbe6))
    - Run rustfmt ([`dd86c6c`](https://github.com/rust-phf/rust-phf/commit/dd86c6c103f25021b52144085b8fab0a94582bef))
    - Merge pull request #217 from JohnTitor/rename-feature ([`ff77659`](https://github.com/rust-phf/rust-phf/commit/ff77659a001c08f1f069a17cc5d2ff6fdd51569c))
    - Rename `unicase_support` to `unicase` ([`b47174b`](https://github.com/rust-phf/rust-phf/commit/b47174bb9ebbd68e41316e1aa39c6541a45356a6))
    - Merge pull request #215 from rust-phf/gha ([`12121ec`](https://github.com/rust-phf/rust-phf/commit/12121ec6d16d79d73cf9a2a7cdae1681798351b4))
    - Run UI tests only on stable ([`7522b16`](https://github.com/rust-phf/rust-phf/commit/7522b160e76e981e430f6586dbfa8747c85f2f76))
    - Merge pull request #205 from skyfloogle/ordered-stuff ([`9ae1678`](https://github.com/rust-phf/rust-phf/commit/9ae1678f2507d6d26a1b780385a2e17bdfbb0b5c))
    - Add back ordered_map, ordered_set ([`0ab0108`](https://github.com/rust-phf/rust-phf/commit/0ab01081e4bd8f40bc18ab554c95f217220228d5))
    - Merge pull request #209 from JohnTitor/unicase_support ([`ec43f5c`](https://github.com/rust-phf/rust-phf/commit/ec43f5c912e48d7f56a4126fca8247733baee18f))
    - Improve implementation for unicase support ([`6957e47`](https://github.com/rust-phf/rust-phf/commit/6957e470b6fcd3b389440bf3d2ddcb12e1d38911))
    - Restore unicase_support for phf_macros ([`77e6cce`](https://github.com/rust-phf/rust-phf/commit/77e6cce1931fe8b43e434061a369f3620b3e97e0))
    - Merge pull request #208 from JohnTitor/simplify-workspace ([`a47ac36`](https://github.com/rust-phf/rust-phf/commit/a47ac36b16dd8798659be3e24f74051cd1ed760d))
    - Use `[patch.crates-io]` section instead of path key ([`f47515b`](https://github.com/rust-phf/rust-phf/commit/f47515bce5c433214dbecee262a7a6f14e6a74d4))
    - Merge pull request #206 from Kazurin-775/master ([`7ebc9e7`](https://github.com/rust-phf/rust-phf/commit/7ebc9e7986ca9ae86c6e871b4fd495a401d6b5ca))
    - Fix phf_macros on no_std ([`d7af3dc`](https://github.com/rust-phf/rust-phf/commit/d7af3dc96a67070e2f9000158d074825f0a9d592))
    - Merge pull request #207 from JohnTitor/fix-ci ([`5b42ba6`](https://github.com/rust-phf/rust-phf/commit/5b42ba673ac03299799a69b317dfff90a994b240))
    - Update stderrs ([`0f1407e`](https://github.com/rust-phf/rust-phf/commit/0f1407ec8aa6df74e7ed95dd073685295958d5d5))
    - Merge pull request #201 from benesch/rand-08-redux ([`73a6799`](https://github.com/rust-phf/rust-phf/commit/73a6799f048228039af32c8e21246a63d977c9e3))
    - Update expected test case output for latest nightly ([`e387f69`](https://github.com/rust-phf/rust-phf/commit/e387f69540138026ab679537322c94500876fe8d))
    - Merge pull request #180 from abonander/master ([`81c7cc5`](https://github.com/rust-phf/rust-phf/commit/81c7cc5b48649108428671d3b8ad151f6fbdb359))
    - Release v0.8.0 ([`4060288`](https://github.com/rust-phf/rust-phf/commit/4060288dc2c1ebe3b0630e4016ed51935bb0c863))
    - Merge pull request #181 from mati865/criterion ([`696eee1`](https://github.com/rust-phf/rust-phf/commit/696eee1f38213fe4a404ddfb9ef10d8e61ef0700))
    - Avoid missing main error in tests ([`1992222`](https://github.com/rust-phf/rust-phf/commit/19922229dfe8c25076ab13344a0b876fe2c3bda3))
    - Merge pull request #179 from FauxFaux/bumps ([`5f86fa4`](https://github.com/rust-phf/rust-phf/commit/5f86fa46ebf28eb6ef83d70d58b1212795639ba3))
    - Upgrade syn/proc-macro ([`d40d663`](https://github.com/rust-phf/rust-phf/commit/d40d663ca96f668bcd6f86cc691085629111c0b5))
    - Merge pull request #171 from abonander/170-removals ([`0d00821`](https://github.com/rust-phf/rust-phf/commit/0d0082178568036736bb6d51cb91f95ca5a616c3))
    - Remove ordered_map, ordered_set, phf_builder ([`8ae2bb8`](https://github.com/rust-phf/rust-phf/commit/8ae2bb886841a69a4fc482f439e2374f2373ab15))
    - Merge pull request #166 from abonander/158-trybuild ([`50c6c75`](https://github.com/rust-phf/rust-phf/commit/50c6c75d406b529601f0377afba93e562bbff2aa))
    - Port compile-fail tests to trybuild ([`4a4256c`](https://github.com/rust-phf/rust-phf/commit/4a4256cf1963a349c8d63f4f93c7c562e8963d59))
    - Merge pull request #161 from abonander/display-builders ([`171f7ed`](https://github.com/rust-phf/rust-phf/commit/171f7edccb71766e9381600108a0d996513ec7ea))
    - Create `Display` adapters for `phf_codegen` builders ([`93aa7ae`](https://github.com/rust-phf/rust-phf/commit/93aa7ae1de87345ea19f38e747283bc712384650))
    - Merge pull request #164 from abonander/perf-improvements ([`70129c6`](https://github.com/rust-phf/rust-phf/commit/70129c6fbcdf428ce9f1014eea935301ac70e410))
    - Ignore compiletest ([`f1362b2`](https://github.com/rust-phf/rust-phf/commit/f1362b25674538ed02d41fcc9f7cc1c8ba6ec57c))
    - Merge pull request #160 from abonander/readme-edits ([`6e1f6ac`](https://github.com/rust-phf/rust-phf/commit/6e1f6ac9b1f917089a4501ccb32f4f477799e39c))
    - Proc_macro_hygiene is not needed with proc-macro-hack ([`ab473a4`](https://github.com/rust-phf/rust-phf/commit/ab473a4c7fcc1a8e8a99594c261fe00b4ad96865))
    - Merge pull request #149 from danielhenrymantilla/proc-macro-hack ([`ae649cd`](https://github.com/rust-phf/rust-phf/commit/ae649cd67d9ce1452092ee739971d8ee232505ee))
    - Made macros work in stable ([`4fc0d1a`](https://github.com/rust-phf/rust-phf/commit/4fc0d1a8c3bcc3950082b614d8bfa4a0f63d6962))
    - Merge branch 'master' into patch-1 ([`cd0d7ce`](https://github.com/rust-phf/rust-phf/commit/cd0d7ce1194252dcaca3153988ba2a4effa66b4f))
    - Merge pull request #155 from abonander/128-bit-ints ([`6749552`](https://github.com/rust-phf/rust-phf/commit/674955292a7028752f2eb25e34c27e881f6b11a1))
    - Implement support for 128-bit ints and fix high magnitude vals ([`5be5919`](https://github.com/rust-phf/rust-phf/commit/5be59199389c0703fff62f640eb1a0d19243fc48))
    - Merge pull request #146 from Benjamin-L/master ([`d41f27d`](https://github.com/rust-phf/rust-phf/commit/d41f27d3e2bcbb4a2868a62b0e022b4bdb267d8b))
    - Fixed typo in benchmark ([`f46b2e1`](https://github.com/rust-phf/rust-phf/commit/f46b2e19622de2f845ea5eb8e8d4f54ece364242))
    - Fix tests ([`ae4ef3e`](https://github.com/rust-phf/rust-phf/commit/ae4ef3ea68d6baca0916b5ef2a15245ad78674ae))
    - Release v0.7.24 ([`1287414`](https://github.com/rust-phf/rust-phf/commit/1287414b1302d2d717c5f4be81accf4c12ccad48))
    - Reexport macros through phf crate ([`588fd1a`](https://github.com/rust-phf/rust-phf/commit/588fd1a785492afa5ad76db0556097e32e24387d))
    - Convert phf_macros to new-style proc-macros ([`5ae4131`](https://github.com/rust-phf/rust-phf/commit/5ae413129c391223782bc2944ec0ffbded103791))
    - Release v0.7.23 ([`a050b6f`](https://github.com/rust-phf/rust-phf/commit/a050b6f2a6b825bf0824339266ab9545340420d4))
    - Update to nightly-2018-08-23 ([`e03f536`](https://github.com/rust-phf/rust-phf/commit/e03f536f32a8a2a31d07e43b19e05c7d4fd1cb82))
    - Release 0.7.22 ([`ab88405`](https://github.com/rust-phf/rust-phf/commit/ab884054fa17eef915db2bdb5259c7aa71fbfea6))
    - Fix build ([`2071d25`](https://github.com/rust-phf/rust-phf/commit/2071d2515ff37590c45ee2e88cead583cdb81089))
    - Update to latest nightly ([`fcf758f`](https://github.com/rust-phf/rust-phf/commit/fcf758faa21c6c2c93dbab9fe6ac82a36bab0dd9))
    - Upgrade rand ([`e7b5a35`](https://github.com/rust-phf/rust-phf/commit/e7b5a35d14f6927a748f3c55a1c87b5b751ececd))
    - Release v0.7.21 ([`6c7e2d9`](https://github.com/rust-phf/rust-phf/commit/6c7e2d9ce17ff1b87507925bdbe87e6e682ed3e4))
    - Merge pull request #101 from SimonSapin/rustup ([`8889199`](https://github.com/rust-phf/rust-phf/commit/888919958cd0b8bb1ca81b3e4d59fdb6716d30f1))
    - Upgrade to rustc 1.16.0-nightly (c07a6ae77 2017-01-17) ([`dc756bf`](https://github.com/rust-phf/rust-phf/commit/dc756bfb1400715eeedd0dfaa394296274f59be4))
    - Don't ICE on bad syntax ([`e87e95f`](https://github.com/rust-phf/rust-phf/commit/e87e95fb96cfad1cc6699b828fb8994d2429f424))
    - Link to docs.rs ([`61142c5`](https://github.com/rust-phf/rust-phf/commit/61142c5aa168cff1bf53a6961ddc12012b49e1bb))
    - Cleanup ([`9278c47`](https://github.com/rust-phf/rust-phf/commit/9278c470b33571de286314cae555c4de9dd7d177))
    - Fix tests ([`5947cd1`](https://github.com/rust-phf/rust-phf/commit/5947cd14b9aac452f4f8feb25b57fd11240970ee))
    - Remove time dependency ([`98f56e5`](https://github.com/rust-phf/rust-phf/commit/98f56e53c212795e048c7baa0f488e1b294e9c37))
    - Dependency cleanup ([`f106aa6`](https://github.com/rust-phf/rust-phf/commit/f106aa66d85abfba3d627d12fd46a9b080c83e95))
    - Release v0.7.20 ([`f631f50`](https://github.com/rust-phf/rust-phf/commit/f631f50abfaf6ea3d6fc8caaada47975b6df3a62))
    - Merge pull request #96 from nox/rustup ([`2f509ca`](https://github.com/rust-phf/rust-phf/commit/2f509ca1a5e7910c3bc7aec773418098bc27d3ea))
    - Update to Rust 1.15.0-nightly (7b3eeea22 2016-11-21) ([`39cc485`](https://github.com/rust-phf/rust-phf/commit/39cc485f777daaf2076f1da7337cc5ad7e9f00ad))
    - Merge branch 'release' ([`ea7e256`](https://github.com/rust-phf/rust-phf/commit/ea7e2562706663632a0af65ae9fa94e5cf78c4ea))
    - Merge branch 'release-v0.7.19' into release ([`81a4806`](https://github.com/rust-phf/rust-phf/commit/81a4806b05f14fb49aa972de27a42926a542ec44))
    - Release v0.7.19 ([`0a98dd1`](https://github.com/rust-phf/rust-phf/commit/0a98dd1865d12a3fa4cc27bdb38fa1e7374940d9))
    - Merge pull request #95 from nox/rustup ([`969bcd5`](https://github.com/rust-phf/rust-phf/commit/969bcd57629b97f06f3cf05453e36cd584cd85f7))
    - Update phf_macros to Rust 1.14.0-nightly (7c69b0d5a 2016-11-01) ([`b7d2d4d`](https://github.com/rust-phf/rust-phf/commit/b7d2d4d36cb43a8fa159135250bd2265cb30f523))
    - Merge branch 'release' ([`ecab54b`](https://github.com/rust-phf/rust-phf/commit/ecab54b8a028c88938f220dbb0a684e017bab62f))
    - Merge branch 'release-v0.7.18' into release ([`dfa970b`](https://github.com/rust-phf/rust-phf/commit/dfa970b229cc32cfb2da1692aa94ad8a266e704a))
    - Release v0.7.18 ([`3f71765`](https://github.com/rust-phf/rust-phf/commit/3f717650f4331f5dbb9d7a3f878228fcf1138729))
    - Merge pull request #94 from Bobo1239/master ([`81f2a5d`](https://github.com/rust-phf/rust-phf/commit/81f2a5d7bc9897711a064b343b8a8b6216e252b7))
    - Fix for latest nightly ([`35e991b`](https://github.com/rust-phf/rust-phf/commit/35e991b11efca3bd065a28f661ab76f423a83601))
    - Merge branch 'release' ([`5f08563`](https://github.com/rust-phf/rust-phf/commit/5f0856327731107d9fada1b0318f6f15f32957c2))
    - Merge branch 'release-v0.7.17' into release ([`e073dd2`](https://github.com/rust-phf/rust-phf/commit/e073dd262d1b4c95234222ee5048fc883b9c7301))
    - Release v0.7.17 ([`21ecf72`](https://github.com/rust-phf/rust-phf/commit/21ecf72101715e4754db95a64ecd7de5a37b7f14))
    - Merge pull request #92 from Bobo1239/master ([`d4b788d`](https://github.com/rust-phf/rust-phf/commit/d4b788dbce05fa8e103bd9d0a3022230ae738b81))
    - Fix for latest nightly ([`cb1ec95`](https://github.com/rust-phf/rust-phf/commit/cb1ec955442750fc712d155346beeb9562905602))
    - Merge pull request #91 from Bobo1239/master ([`bf472f2`](https://github.com/rust-phf/rust-phf/commit/bf472f2baed1552530a80c95ba5872a78fd68a5c))
    - Remove dead code ([`df0d8e8`](https://github.com/rust-phf/rust-phf/commit/df0d8e8ae9b23482fb19ca70f1f3bd6cdfe59358))
    - Add compile-fail test for equivalent UniCase keys ([`711515a`](https://github.com/rust-phf/rust-phf/commit/711515ad0ab53c14303b6c659a1fb3c2b3c86df5))
    - Add UniCase support to phf_macros and bump unicase version ([`2af3abb`](https://github.com/rust-phf/rust-phf/commit/2af3abb00cafc85d43755e43767a2a8b274f6670))
    - Merge branch 'release' ([`839f06d`](https://github.com/rust-phf/rust-phf/commit/839f06d5a10c1300353b8f3c972990624695b668))
    - Merge branch 'release-v0.7.16' into release ([`6f5575c`](https://github.com/rust-phf/rust-phf/commit/6f5575c9b12d3619ea17c0825a613fcac12820f4))
    - Release v0.7.16 ([`8bf29c1`](https://github.com/rust-phf/rust-phf/commit/8bf29c10a878c83d73cc40385f0e96cb9cc95afa))
    - Merge pull request #89 from Machtan/master ([`ce387c3`](https://github.com/rust-phf/rust-phf/commit/ce387c3e2fb64ee031e812b93a64064098c5d617))
    - Update the TokenTree import ([`f404629`](https://github.com/rust-phf/rust-phf/commit/f40462989e75ce85de8c88d6faaee934d05fe006))
    - Merge branch 'release' ([`b4ec398`](https://github.com/rust-phf/rust-phf/commit/b4ec398f415e5cac2cd4d794b1889788e644447f))
    - Merge branch 'release-v0.7.15' into release ([`6bbc9e2`](https://github.com/rust-phf/rust-phf/commit/6bbc9e249b9a84e2019432b7d3b178851d2d776e))
    - Release v0.7.15 ([`20f896e`](https://github.com/rust-phf/rust-phf/commit/20f896e6975cabb9cf9883b08eaa5b3da8597f11))
    - Merge branch 'release' ([`7c692d4`](https://github.com/rust-phf/rust-phf/commit/7c692d42970bf6cb2540f6b2d3c88d63b3fd1f7a))
    - Merge branch 'release-v0.7.14' into release ([`ea8dd65`](https://github.com/rust-phf/rust-phf/commit/ea8dd652c292746a20bf3a680e9f925f6f0530b1))
    - Release v0.7.14 ([`fee66fc`](https://github.com/rust-phf/rust-phf/commit/fee66fc20e33f2b119f830a8926f3b6e52abcf09))
    - Introduce a Slice abstraction for buffers ([`0cc3844`](https://github.com/rust-phf/rust-phf/commit/0cc38449c21f29bd9348e28c5719d650e16159cf))
    - Merge branch 'release' ([`d9351e1`](https://github.com/rust-phf/rust-phf/commit/d9351e1488bd42d1a4453e4a465177fb1c781fdc))
    - Merge branch 'release-v0.7.13' into release ([`b582e4e`](https://github.com/rust-phf/rust-phf/commit/b582e4ecec23be992ba915fc7873c0d5598f388a))
    - Release v0.7.13 ([`4769a6d`](https://github.com/rust-phf/rust-phf/commit/4769a6d2ce1d392da06e4b3cb833a1cdccb1f1aa))
    - Merge pull request #80 from nox/rustup ([`6d17c1f`](https://github.com/rust-phf/rust-phf/commit/6d17c1ffe01d82eaeb0d087762c73ed6ab288bbe))
    - Update to Rust 2016-02-22 ([`c995514`](https://github.com/rust-phf/rust-phf/commit/c9955143ffdb07bf85a525494811bd96517bf688))
    - Merge branch 'release' ([`5659a9d`](https://github.com/rust-phf/rust-phf/commit/5659a9db39bc5ee2179b264fce4cba4384d6d025))
    - Merge branch 'release-v0.7.12' into release ([`2f0a5de`](https://github.com/rust-phf/rust-phf/commit/2f0a5de9f01d9d22c774d8d85daec2a047a462e8))
    - Release v0.7.12 ([`9b75ee5`](https://github.com/rust-phf/rust-phf/commit/9b75ee5ed14060c45a5785fba0387be09e698624))
    - Merge pull request #77 from nox/byte-string-key ([`75606bc`](https://github.com/rust-phf/rust-phf/commit/75606bc371b532dddb814588bc65a9a2a5343ddb))
    - Support byte string keys in phf_macros (fixes #76) ([`652beae`](https://github.com/rust-phf/rust-phf/commit/652beae0cac6711ab0931d8dc844cd291559dad7))
    - Merge branch 'release' ([`87ffab8`](https://github.com/rust-phf/rust-phf/commit/87ffab863aaeefb5ac2164da62f0407122d8057e))
    - Merge branch 'release-v0.7.11' into release ([`7260d04`](https://github.com/rust-phf/rust-phf/commit/7260d04413349bacab484afb74f9a496335278e1))
    - Release v0.7.11 ([`a004227`](https://github.com/rust-phf/rust-phf/commit/a0042277b181ec95fcbf29751b9a453f4f962ebb))
    - Merge pull request #74 from djudd/fix-eat-retval ([`4791e96`](https://github.com/rust-phf/rust-phf/commit/4791e9602bc00e67bc9dd22fa55a58d7609d469c))
    - Update for changed return value of parser.eat ([`82da9f0`](https://github.com/rust-phf/rust-phf/commit/82da9f00f404634c09097f9116cda9e8e742d556))
    - Switch timing info back to a hint ([`771e781`](https://github.com/rust-phf/rust-phf/commit/771e781e704e581c1a103f56ed0f6f2a68917883))
    - Merge branch 'release' ([`1579bec`](https://github.com/rust-phf/rust-phf/commit/1579bec1448c7b833f5965fe39d4ef2df66c982c))
    - Merge branch 'release-v0.7.10' into release ([`25cea13`](https://github.com/rust-phf/rust-phf/commit/25cea133fb4eec938bdfa74f04adbc8d94e30d4e))
    - Release v0.7.10 ([`c43154b`](https://github.com/rust-phf/rust-phf/commit/c43154b2661dc09620a7879c16f37b47d6ec03ae))
    - Update for syntax changes ([`3be2db8`](https://github.com/rust-phf/rust-phf/commit/3be2db8d9254214bf1571fafd466ed7d6b96af55))
    - Merge branch 'release' ([`2c67ce5`](https://github.com/rust-phf/rust-phf/commit/2c67ce5a4129cd543178bf015f021a3bb83b6895))
    - Merge branch 'release-v0.7.9' into release ([`87206e1`](https://github.com/rust-phf/rust-phf/commit/87206e1c7b8d4089370dc168402ded0c0700a447))
    - Release v0.7.9 ([`b7d29df`](https://github.com/rust-phf/rust-phf/commit/b7d29dfe0df288b2da74de195f764eace1c8e443))
    - Merge pull request #71 from djudd/rustc-plugin-rename ([`260437e`](https://github.com/rust-phf/rust-phf/commit/260437ee8dc5fcad43654b07ccef101089cadabd))
    - Registry now seems to live in rustc_plugin instead of rustc::plugin ([`ba8d701`](https://github.com/rust-phf/rust-phf/commit/ba8d7019599cb779b9f7ab983f6cc2aa4f422991))
    - Merge branch 'release' ([`cd33902`](https://github.com/rust-phf/rust-phf/commit/cd339023e90ac1ce6971fa81badea65fb1f2b086))
    - Merge branch 'release-v0.7.8' into release ([`8bc23a0`](https://github.com/rust-phf/rust-phf/commit/8bc23a023908a038d668b6f7d8e94ee416995285))
    - Release v0.7.8 ([`aad0b9b`](https://github.com/rust-phf/rust-phf/commit/aad0b9b658fb970e3df60b066961aafca1a17c44))
    - Merge pull request #70 from nrc/rustup ([`2cc2ed3`](https://github.com/rust-phf/rust-phf/commit/2cc2ed36e30dea0ce0411784be87b184c0c68961))
    - Rustup ([`a6c43fa`](https://github.com/rust-phf/rust-phf/commit/a6c43fa25e06684121df6a93b2b90405d8e0fc2e))
    - Merge branch 'release' ([`dccff69`](https://github.com/rust-phf/rust-phf/commit/dccff69384729e3d4972174ce62d8f9db9429485))
    - Merge branch 'release-v0.7.7' into release ([`2d988b7`](https://github.com/rust-phf/rust-phf/commit/2d988b7dfb04d949246adc047f6b195263612246))
    - Release v0.7.7 ([`c9e7a93`](https://github.com/rust-phf/rust-phf/commit/c9e7a93f4d6f85a72651aba6187e4c956d8c1167))
    - Merge pull request #69 from nrc/rustup ([`8185728`](https://github.com/rust-phf/rust-phf/commit/81857284f30ff832f4c8eb7c68a2957f2acdb198))
    - Rustup for phf_macros ([`4c51ffc`](https://github.com/rust-phf/rust-phf/commit/4c51ffc6d63f768dea75cab65ad6cb809bce9bb4))
    - Run through rustfmt ([`58e2223`](https://github.com/rust-phf/rust-phf/commit/58e222380b7fc9609a055cb5a6110ba04e47d677))
    - Merge branch 'release' ([`776046c`](https://github.com/rust-phf/rust-phf/commit/776046c961456dee9e16a6b6574d336c66e259f8))
    - Merge branch 'release-v0.7.6' into release ([`2ea7d5c`](https://github.com/rust-phf/rust-phf/commit/2ea7d5cab5e9e54952ca618b43ec3583a33a4847))
    - Release v0.7.6 ([`5bcd5c9`](https://github.com/rust-phf/rust-phf/commit/5bcd5c95215f5aa29e133cb2912662085a8158f0))
    - Merge branch 'release' ([`1f770df`](https://github.com/rust-phf/rust-phf/commit/1f770df1290b586a8d641ecb0bbd105080afc0ea))
    - Merge branch 'release-v0.7.5' into release ([`bb65b8c`](https://github.com/rust-phf/rust-phf/commit/bb65b8cca30ef9d4518e3083558019a972873efa))
    - Release v0.7.5 ([`fda44f5`](https://github.com/rust-phf/rust-phf/commit/fda44f550401c1bd4aad29bb2c07030b86761028))
    - Merge pull request #65 from dinfuehr/master ([`fc1f6b0`](https://github.com/rust-phf/rust-phf/commit/fc1f6b00c5aeb00b1d1e5d418b5979c7cb8b8afd))
    - Update code for changes in Rust ([`8225c4b`](https://github.com/rust-phf/rust-phf/commit/8225c4b90d6ee71483304e71342c269fca86a044))
    - Macro assemble benchmark map and match to ensure sync ([`a2486ed`](https://github.com/rust-phf/rust-phf/commit/a2486eda19c647d16c9976bb33ba8634388a0569))
    - Merge pull request #63 from erickt/master ([`e879788`](https://github.com/rust-phf/rust-phf/commit/e8797888ff6f1a7a690a44844b692107cbf2c8a9))
    - Add benchmarks ([`9585cc3`](https://github.com/rust-phf/rust-phf/commit/9585cc3c0391725d02f6199eaed500ba5fafcaf3))
    - Merge branch 'release' ([`269b5dc`](https://github.com/rust-phf/rust-phf/commit/269b5dc41ebf82f423393d5219e8107e9c911a03))
    - Merge branch 'release-v0.7.4' into release ([`7c093e8`](https://github.com/rust-phf/rust-phf/commit/7c093e83ffe5192d9cdcd5402b6abb7800ffafb3))
    - Release v0.7.4 ([`c7c0d3c`](https://github.com/rust-phf/rust-phf/commit/c7c0d3c294126157f0275a05b7c3a65c419234a1))
    - Update PhfHash to mirror std::hash::Hash ([`96ef156`](https://github.com/rust-phf/rust-phf/commit/96ef156baae669b233673d6be2b96617ad48551e))
    - Release v0.7.3 ([`77ea239`](https://github.com/rust-phf/rust-phf/commit/77ea23917e908b10c4c5c463671a8409292f8661))
    - Release v0.7.2 ([`642b69d`](https://github.com/rust-phf/rust-phf/commit/642b69d0100a4ee7ec6e430ef1351bd1f28f9a4a))
    - Add an index test ([`f51f449`](https://github.com/rust-phf/rust-phf/commit/f51f449261ddd8ad30bfb5507b166e7980df1aa7))
    - Release v0.7.1 ([`9cb9de9`](https://github.com/rust-phf/rust-phf/commit/9cb9de911ad4e16964f0def29780dde1630c3619))
    - Fix phf-macros ([`6c98e9f`](https://github.com/rust-phf/rust-phf/commit/6c98e9f16a6d9ebf11e0a9c8e9ff91b4b320d2af))
    - Release v0.7.0 ([`555a690`](https://github.com/rust-phf/rust-phf/commit/555a690561673597aee068650ac884bbcc2e31cf))
    - Stabilize phf ([`e215273`](https://github.com/rust-phf/rust-phf/commit/e2152739cbdd471116d88bb4a9cea4cdfede1e42))
    - Release v0.6.19 ([`5810d30`](https://github.com/rust-phf/rust-phf/commit/5810d30ef2162f33cfb4da99c65b7344c7f2913b))
    - Release v0.6.18 ([`36efc72`](https://github.com/rust-phf/rust-phf/commit/36efc721478d097fba1e5458cbdd9f288637abae))
    - Fix for upstream changes ([`eabadcf`](https://github.com/rust-phf/rust-phf/commit/eabadcf7e8af351ba8f07d86746e35adc8c5812e))
    - Release v0.6.17 ([`271ccc2`](https://github.com/rust-phf/rust-phf/commit/271ccc27d885363d4d8c549f75624d08c48e56c5))
    - Release v0.6.15 ([`ede14df`](https://github.com/rust-phf/rust-phf/commit/ede14df1e574674852b09bcafff4ad549ebfd4ae))
    - Remove broken test ([`f54adb7`](https://github.com/rust-phf/rust-phf/commit/f54adb783a71678c9397b4d7c1e02ee82b9646b8))
    - Release v0.6.14 ([`cf64ebb`](https://github.com/rust-phf/rust-phf/commit/cf64ebb8f769c9f12c9a03d05713dde6b8caf371))
    - Release v0.6.13 ([`4fdb533`](https://github.com/rust-phf/rust-phf/commit/4fdb5331fd9978ca3e180a06fb2e34627f50fb77))
    - Fix warnings and use debug builders ([`4d28684`](https://github.com/rust-phf/rust-phf/commit/4d28684b72333e911e23b898b5780947d49822a5))
    - Release v0.6.12 ([`59ca586`](https://github.com/rust-phf/rust-phf/commit/59ca58637206c9806c13cc24cb35cb7d0ce9d23f))
    - Fix phf_macros ([`6567152`](https://github.com/rust-phf/rust-phf/commit/6567152be9e018a99fedf6e54017d827812b8f13))
    - Release v0.6.11 ([`e1e6d3b`](https://github.com/rust-phf/rust-phf/commit/e1e6d3b40a6babddd0989406f2b4e952443ff52e))
    - Release v0.6.10 ([`fc45373`](https://github.com/rust-phf/rust-phf/commit/fc45373b34a461664f532c5108f3d2625172c128))
    - Add doc URLs ([`4605db3`](https://github.com/rust-phf/rust-phf/commit/4605db3e7e0c4bef09ccf6c09c7dbcc36b707a9f))
    - Add documentation for phf_macros ([`8eca797`](https://github.com/rust-phf/rust-phf/commit/8eca79711f33d04ad773a023581b6bd0a6f1efdc))
    - Move generation logic to its own crate ([`cfeee87`](https://github.com/rust-phf/rust-phf/commit/cfeee8714caa4ecb3199df2a2ac149fe6a28ecc0))
    - Move tests to phf_macros ([`40dbc32`](https://github.com/rust-phf/rust-phf/commit/40dbc328456003484716021cc317156967f1b2c1))
    - Release v0.6.9 ([`822f4e3`](https://github.com/rust-phf/rust-phf/commit/822f4e3fb127dc02d36d802803d71aa5b98bed3c))
    - More fixes ([`0c04b9c`](https://github.com/rust-phf/rust-phf/commit/0c04b9cb2679a63394778a7362ef14441b6c2032))
    - Release v0.6.8 ([`cd637ca`](https://github.com/rust-phf/rust-phf/commit/cd637cafb6d37b1901b6c119a7d26f253e9a288e))
    - Release v0.6.7 ([`bfc36c9`](https://github.com/rust-phf/rust-phf/commit/bfc36c979225f652cdb72f3b1f2a25e77b50ab8c))
    - Fix for upstream changes ([`5ff7040`](https://github.com/rust-phf/rust-phf/commit/5ff70403a1b12c30206b128ac619b31c69e42eb4))
    - Merge pull request #47 from globin/fix/rustup ([`5aac93b`](https://github.com/rust-phf/rust-phf/commit/5aac93bad40ccac195e1f66614a29a9240dcaf54))
    - Rustup to current master ([`f6922e2`](https://github.com/rust-phf/rust-phf/commit/f6922e245752b4932f9a3a420c1f8d10e66e0b78))
    - Release v0.6.6 ([`b09a174`](https://github.com/rust-phf/rust-phf/commit/b09a174a166c7744c5989bedc6ba68340f6f7fd1))
    - Release v0.6.5 ([`271e784`](https://github.com/rust-phf/rust-phf/commit/271e7848f35b31d6ce9fc9268de173738464bfc8))
    - Move docs to this repo and auto build them ([`f8ef160`](https://github.com/rust-phf/rust-phf/commit/f8ef160480e2d4ce72fa7afb6ebce70e45acbc76))
    - Release v0.6.4 ([`6866c1b`](https://github.com/rust-phf/rust-phf/commit/6866c1bf5ad5091bc969f1356884aa86c27458cb))
    - Remove unused feature ([`2ee5f78`](https://github.com/rust-phf/rust-phf/commit/2ee5f788d493d929b669550c144ff23aad52721b))
    - Merge pull request #45 from Manishearth/internedstring ([`9b9c009`](https://github.com/rust-phf/rust-phf/commit/9b9c00934e33d920ab287765458d26ab321d8ab4))
    - InternedString.get() removal; brings us to rustc 1.0.0-dev (80627cd3c 2015-02-07 12:01:31 +0000) ([`3150bf0`](https://github.com/rust-phf/rust-phf/commit/3150bf0d608b051f2c8db3826ee21ce593f4f61c))
    - Release v0.6.3 ([`b0c5e3c`](https://github.com/rust-phf/rust-phf/commit/b0c5e3cb69742f81160ea80a3ba1782a0b4e01a2))
    - Use out of tree rand ([`9e1623b`](https://github.com/rust-phf/rust-phf/commit/9e1623bc7d1b8a432cdae47187eab40fa168401f))
    - Release v0.6.2 ([`d9ddf45`](https://github.com/rust-phf/rust-phf/commit/d9ddf45b15ba812b0d3acedffb08e901742e56c4))
    - Release v0.6.1 ([`ca0e9f6`](https://github.com/rust-phf/rust-phf/commit/ca0e9f6b9c737f3d11bcad2f4624bb5603a8170e))
    - Fix for stability changes ([`f7fb510`](https://github.com/rust-phf/rust-phf/commit/f7fb510dfe67f11522a2d214bd14d21f910bfd7b))
    - Release v0.6.0 ([`09d6870`](https://github.com/rust-phf/rust-phf/commit/09d687053caf4d321f72907528573b3334fae3c2))
    - Rename phf_mac to phf_macros ([`c50d107`](https://github.com/rust-phf/rust-phf/commit/c50d1077b1d53fccd703021911a7100b8937bbc7))
</details>

