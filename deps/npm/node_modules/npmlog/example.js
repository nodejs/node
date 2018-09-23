var log = require('./log.js')

log.heading = 'npm'

console.error('log.level=silly')
log.level = 'silly'
log.silly('silly prefix', 'x = %j', {foo:{bar:'baz'}})
log.verbose('verbose prefix', 'x = %j', {foo:{bar:'baz'}})
log.info('info prefix', 'x = %j', {foo:{bar:'baz'}})
log.http('http prefix', 'x = %j', {foo:{bar:'baz'}})
log.warn('warn prefix', 'x = %j', {foo:{bar:'baz'}})
log.error('error prefix', 'x = %j', {foo:{bar:'baz'}})
log.silent('silent prefix', 'x = %j', {foo:{bar:'baz'}})

console.error('log.level=silent')
log.level = 'silent'
log.silly('silly prefix', 'x = %j', {foo:{bar:'baz'}})
log.verbose('verbose prefix', 'x = %j', {foo:{bar:'baz'}})
log.info('info prefix', 'x = %j', {foo:{bar:'baz'}})
log.http('http prefix', 'x = %j', {foo:{bar:'baz'}})
log.warn('warn prefix', 'x = %j', {foo:{bar:'baz'}})
log.error('error prefix', 'x = %j', {foo:{bar:'baz'}})
log.silent('silent prefix', 'x = %j', {foo:{bar:'baz'}})

console.error('log.level=info')
log.level = 'info'
log.silly('silly prefix', 'x = %j', {foo:{bar:'baz'}})
log.verbose('verbose prefix', 'x = %j', {foo:{bar:'baz'}})
log.info('info prefix', 'x = %j', {foo:{bar:'baz'}})
log.http('http prefix', 'x = %j', {foo:{bar:'baz'}})
log.warn('warn prefix', 'x = %j', {foo:{bar:'baz'}})
log.error('error prefix', 'x = %j', {foo:{bar:'baz'}})
log.silent('silent prefix', 'x = %j', {foo:{bar:'baz'}})
log.error('404', 'This is a longer\n'+
                 'message, with some details\n'+
                 'and maybe a stack.\n'+
                 new Error('a 404 error').stack)
log.addLevel('noise', 10000, {beep: true})
log.noise(false, 'LOUD NOISES')
