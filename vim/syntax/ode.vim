syn keyword odeStatement     return global ode initial
syn keyword odeStatement     fn nextgroup=odeFunction skipwhite
syn keyword odeStatement     endfn nextgroup=odeFunction skipwhite
syn match   odeStatement     '\$.*$' display

syn keyword odeRepeat        while
syn keyword odeConditional   if else

syn keyword odeImport        import

syn match   odeFunction    '[a-zA-Z_][a-zA-Z0-9_]*' display contained


syn keyword odeOperator      and or
syn match   odeOperator      '\V=\|-\|+\|*\|/\|%\|<\|>\|!='
syn match   odeComment       '#.*$' display contains=odeTodo,@Spell
syn keyword odeTodo          TODO FIXME XXX contained

" Match strings
syn region odeString start=/"/ skip=/\\"/ end=/"/ oneline contains=odeInterpolatedWrapper
syn region odeInterpolatedWrapper start="\v\\\(\s*" end="\v\s*\)" contained containedin=odeString contains=odeInterpolatedString
syn match odeInterpolatedString "\v\w+(\(\))?" contained containedin=odeInterpolatedWrapper

syn match   odeNumber      '\<\d[lL]\=\>' display
syn match   odeNumber      '\<[0-9]\d\+[lL]\=\>' display
syn match   odeNumber      '\<\d\+[lLjJ]\>' display

syn match   odeFloat       '\.\d\+\%([eE][+-]\=\d\+\)\=[jJ]\=\>' display
syn match   odeFloat       '\<\d\+[eE][+-]\=\d\+[jJ]\=\>' display
syn match   odeFloat       '\<\d\+\.\d*\%([eE][+-]\=\d\+\)\=[jJ]\=' display

syn match odeFunctionCall '\%([^[:cntrl:][:space:][:punct:][:digit:]]\|_\)\%([^[:cntrl:][:punct:][:space:]]\|_\)*\ze\%(\s*(\)'


highlight default link odeStatement        Statement
highlight default link odeImport           Include
highlight default link odeFunction         Function
highlight default link odeFunctionCall     Function
highlight default link odeConditional      Conditional
highlight default link odeRepeat           Repeat
highlight default link odeOperator         Operator
highlight default link odeNumber           Number
highlight default link odeFloat            Float
highlight default link odeString           String
highlight default link odeComment          Comment
