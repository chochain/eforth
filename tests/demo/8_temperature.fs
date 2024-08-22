.( example 8. temperature conversion )

variable t8
: f>c ( nfarenheit -- ncelcius )
        32 -
        10 18 */
        ;

: c>f ( ncelcius -- nfarenheit )
        18 10 */
        32 +
        ;

cr .( 90 f>c . => ) 90 f>c .
cr .( 0  c>f . => ) 0  c>f . cr

