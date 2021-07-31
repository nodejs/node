Install snyk tool: zync_tool-install.py_Q#

Run the following command from a local terminal:

npm
Linux/Mac
Windows
npm install -g snyk
Node isn’t available on your environment? Download a binary for your platform

2
Authenticate your machine

snyk auth
Want to run this on a remote machine? Configure for CI

3
Analyze and test your dependencies

Navigate into your code’s directory and run:

snyk monitor
After scanning your project you'll be given a URL where you can see the results.
