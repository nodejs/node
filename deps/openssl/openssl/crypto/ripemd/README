RIPEMD-160
http://www.esat.kuleuven.ac.be/~bosselae/ripemd160.html

This is my implementation of RIPEMD-160.  The pentium assember is a little
off the pace since I only get 1050 cycles, while the best is 1013.
I have a few ideas for how to get another 20 or so cycles, but at
this point I will not bother right now.  I believe the trick will be
to remove my 'copy X array onto stack' until inside the RIP1() finctions the
first time round.  To do this I need another register and will only have one
temporary one.  A bit tricky....  I can also cleanup the saving of the 5 words
after the first half of the calculation.  I should read the origional
value, add then write.  Currently I just save the new and read the origioal.
I then read both at the end.  Bad.

eric (20-Jan-1998)
