.( example 16. random numbers and number guessing game)

variable t16
variable seed                            ( seed )
here seed !                              ( initialize seed )

: random ( -- n, a random number within 0 to 65536 )
        seed @ 31421 *                  ( seed*31421 )
        6927 + 65535 and                ( rnd*31421+6926, mod 65536)
        dup seed !                      ( refresh he seed )
        ;

: choose ( n1 -- n2, a random number within 0 to n1 )
        random *                        ( n1*random to a double product)
        65536 /                         ( discard lower part )
        ;                               ( in fact divide by 65536 )

cr .( to test the routine, type )
cr .( 100 choose . => ) 100 choose .
cr .( 100 choose . => ) 100 choose .
cr .( 100 choose . => ) 100 choose . cr

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
        > if    cr ." too high! "
        else    cr ." too low. "
        then    cr ." guess again? "
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
    cr ;

: n-guess
  3 for
    32000 choose dup . ." guess " cr
    guess
  next ;

cr .( type 'greet' for a number guessing game )
greet
cr .( 3 limit ) 3 limit
n-guess
