.( example 14. radix for number conversions )
variable t14

decimal
: octal  8 base !  ;
: binary 2 base !  ;

cr .( try converting numbers among different radices: )
cr .( decimal 12345 hex .           => ) decimal 12345 hex  .
cr .( hex abcd decimal .            => ) hex abcd decimal  .
cr .( decimal 100 binary .          => ) decimal 100 binary  .
cr .( binary 101010101010 decimal . => ) binary 101010101010 decimal . cr

