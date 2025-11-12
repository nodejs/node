set -ex

if [ ! -d "webidl2.js" ]; then
  git clone https://github.com/w3c/webidl2.js.git
fi
cd webidl2.js
npm install
npm run build-debug
HASH=$(git rev-parse HEAD)
cd ..
cp webidl2.js/dist/webidl2.js lib/
echo "Currently using webidl2.js@${HASH}." > lib/VERSION.md
