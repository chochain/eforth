: xx 9999 for 34 drop next ;
: yy 9999 for xx next ;
: zz clock negate yy clock +
  lock
    ." VM" rank .
    .
    ." ms " cr
  unlock ;
' zz constant xt
: ser ( n -- ) for zz next ;     \ tasks in serial 
: par ( n -- )                   \ tasks in parallel
  dup >r                           \ keep loop count
  for
    xt task dup start              \ start task, and keep task id as TOS
  next
  r> for join next ;               \ pop loop count and join tasks
: bench ( fn n -- )              \ benchmark
  clock negate >r
  1- swap exec
  r> clock + 
  ." elapsed ms=" . cr ;
.( 4 tasks run in serial ) cr
' ser 4 bench
.( 4 tasks run in parallel ) cr
' par 4 bench
bye

