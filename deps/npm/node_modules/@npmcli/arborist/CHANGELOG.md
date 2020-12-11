# CHANGELOG

## 2.0

* BREAKING CHANGE: root node is now included in inventory
* All parent/target/fsParent/etc. references set in `root` setter, rather
  than the hodgepodge of setters that existed before.
* `treeCheck` function added, to enforce strict correctness guarantees when
  `ARBORIST_DEBUG=1` in the environment (on by default in Arborist tests).

## 1.0

* Release for npm v7 beta
* Fully functional

## 0.0

* Proof of concept
* Before this, it was [`read-package-tree`](http://npm.im/read-package-tree)
