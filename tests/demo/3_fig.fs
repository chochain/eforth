.( example 3. fig, forth interest group )

variable t3
: center cr ."   *  " ;
: sides  cr ." *   *" ;
: triad1 cr ." * * *" ;
: triad2 cr ." **  *" ;
: triad3 cr ." *  **" ;
: triad4 cr ."  *** " ;
: quart  cr ." ** **" ;
: right  cr ." * ***" ;
: bigt  bar center center center center center center ;
: bigi  center center center center center center center ;
: bign  sides triad2 triad2 triad1 triad3 triad2 sides ;
: bigg  triad4 sides post right triad1 sides triad4 ;
: fig  f bigi bigg ;

cr .( type 'fig' and a return on you keyboard to execute =>)
fig

