cr .( example 13. square root )
variable t13

decimal
: sqrt ( n -- root )
        1600000000 over <               ( largest square it can handle)
        if drop 40000 exit then         ( safety exit )
        >r                              ( save sqaure )
        1 1                             ( initial square and root )
        begin                           ( set n1 as the limit )
                over r@ <              ( next square )
        while
                dup 2 * 1 +              ( n*n+2n+1 )
                rot + swap
                1 +                     ( n+1 )
        repeat
        swap drop
        r> drop
        ;

cr .( to test the routines, type: )
cr .( 16    sqrt . => ) 16    sqrt .
cr .( 625   sqrt . => ) 625   sqrt .
cr .( 10000 sqrt . => ) 10000 sqrt . cr
