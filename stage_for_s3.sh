#!/bin/bash

mkdir stage
cd stage

TIMESTAMP=$(date '+%Y%m%d.%H%M')

echo "Current timestamp is $TIMESTAMP"

gh release download -p "*.gz"
gh release download -p "*.xz"

curl "https://asana-oss-cache.s3.us-east-1.amazonaws.com/node-fibers/fibers-5.0.4.pc.tgz"  --output fibers-5.0.4.tar.gz
tar -xzf fibers-5.0.4.tar.gz

ls *.gz | while read a
do
	tar -xzf "$a" -C package/bin
	rm $a
done

tar -czf temp.tgz package/
rm -fr package
SHORT_HASH=$(cat temp.tgz | sha1sum | cut -c1-4)
echo HASH: $SHORT_HASH
UNIQUE="pc-${TIMESTAMP}-${SHORT_HASH}"

mv temp.tgz fibers-5.0.4-${UNIQUE}.tgz

#for file in *.tar.xz; do
  #if [[ "$file" == *-LATEST.tar.xz ]]; then
    ## Extract base name without the -LATEST part
    #base="${file%-LATEST.tar.xz}"
#
    ## New filename
    #new_name="${base}-${UNIQUE}.tar.xz"
#
    #echo "Renaming: $file -> $new_name"
    #mv "$file" "$new_name"
  #fi
#done

for file in *.tar.xz; do
  if [[ "$file" == *-LATEST.tar.xz ]]; then
    base="${file%-LATEST.tar.xz}"
    new_name="${base}-${UNIQUE}.tar.xz"

    echo "Renaming: $file -> $new_name"
    mv "$file" "$new_name"

    if [[ "$new_name" =~ node-v([0-9.]+)-(darwin|linux)-(arm64|x64)-pc.*\.tar\.xz$ ]]; then
      version="${BASH_REMATCH[1]}"
      os="${BASH_REMATCH[2]}"
      arch="${BASH_REMATCH[3]}"
      target_dir="node-v${version}-${os}-${arch}"

      echo "Target Dir: $target_dir"
      mkdir $target_dir
      tar -xzf "$new_name" -C "$target_dir"
      mv $target_dir/usr/local/* $target_dir
      rm -fr $target_dir/usr/local

      tar -cJf "$new_name" $target_dir

      rm -fr $target_dir

      echo "✅ Done: Archive now contains:"
      tar -tf "$new_name" | head

      # Make a clean working dir
      #temp_dir="$(mktemp -d)"
      #extract_dir="${temp_dir}/extract"
      #mkdir -p "$extract_dir"

      #echo "Extracting $new_name..."
      #tar -xf "$new_name" -C "$extract_dir"

      ## Move usr/local to node-v*/...
      #if [ -d "$extract_dir/usr/local" ]; then
        #echo "Rewriting archive paths under $target_dir/"
        #mkdir -p "$extract_dir/$target_dir"
        #mv "$extract_dir/usr/local/"* "$extract_dir/$target_dir/"
        #rm -rf "$extract_dir/usr"  # Clean up
      #else
        #echo "Error: expected usr/local inside archive, but not found."
        #ls "$extract_dir"
        #exit 1
      #fi

      #echo "Repacking $new_name with new paths..."
      #(
        #cd "$extract_dir"
        #tar -cJf "$new_name" "$target_dir"
      #)
#
      #echo "✅ Done: Archive now contains:"
      #tar -tf "$new_name" | head

      #rm -rf "$temp_dir"
    else
      echo "Warning: Skipped $new_name due to unexpected filename format."
    fi
  fi
done


cd ..
mv stage node-${UNIQUE}

echo "Files are in node-${UNIQUE}, please upload to s3"



