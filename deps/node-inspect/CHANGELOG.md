### 1.10.4

* [`1c31bf7`](https://github.com/buggerjs/node-inspect/commit/1c31bf7d1b3ea1b424ae0662526596670cb506c9) **chore:** Support embedded mode


### 1.10.3

* [`7b20379`](https://github.com/buggerjs/node-inspect/commit/7b20379069af692a9038a31a4465f72db9eb532f) **chore:** Mark .eslintrc as root


### 1.10.2

* Run tests on windows - **[@jkrems](https://github.com/jkrems)** [#16](https://github.com/buggerjs/node-inspect/pull/16)
  - [`5a57f98`](https://github.com/buggerjs/node-inspect/commit/5a57f9865e02eef0763c2a7f26236c34a632ccdd) **chore:** Run tests on windows
  - [`0a04b50`](https://github.com/buggerjs/node-inspect/commit/0a04b50cc8b4dc6ce868927c635c479d75ce71f4) **chore:** Bump nlm to get rid of postinstall
  - [`4a8b27c`](https://github.com/buggerjs/node-inspect/commit/4a8b27cea814a37895effd2a0c1b85dbfee3a7f4) **test:** Remove unix path assumptions


### 1.10.1

* [`4ba3c72`](https://github.com/buggerjs/node-inspect/commit/4ba3c72270fae9a71343ddca11aa27980678a67c) **refactor:** Undo weird bundling into one file


### 1.10.0

* [`3e1a66a`](https://github.com/buggerjs/node-inspect/commit/3e1a66a489bef19beaa5f859e99e027274ff43cb) **feat:** Support CPU & heap profiles


### 1.9.3

* Move back to single file - **[@jkrems](https://github.com/jkrems)** [#15](https://github.com/buggerjs/node-inspect/pull/15)
  - [`9877660`](https://github.com/buggerjs/node-inspect/commit/9877660a73ff0ec0885ad7f939ba62020a46b4b6) **refactor:** Wrap client in IIFE
  - [`7795c53`](https://github.com/buggerjs/node-inspect/commit/7795c533f0605eb128db610a5874b27e555251ef) **refactor:** Move more code in createRepl scope
  - [`be34a39`](https://github.com/buggerjs/node-inspect/commit/be34a398e823612bdf5ac90bad5222af27035a00) **refactor:** Move back to single file
  - [`ab45b62`](https://github.com/buggerjs/node-inspect/commit/ab45b6273dc0d3a49d3cf46a80cb48ab79d1caf8) **refactor:** Remove single-use functions
  - [`37a711e`](https://github.com/buggerjs/node-inspect/commit/37a711ed5334c06ed4d85f995e567a9f176a68d5) **style:** Stop using `new Buffer`
  - [`d669dc5`](https://github.com/buggerjs/node-inspect/commit/d669dc593f5ad5ca7a48f19f0905ef66ec0e540d) **chore:** Switch to node eslint rules
  - [`15e7917`](https://github.com/buggerjs/node-inspect/commit/15e79177918d96dcffd2384715faf0308e97a26c) **style:** Use var in classical for loops


### 1.9.2

* [`c9dc4be`](https://github.com/buggerjs/node-inspect/commit/c9dc4beb08236e33d64f19417682cf5b3f5aeed6) **doc:** Link directly to GOVERNANCE file


### 1.9.1

* Handle big ws frames correctly - **[@jkrems](https://github.com/jkrems)** [#14](https://github.com/buggerjs/node-inspect/pull/14)
  - [`f80100e`](https://github.com/buggerjs/node-inspect/commit/f80100e932710d232d074b239cbf8fefa564c789) **fix:** Handle big ws frames correctly - see: [#10](https://github.com/buggerjs/node-inspect/issues/10)


### 1.9.0

* Support for low-level agent access - **[@jkrems](https://github.com/jkrems)** [#13](https://github.com/buggerjs/node-inspect/pull/13)
  - [`90ed431`](https://github.com/buggerjs/node-inspect/commit/90ed4310c62d130637c12f8ecdb752075c43ac36) **feat:** Support for low-level agent access


### 1.8.4

* Use proper path for websocket - **[@jkrems](https://github.com/jkrems)** [#12](https://github.com/buggerjs/node-inspect/pull/12)
  - [`3405225`](https://github.com/buggerjs/node-inspect/commit/3405225979dfc2058bcc6d1b90f41c060dbd1f92) **fix:** Use proper path for websocket - see: [#11](https://github.com/buggerjs/node-inspect/issues/11)


### 1.8.3

* [`6f9883d`](https://github.com/buggerjs/node-inspect/commit/6f9883d4b29419831133988981b83e891b19739a) **fix:** Breakpoints & scripts work when not paused
* [`ecb1362`](https://github.com/buggerjs/node-inspect/commit/ecb1362c842e6ed5bc28c091a32bfd540742db75) **chore:** Pin node to 6.8


### 1.8.2

* [`4219a98`](https://github.com/buggerjs/node-inspect/commit/4219a98d6514f1068feabce2945c21a0d5ba6561) **refactor:** Decouple source snippet from repl


### 1.8.1

* [`95402ee`](https://github.com/buggerjs/node-inspect/commit/95402ee5dff04057f074677d39db2f61ec74c151) **refactor:** Move `list` into CallFrame


### 1.8.0

* [`d0e6499`](https://github.com/buggerjs/node-inspect/commit/d0e6499084f5d656ef0c5fd470d3ab21f2e9a6b4) **feat:** `exec .scope`


### 1.7.0

* `breakOn{Exception,Uncaught,None}` - **[@jkrems](https://github.com/jkrems)** [#8](https://github.com/buggerjs/node-inspect/pull/8)
  - [`fa8c4c7`](https://github.com/buggerjs/node-inspect/commit/fa8c4c7d7bb6972733c92da4d04fdd62c02b0e3b) **feat:** `breakOn{Exception,Uncaught,None}` - see: [#6](https://github.com/buggerjs/node-inspect/issues/6)


### 1.6.0

* Add `help` command - **[@jkrems](https://github.com/jkrems)** [#7](https://github.com/buggerjs/node-inspect/pull/7)
  - [`09b37a0`](https://github.com/buggerjs/node-inspect/commit/09b37a02e04e16a38ce27f69538d3b098548b47c) **feat:** Add `help` command - see: [#5](https://github.com/buggerjs/node-inspect/issues/5)


### 1.5.0

* [`7e0fd99`](https://github.com/buggerjs/node-inspect/commit/7e0fd99fcfc65d8b647a2259df78f4cabf1d3d63) **feat:** Add `r` shortcut for `run`


### 1.4.1

* [`484d098`](https://github.com/buggerjs/node-inspect/commit/484d0983f06d6ff9639ab5197ba0a58313f532df) **chore:** Remove old implementation


### 1.4.0

* Properly tested implementation - **[@jkrems](https://github.com/jkrems)** [#4](https://github.com/buggerjs/node-inspect/pull/4)
  - [`ba060d3`](https://github.com/buggerjs/node-inspect/commit/ba060d3ef65ae84df2a3a9b9f16d563f3c4b29be) **feat:** Error handling w/o args
  - [`b39b3bc`](https://github.com/buggerjs/node-inspect/commit/b39b3bc07c13adc48fc8bb720889285c51e62548) **feat:** Launch child
  - [`481693f`](https://github.com/buggerjs/node-inspect/commit/481693f676ee099b7787cd2426b980858e973602) **feat:** Connect debug client
  - [`3bba0f2`](https://github.com/buggerjs/node-inspect/commit/3bba0f2416b2e3b4e6010de675003fcc328b16e8) **chore:** Disable lint for inactive code
  - [`cc7bdfc`](https://github.com/buggerjs/node-inspect/commit/cc7bdfcf7f21ef5cd5c32c7800407238b0d4f100) **feat:** Properly fail with invalid host:port
  - [`73f34f9`](https://github.com/buggerjs/node-inspect/commit/73f34f902634e9778597e129f46895aa8b643d72) **refactor:** Remove unused field
  - [`6a23e0c`](https://github.com/buggerjs/node-inspect/commit/6a23e0cf3179f43ca6fc5a0fa2b1dd18ebc044b5) **refactor:** Better debug output & support node 6.6
  - [`63b0f9b`](https://github.com/buggerjs/node-inspect/commit/63b0f9b6ef8bd9af0f7cb14a5938a45838731fc9) **test:** Add timeout to waitFor(pattern)
  - [`cfa197b`](https://github.com/buggerjs/node-inspect/commit/cfa197bf8325a1a4ca1b296f8d6971d368bfbfbb) **refactor:** Move REPL setup into own file
  - [`3f46c2c`](https://github.com/buggerjs/node-inspect/commit/3f46c2c43f836e1135b66871087aa74969f6b330) **feat:** Working repl eval
  - [`6911eb1`](https://github.com/buggerjs/node-inspect/commit/6911eb1a00b964bc5683506d433fa4f665f5a82c) **feat:** Enter repeats last command
  - [`7d20b7d`](https://github.com/buggerjs/node-inspect/commit/7d20b7deadf1b251ea8cf2cc9167c175624932c4) **chore:** Add missing license header
  - [`23c62f8`](https://github.com/buggerjs/node-inspect/commit/23c62f8375ca7c8b71d032047e728dace02f4efa) **feat:** Print break context
  - [`5dbc83d`](https://github.com/buggerjs/node-inspect/commit/5dbc83df31171f9c38a974c99340bde26f2e24ec) **feat:** Stepping and breakpoints
  - [`8deb8cc`](https://github.com/buggerjs/node-inspect/commit/8deb8cc36b9fca432ab8df63a82e9de7ab5adaf0) **feat:** list for printing source
  - [`1ed2ec9`](https://github.com/buggerjs/node-inspect/commit/1ed2ec9937070652be611dbb6b11dfb42cb840f8) **chore:** Disable verbose output on CI
  - [`625a435`](https://github.com/buggerjs/node-inspect/commit/625a435925dd8fd980bed2dc9e3fd73dd27df4ef) **fix:** Gracefully handle delayed scriptParsed
  - [`8823c60`](https://github.com/buggerjs/node-inspect/commit/8823c60d347600b2313cfdd8cb5e96fe02419a8a) **chore:** Run all the tests
  - [`00506f7`](https://github.com/buggerjs/node-inspect/commit/00506f763928cc440505a81030167a11b9a84e00) **feat:** backtrace/bt
  - [`e1ee02d`](https://github.com/buggerjs/node-inspect/commit/e1ee02d5cc389916489d387d07d5dd161230427a) **refactor:** Leverage util.inspect.custom
  - [`5dcc319`](https://github.com/buggerjs/node-inspect/commit/5dcc31922d40f56c7435319d1538390a442e8e4b) **feat:** scripts and scripts(true)
  - [`085cd5a`](https://github.com/buggerjs/node-inspect/commit/085cd5a76a961edfcaa342fff5eb09bf2f9c8983) **refactor:** Consistent import style
  - [`1c60f91`](https://github.com/buggerjs/node-inspect/commit/1c60f91f233848c05d865617dc7f5aacb36270b6) **feat:** Set breakpoint before file is loaded
  - [`bc82ecc`](https://github.com/buggerjs/node-inspect/commit/bc82eccb2a1a7c0f5332371254f6584e748216aa) **feat:** breakpoints to list breakpoints
  - [`7f48c95`](https://github.com/buggerjs/node-inspect/commit/7f48c9510696ec400d51afaca8d23a9c292640f8) **feat:** watchers & exec
  - [`0f8cd13`](https://github.com/buggerjs/node-inspect/commit/0f8cd13a092e5dbeb395ff04cbe2ed97cb986423) **feat:** clearBreakpoint
  - [`0d31560`](https://github.com/buggerjs/node-inspect/commit/0d315603bdcb9f4da42fab24dc569c325151269e) **feat:** version to print v8 version
  - [`df6b89d`](https://github.com/buggerjs/node-inspect/commit/df6b89df580a9afcb3b8883b0e4224cbcebb384f) **feat:** Paused & global exec
  - [`9e97d73`](https://github.com/buggerjs/node-inspect/commit/9e97d73073ceffd70974d45887c84fadb9159d5c) **feat:** repl to enter exec mode
  - [`9ee9f90`](https://github.com/buggerjs/node-inspect/commit/9ee9f903d6202f54ed2b3b3559da4006b65d39b5) **feat:** run & restart
* [`3a752aa`](https://github.com/buggerjs/node-inspect/commit/3a752aaa773968bfe16c5f543bd739feed598bea) **feat:** kill
* [`a67e470`](https://github.com/buggerjs/node-inspect/commit/a67e47018b20d46aeeaa7abd27eb8e7770fd0b8f) **feat:** Restore breakpoints on restart


### 1.3.3

* [`eb7a54c`](https://github.com/buggerjs/node-inspect/commit/eb7a54c6fa731ed3276072c72034046fc5ffbac6) **chore:** Switch to tap for tests


### 1.3.2

* Add notes about governance - **[@jkrems](https://github.com/jkrems)** [#3](https://github.com/buggerjs/node-inspect/pull/3)
  - [`e94089d`](https://github.com/buggerjs/node-inspect/commit/e94089d93689cacf5c953e94563463d1e174452d) **chore:** Add notes about governance


### 1.3.1

* [`8767137`](https://github.com/buggerjs/node-inspect/commit/8767137c53a2f6b1d36970074ea95be9871e50e3) **style:** Remove rogue console.log


### 1.3.0

* [`3ac6232`](https://github.com/buggerjs/node-inspect/commit/3ac623219ba44b0af40ef66826610a26a46c7966) **feat:** Add `version` command


### 1.2.0

* [`86b5812`](https://github.com/buggerjs/node-inspect/commit/86b581218ccab44e6bde259a17ad1e71645a6137) **feat:** scripts & listScripts(true)


### 1.1.1

* [`feaea38`](https://github.com/buggerjs/node-inspect/commit/feaea385a981e6b72a8d99277fbf575c54e15fc6) **style:** Typo in comment


### 1.1.0

* [`c64155f`](https://github.com/buggerjs/node-inspect/commit/c64155faa552f71463842a26330aa5bcbfc31670) **feat:** repl command


### 1.0.0

* [`44c4c79`](https://github.com/buggerjs/node-inspect/commit/44c4c79af5a228ccfd8906f11409b2a33390b878) **chore:** Initial commit
* [`985873c`](https://github.com/buggerjs/node-inspect/commit/985873cfb97146b38480080f9907219c473f1f6f) **feat:** Launching the example works
* [`3d92d05`](https://github.com/buggerjs/node-inspect/commit/3d92d05cca152a2c2647aa64eefc80432638bc4d) **chore:** Proper license and passing tests
* [`b3f99d9`](https://github.com/buggerjs/node-inspect/commit/b3f99d981038b17663fcfd984d2f5d6d9b51ee18) **feat:** Futile attempts to send a valid ws frame
* [`465cfb7`](https://github.com/buggerjs/node-inspect/commit/465cfb7b295aebb48b285c26f6de9c4657fe590d) **feat:** Working ws connection
* [`da9f011`](https://github.com/buggerjs/node-inspect/commit/da9f01118e2b144f2da8cd370113a608526774a1) **fix:** Fix remote connect
* [`5ef33d7`](https://github.com/buggerjs/node-inspect/commit/5ef33d7892cc49becb4c66098fc7927bc74b014a) **feat:** Working step-by-step
* [`534e1e4`](https://github.com/buggerjs/node-inspect/commit/534e1e46b307d61d51eb4c0aab4a3b17c17aea3d) **chore:** Add bin entry
* [`8cff9cf`](https://github.com/buggerjs/node-inspect/commit/8cff9cfb0138b5ecff0f5f6a7839dbfddc0684fd) **style:** Use simpler key thingy
* [`720ec53`](https://github.com/buggerjs/node-inspect/commit/720ec53a5b251ab3caf27f06b60924efb9e03a92) **doc:** Add instructions
* [`b89ad60`](https://github.com/buggerjs/node-inspect/commit/b89ad601b885a417e6433b1609477d8453f498a1) **doc:** More helpful docs
* [`de9243c`](https://github.com/buggerjs/node-inspect/commit/de9243c95eabe733d05952229340808c3cebf129) **feat:** Watchers
* [`e16978f`](https://github.com/buggerjs/node-inspect/commit/e16978ff8e4b2b2bdccf88fd7d3905f525822981) **docs:** Working usage hints
* [`2dbc204`](https://github.com/buggerjs/node-inspect/commit/2dbc2042145fd97169fc7536186a449715e27810) **refactor:** Use proxies
* [`b8c9b14`](https://github.com/buggerjs/node-inspect/commit/b8c9b147713f63181396d5a7fe4c2f737b733b4c) **style:** Remove unused var
* [`f6b4b20`](https://github.com/buggerjs/node-inspect/commit/f6b4b20a1d28d91cfe452b995f7dbe5f7c749e89) **feat:** Nicer inspect of remote values
* [`36887c6`](https://github.com/buggerjs/node-inspect/commit/36887c66bbf26d540f087f80ddfec38462a33bdf) **fix:** Properly print watchers
* [`7729442`](https://github.com/buggerjs/node-inspect/commit/77294426157a28cc76e339cb13916a205182641e) **feat:** Add pause command
* [`e39a713`](https://github.com/buggerjs/node-inspect/commit/e39a7134873f06da37baaa9b6252cede4ad38d7a) **fix:** Properly format boolean properties
* [`f8f51d7`](https://github.com/buggerjs/node-inspect/commit/f8f51d7a01e8d74023306a08a3d6e2da63d123e1) **fix:** Properly format numeric properties
* [`89e6e08`](https://github.com/buggerjs/node-inspect/commit/89e6e087220f3c3cb628ac7541c44298485a2e04) **feat:** Add backtrace command
* [`82362ac`](https://github.com/buggerjs/node-inspect/commit/82362acfc7ce22b4cccc64889ec136dedc8895ec) **feat:** Add setBreakpoint()
* [`7064cce`](https://github.com/buggerjs/node-inspect/commit/7064ccec3b103683088d532abfe5b4e7c066948b) **feat:** Add `setBreakpoint(line)`
* [`360580e`](https://github.com/buggerjs/node-inspect/commit/360580eba4353e81311e56df018eec0ca233da11) **feat:** Add run/kill/restart
* [`b1b576e`](https://github.com/buggerjs/node-inspect/commit/b1b576e2645723a8575df544e0bfb672d60d9d91) **feat:** Add `help` command
* [`2db4660`](https://github.com/buggerjs/node-inspect/commit/2db46609cd1c8543d31ebd5dc47e4c27ec254841) **feat:** Add remaining sb() variants
* [`f2ad1ae`](https://github.com/buggerjs/node-inspect/commit/f2ad1aeedafb154043d70bb9195b10986d311d26) **fix:** Display breakpoints set into the future
* [`73272f9`](https://github.com/buggerjs/node-inspect/commit/73272f9ace1f8546f8cad1d53627dbffba50bb4e) **refactor:** Make breakpoints more inspect friendly
* [`507a71d`](https://github.com/buggerjs/node-inspect/commit/507a71de345a3de7fe144517e9f5ea264ff993e3) **feat:** Add breakpoints command
* [`5fb3e5d`](https://github.com/buggerjs/node-inspect/commit/5fb3e5d17bbcfd45b264431547b3cf0b781c7640) **docs:** Link to Command Line API docs
* [`81af501`](https://github.com/buggerjs/node-inspect/commit/81af501bbf85397e2078310c7f24a9ac5b7f02dc) **chore:** Fix license field
