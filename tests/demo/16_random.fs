.( example 16.      random numbers )

variable t16_17
variable rnd                            ( seed )
here rnd !                              ( initialize seed )

: random ( -- n, a random number within 0 to 65536 )
        rnd @ 31421 *                   ( rnd*31421 )
        6927 + 65535 and                ( rnd*31421+6926, mod 65536)
        dup rnd !                       ( refresh he seed )
        ;

: choose ( n1 -- n2, a random number within 0 to n1 )
        random *                        ( n1*random to a double product)
        65536 /                         ( discard lower part )
        ;                               ( in fact divide by 65536 )

( to test the routine, type )
\        100 choose . cr
\        100 choose . cr
\        100 choose . cr

.( example 17.      guess a number )
( example 16 must be loaded.)

variable myNumber
variable yourNumber

: limit ( n -- )
    yourNumber !
    cr ." Now, type you guess as:"
    cr ." xxxx guess"
    cr ." where xxxx is your guess."
    yourNumber @ choose myNumber !
        ;

: guess ( n1 -- , allow player to guess, exit when the guess is correct )
    myNumber @ 2dup =                  ( equal? )
        if      2drop           ( discard both numbers )
                cr ." correct!!!"
                exit
        then
        > if    cr ." too high!"
        else    cr ." too low."
        then    cr ." guess again?"
        ;

: greet ( -- )
        cr cr cr ." guess a number"
        cr ." this is a number guessing game.  i'll think"
        cr ." of a number between 0 and any limit you want."
        cr ." (it should be smaller than 32000.)"
        cr ." then you have to guess what it is."
    cr
    cr ." Set up the limit by typing:"
    cr ." xxxx limit "
    cr ." where xxxx is a number smaller than 32000."
        ;

( greet )
