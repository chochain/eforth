.( example 5. text display - the theory that jack built )

variable t5
decimal
: the           ." the " ;
: that          cr ." that " ;
: this          cr ." this is " the ;
: jack          ." jack builds" ;
: summary       ." summary" ;
: flaw          ." flaw" ;
: mummery       ." mummery" ;
: k             ." constant k" ;
: haze          ." krudite verbal haze" ;
: phrase        ." turn of a plausible phrase" ;
: bluff         ." chaotic confusion and bluff" ;
: stuff         ." cybernatics and stuff" ;
: theory        ." theory " jack ;
: button        ." button to start the machine" ;
: child         ." space child with brow serene" ;
: cybernatics   ." cybernatics and stuff" ;

: hiding        cr ." hiding " the flaw ;
: lay           that ." lay in " the theory ;
: based         cr ." based on " the mummery ;
: saved         that ." saved " the summary ;
: cloak         cr ." cloaking " k ;
: thick         if that else cr ." and " then
                ." thickened " the haze ;
: hung          that ." hung on " the phrase ;
: cover         if that ." covered "
                else cr ." to cover "
                then bluff ;
: make          cr ." to make with " the cybernatics ;
: pushed        cr ." who pushed " button ;
: without       cr ." without confusion, exposing the bluff" ;
: rest                                  ( pause for user interaction )
        ." . "                          ( print a period )
        10 spaces                       ( followed by 10 spaces )
        cr cr ;

: cloaked cloak saved based hiding lay rest ;
: THEORY
        cr this theory rest
        this flaw lay rest
        this mummery hiding lay rest
        this summary based hiding lay rest
        this k saved based hiding lay rest
        this haze cloaked
        this bluff hung 1 thick cloaked
        this stuff 1 cover hung 0 thick cloaked
        this button make 0 cover hung 0 thick cloaked
        this child pushed
                cr ." that made with " cybernatics without hung
                cr ." and, shredding " the haze cloak
                cr ." wrecked " the summary based hiding
                cr ." and demolished " the theory rest
        ;

cr .( type 'THERY' to execute =>)
THEORY
