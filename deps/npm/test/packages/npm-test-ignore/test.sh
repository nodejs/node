x=`find . | grep ignore | grep -v npmignore`
if [ "$x" != "" ]; then
  echo "ignored files included: $x"
  exit 1
fi

x=`find . | grep -v ignore | sort`
y=".
./include4
./package.json
./README
./sub
./sub/include
./sub/include2
./sub/include4
./test.sh"
y="`echo "$y" | sort`"
if [ "$x" != "$y" ]; then
  echo "missing included files"
  echo "got:"
  echo "==="
  echo "$x"
  echo "==="
  echo "wanted:"
  echo "==="
  echo "$y"
  echo "==="
  exit 1
fi
