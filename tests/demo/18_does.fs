.( lesson 18 - create..does> )

cr .( : const create , ." created " does> @ ." fetched " ;)
: const
  create
    , ." created "
  does>
    @ ." fetched " ;

cr .( 123 const x ) 123 const x
cr .( see x => ) cr see x
cr .( x => ) cr x .

cr .( create...does> can also be used for data structure )
cr .( : vec3 create , , , does> dup >r @ r@ 1 th @ r> 2 th @ ;)
: vec3
  create
    rot , swap , ,
  does>
    dup >r @
    r@ 1 th @
    r> 2 th @ ;
: 3. ( x y z -- ) rot . swap . . ;
cr .( 111 222 333 vec3 xyz ) 111 222 333 vec3 xyz
cr .( see xyz => ) cr see xyz
cr .( xyz => ) xyz 3. cr
