#!/bin/bash

cd ~/node/test/parallel

mkdir test-stream-writable-dir

for file in ./*.js
do
    if [[ "$file" =~ .*"test-stream-writable".* ]]; then
        cp $file "./test-stream-writable-dir/$file"
        echo "{$file} copied"
    fi
done