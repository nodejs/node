#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

BASE_URL = "https://github.com/nodejs/gyp-next/archive/"
CHECKOUT_PATH = os.path.dirname(os.path.realpath(__file__))
CHECKOUT_GYP_PATH = os.path.join(CHECKOUT_PATH, "gyp")

parser = argparse.ArgumentParser()
parser.add_argument("tag", help="gyp tag to update to")
args = parser.parse_args()

tar_url = BASE_URL + args.tag + ".tar.gz"

changed_files = subprocess.check_output(["git", "diff", "--name-only"]).strip()
if changed_files:
    raise Exception("Can't update gyp while you have uncommitted changes in node-gyp")

with tempfile.TemporaryDirectory() as tmp_dir:
    tar_file = os.path.join(tmp_dir, "gyp.tar.gz")
    unzip_target = os.path.join(tmp_dir, "gyp")
    with open(tar_file, "wb") as f:
        print("Downloading gyp-next@" + args.tag + " into temporary directory...")
        print("From: " + tar_url)
        with urllib.request.urlopen(tar_url) as in_file:
            f.write(in_file.read())

        print("Unzipping...")
        with tarfile.open(tar_file, "r:gz") as tar_ref:
            def is_within_directory(directory, target):

                abs_directory = os.path.abspath(directory)
                abs_target = os.path.abspath(target)

                prefix = os.path.commonprefix([abs_directory, abs_target])

                return prefix == abs_directory

            def safe_extract(tar, path=".", members=None, *, numeric_owner=False):

                for member in tar.getmembers():
                    member_path = os.path.join(path, member.name)
                    if not is_within_directory(path, member_path):
                        raise Exception("Attempted Path Traversal in Tar File")

                tar.extractall(path, members, numeric_owner=numeric_owner)

            safe_extract(tar_ref, unzip_target)

        print("Moving to current checkout (" + CHECKOUT_PATH + ")...")
        if os.path.exists(CHECKOUT_GYP_PATH):
            shutil.rmtree(CHECKOUT_GYP_PATH)
        shutil.move(
            os.path.join(unzip_target, os.listdir(unzip_target)[0]), CHECKOUT_GYP_PATH
        )

subprocess.check_output(["git", "add", "gyp"], cwd=CHECKOUT_PATH)
subprocess.check_output(["git", "commit", "-m", "feat(gyp): update gyp to " + args.tag])
