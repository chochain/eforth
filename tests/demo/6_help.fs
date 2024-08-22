.( example 6. help system)

variable t6
decimal
: question
        cr cr ." any more problems you want to solve?"
        cr ." what kind ( sex, job, money, health ) ?"
        cr
        ;

: help  cr
        cr ." hello!  my name is creating computer."
        cr ." hi there!"
        cr ." are you enjoying yourself here?"
        key 32 or 121 =
        cr
        if      cr ." i am glad to hear that."
        else    cr ." i am sorry about that."
                cr ." maybe we can brighten your visit a bit."
        then
        cr ." say!"
        cr ." i can solved all kinds of problems except those dealing"
        cr ." with greece. "
        question
        ;

: sex   cr cr ." is your problem too much or too little?"
        cr
        ;

: too  ;                                ( noop for syntax smoothness )

: much  cr cr ." you call that a problem?!!  i should have that problem."
        cr ." if it reall y bothers you, take a cold shower."
        question
        ;

: little
        cr cr ." why are you here!"
        cr ." you should be in tokyo or new york of amsterdam or"
        cr ." some place with some action."
        question
        ;

: health
        cr cr ." my advise to you is:"
        cr ."      1. take two tablets of aspirin."
        cr ."      2. drink plenty of fluids."
        cr ."      3. go to bed (along) ."
        question
        ;

: job   cr cr ." i can sympathize with you."
        cr ." i have to work very long every day with no pay."
        cr ." my advise to you, is to open a rental computer store."
        question
        ;

: money
        cr cr ." sorry!  i am broke too."
        cr ." why don't you sell encyclopedias of marry"
        cr ." someone rich or stop eating, so you won't "
        cr ." need so much money?"
        question
        ;

.( type 'help' to execute the help system =>)
help
cr .( money  =>) money  1000 delay
cr .( sex    =>) sex    1000 delay
cr .( job    =>) job    1000 delay
cr .( health =>) health 1000 delay


