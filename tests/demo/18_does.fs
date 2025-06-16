.( lesson 18 - create..does> )

.( : const create , ." created " does> @ ." fetched " ;) cr
: const
  create
    , ." created "
  does>
    @ ." fetched " ;

.( 123 const x ) cr 123 const x
.( see x => ) cr see x
.( x => ) x . cr cr

.( create...does> can also be used for data structure ) cr
.( : vec3 create , , , does> dup >r @ r@ 1 th @ r> 2 th @ ;) cr
: vec3
  create
    rot , swap , ,
  does>
    dup >r @
    r@ 1 th @
    r> 2 th @ ;
: 3. ( x y z -- ) rot . swap . . ;
.( 111 222 333 vec3 xyz ) cr 111 222 333 vec3 xyz
.( see xyz => ) cr see xyz
.( xyz => ) xyz 3. cr cr
