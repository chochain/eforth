.( example 4. repeated patterns )

variable t4
variable width                  ( number of asterisks to print )

: asterisks ( -- , print n asterisks on the screen, n=width )
        width @                 ( limit=width, initial index=0 )
        for ." *"               ( print one asterisk at a time )
        next                    ( repeat n times )
        ;

: rectangle ( height width -- , print a rectangle of asterisks )
        width !                 ( initialize width to be printed )
        for     cr
                asterisks       ( print a line of asterisks )
        next
        ;

: parallelogram ( height width -- )
        width !
        for     cr r@ spaces    ( shift the lines to the right )
                asterisks       ( print one line )
        next
        ;

: triangle ( width -- , print a triangle area with asterisks )
        for     cr
                r@ width !      ( increase width every line )
                asterisks       ( print one line )
        next
        ;

.( try the following instructions: )
cr .( 3 10 rectangle)
3 10 rectangle

cr .( 5 18 parallelogram)
5 18 parallelogram

cr .( 12 triangle)
12 triangle  
