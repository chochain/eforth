0 constant pp                      \ producer task handle
0 constant cc                      \ consumer task handle
: xx                               \ producer method
  1 2 3 4 4 cc send                  \ send 4 items to consumer
  lock                               \ lock IO for printing
    ." ...xx done "
  unlock ;                           \ free IO
: yy                               \ consumer method
  recv                               \ receiver waiting for data
  + + +                              \ got the data, sum them up
  lock                               \ lock IO for printing
    ." ...yy done "
  unlock ;                           \ free IO
' xx task to pp                    \ create producer task, save handle
' yy task to cc                    \ create consumer task, save handle
cc start                           \ start receiver task, it will wait
.( VM0: non-blocking, and we have two threads now... ) cr
1000 ms
pp start                           \ start producer task
.( VM0: non-blocking on pp, needs a join, or free to do other things ) cr
pp join
cc join
1 cc pull                          \ pull from completed task
." total=" . cr
.( VM0: finally, all done! )
bye
