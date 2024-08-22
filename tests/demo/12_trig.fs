.( example 12. sines and cosines )

variable t12
31416 constant pi
10000 constant 10k
variable xs                      ( square of scaled angle )

: kn ( n1 n2 -- n3, n3=10000-n1*x*x/n2 where x is the angle )
        xs @ swap /              ( x*x/n2 )
        negate 10k */            ( -n1*x*x/n2 )
        10k +                    ( 10000-n1*x*x/n2 )
        ;
: (sin) ( x -- sine*10k, x in radian*10k )
        dup dup 10k */           ( x*x scaled by 10k )
        xs !                     ( save it in xs )
        10k 72 kn                ( last term )
        42 kn 20 kn 6 kn         ( terms 3, 2, and 1 )
        10k */                   ( times x )
        ;
: (cos) ( x -- cosine*10k, x in radian*10k )
        dup 10k */ xs !          ( compute and save x*x )
        10k 56 kn 30 kn 12 kn 2 kn ( serial expansion )
        ;
: sin_ ( degree -- sine*10k )
        pi 180 */                ( convert to radian )
        (sin)                    ( compute sine )
        ;
: cos_ ( degree -- cosine*10k )
        pi 180 */
        (cos)
        ;
: sin ( degree -- sin )
    360 mod dup 0< if 360 + then ( mod may be negative )
    dup  46 < if sin_ else
    dup 136 < if 90 - cos_ else 
    dup 226 < if 180 - sin_ negate else 
    dup 316 < if 270 - cos_ negate else
    360 - sin_ then then then then ;
: cos     90 + sin ;

cr .( to test the routines, type: )
cr .( 90 sin . => ) 90 sin .
cr .( 45 sin . => ) 45 sin .
cr .( 30 sin . => ) 30 sin .
cr .( 0  sin . => ) 0 sin .
cr .( 90 cos . => ) 90 cos .
cr .( 45 cos . => ) 45 cos .
cr .( 0  cos . => ) 0 cos . cr

