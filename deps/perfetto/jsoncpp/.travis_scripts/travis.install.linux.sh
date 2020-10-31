set -vex

wget https://github.com/ninja-build/ninja/releases/download/v1.9.0/ninja-linux.zip
unzip -q ninja-linux.zip -d build

pip3 install meson
echo ${PATH}
ls /usr/local
ls /usr/local/bin
export PATH="${PWD}"/build:/usr/local/bin:/usr/bin:${PATH}
