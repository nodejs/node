## test\_posix\_permissions.zip
Rebuild this zip by running:
```
rm test_posix_permissions.zip &&
mkdir z &&
cd z &&
touch 0.txt 1.txt 2.txt 3.txt &&
chmod a+x 0.txt &&
chmod o+x 1.txt &&
chmod u+x 2.txt &&
zip test_posix_permissions.zip * &&
mv test_posix_permissions.zip .. &&
cd .. &&
rm -r z
```
