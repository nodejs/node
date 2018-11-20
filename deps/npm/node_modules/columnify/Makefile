
all: columnify.js

prepublish: all

columnify.js: index.js package.json
	babel index.js > columnify.js

.PHONY: all prepublish
