assert = require "assert"

delay = (ms, fn) -> setTimeout fn, ms
now = undefined
describe "now", ->
  it "initially gives a near zero (< 20 ms) time ", ->
    now = require "../"
    assert now() < 20
    
  it "gives a positive time", ->
    assert now() > 0

  it "two subsequent calls return an increasing number", ->
    a = now()
    b = now()      
    assert now() < now()
    
  it "has less than 10 microseconds overhead", ->
    Math.abs(now() - now()) < 0.010
  
  it "can do 1,000,000 calls really quickly", ->
    now() for i in [0...1000000]
    
  it "shows that at least 990 ms has passed after a timeout of 1 second", (done) ->
    a = now()
    delay 1000, ->
      b = now()
      diff = b - a
      return done new Error "Diff (#{diff}) lower than 990." if diff < 990
      return done null

  it "shows that not more than 1020 ms has passed after a timeout of 1 second", (done) ->
    a = now()
    delay 1000, ->
      b = now()
      diff = b - a
      return done new Error "Diff (#{diff}) higher than 1020." if diff > 1020
      return done null        