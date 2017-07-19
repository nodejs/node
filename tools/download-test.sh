#!/bin/bash

if [ -z ${NODE_VERSION} ]; then
  echo "NODE_VERSION is undefined! Please declare NODE_VERSION"
  exit 1
fi
# Remove old files
rm -r nodejs.org || true

# Scrape the download server
wget -r --no-parent http://nodejs.org/dist/v${NODE_VERSION}/

cat nodejs.org/dist/v${NODE_VERSION}/SHASUMS256.txt | while read line
do
   sha=$(echo "${line}" | awk '{print $1}')
   file=$(echo "${line}" | awk '{print $2}')
   echo "Checking shasum for $file"
   remoteSHA=$(shasum -a 256 "nodejs.org/dist/v${NODE_VERSION}/${file}" | awk '{print $1}')
   if [ ${remoteSHA} != ${sha} ]; then
     echo "Error - SHASUMS256 does not match for ${file}"
     echo "Expected - ${sha}"
     echo "Found - ${remoteSHA}"
     return 1
   else
     echo "Pass - SHASUMS256 for ${file} is correct"
   fi
done
