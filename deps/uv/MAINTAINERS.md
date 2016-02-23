
# Project Maintainers

libuv is currently managed by the following individuals:

* **Ben Noordhuis** ([@bnoordhuis](https://github.com/bnoordhuis))
  - GPG key: D77B 1E34 243F BAF0 5F8E  9CC3 4F55 C8C8 46AB 89B9 (pubkey-bnoordhuis)
* **Bert Belder** ([@piscisaureus](https://github.com/piscisaureus))
* **Fedor Indutny** ([@indutny](https://github.com/indutny))
  - GPG key: AF2E EA41 EC34 47BF DD86  FED9 D706 3CCE 19B7 E890 (pubkey-indutny)
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

