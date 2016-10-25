
# Project Maintainers

libuv is currently managed by the following individuals:

* **Ben Noordhuis** ([@bnoordhuis](https://github.com/bnoordhuis))
  - GPG key: D77B 1E34 243F BAF0 5F8E  9CC3 4F55 C8C8 46AB 89B9 (pubkey-bnoordhuis)
* **Bert Belder** ([@piscisaureus](https://github.com/piscisaureus))
* **Colin Ihrig** ([@cjihrig](https://github.com/cjihrig))
  - GPG key: 94AE 3667 5C46 4D64 BAFA  68DD 7434 390B DBE9 B9C5 (pubkey-cjihrig)
* **Fedor Indutny** ([@indutny](https://github.com/indutny))
  - GPG key: AF2E EA41 EC34 47BF DD86  FED9 D706 3CCE 19B7 E890 (pubkey-indutny)
* **Imran Iqbal** ([@iWuzHere](https://github.com/iWuzHere))
  - GPG key: 9DFE AA5F 481B BF77 2D90  03CE D592 4925 2F8E C41A (pubkey-iwuzhere)
* **Santiago Gimeno** ([@santigimeno](https://github.com/santigimeno))
  - GPG key: 612F 0EAD 9401 6223 79DF  4402 F28C 3C8D A33C 03BE (pubkey-santigimeno)
* **Saúl Ibarra Corretgé** ([@saghul](https://github.com/saghul))
  - GPG key: FDF5 1936 4458 319F A823  3DC9 410E 5553 AE9B C059 (pubkey-saghul)

## Storing a maintainer key in Git

It's quite handy to store a maintainer's signature as a git blob, and have
that object tagged and signed with such key.

Export your public key:

    $ gpg --armor --export saghul@gmail.com > saghul.asc

Store it as a blob on the repo:

    $ git hash-object -w saghul.asc

The previous command returns a hash, copy it. For the sake of this explanation,
we'll assume it's 'abcd1234'. Storing the blob in git is not enough, it could
be garbage collected since nothing references it, so we'll create a tag for it:

    $ git tag -s pubkey-saghul abcd1234

Commit the changes and push:

    $ git push origin pubkey-saghul
