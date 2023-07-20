// since it's ESM, save it as .mjs

import fs from 'node:fs'
import process from 'node:process'
import { Readable } from 'node:stream'

// we initialize a stream, but not start consuming it
const randomNodeStream = fs.createReadStream('/dev/urandom')
// after 10 seconds, it'll get converted to web stream
let randomWebStream

// we check memory usage every second
// since it's a stream, it shouldn't be higher than the chunk size
const reportMemoryUsage = () => {
    const { arrayBuffers } = process.memoryUsage()
    console.log(
        `Array buffers memory usage is ${Math.round(
            arrayBuffers / 1024 / 1024
        )} MiB`
    )
    if (arrayBuffers > 256 * 1024 * 1024) {
        // streaming should not lead to such a memory increase
        // therefore, if it happens => bail
        console.log('Over 256 MiB taken, exiting')
        process.exit(0)
    }
}
setInterval(reportMemoryUsage, 1000)

// after 10 seconds we use Readable.toWeb
// memory usage should stay pretty much the same since it's still a stream
setTimeout(() => {
    console.log('converting node stream to web stream')
    randomWebStream = Readable.toWeb(randomNodeStream)
}, 5000)

// after 15 seconds we start consuming the stream
// memory usage will grow, but the old chunks should be garbage-collected pretty quickly
setTimeout(async () => {
    console.log('reading the chunks')
    for await (const chunk of randomWebStream) {
        // do nothing, just let the stream flow
    }
}, 15000)