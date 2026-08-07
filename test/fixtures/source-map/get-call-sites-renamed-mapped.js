const{getCallSites}=require('node:util');function foo(){process.stdout.write(JSON.stringify(getCallSites({sourceMap:true})[0]));}foo();
//# sourceMappingURL=get-call-sites-renamed-mapped.map
