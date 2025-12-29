if exists("b:current_syntax")
    finish
endif

syn keyword fhTodo         todo todo: tbd: tbd TODO TODO: TBD TBD: FIXME FIXME: XXX XXX: NOTE NOTE: note note: fixme: fixme xxx xxx: contained

" Statements
syntax keyword fhConditional if else elif
syntax keyword fhRepeat      for while repeat until
syntax keyword fhStatement   break continue return self super

syntax keyword fhKeyword   let fn include number string bool map array 
syntax keyword fhSimpleKeyword printf println print len append delete assert type error next_key contains_key io_open io_read io_write io_close io_seek math_tanh math_tan math_sqrt math_sinh math_sin math_randomseed math_random math_rad math_pow math_flt_epsilon math_pi math_modf math_min math_max math_log10 math_log math_ldexp math_huge math_frexp math_fmod math_floor math_ceil math_exp math_deg math_cosh math_cos math_atan2 math_atan math_asin math_sin math_acons math_cons math_abs string_trim string_char string_find string_upper string_lower string_reverse string_substr os_time os_difftime os_localtime os_command os_getenv os_getOS tostring tonumber getversion string_format docstring gc gcfrequency string_join gc_pause gc_info extends math_clamp string_split string_slice string_match grow insert tointeger math_maxval has eval gc_frequency io_tar_open io_tar_read io_tar_list io_tar_close io_tar_write_header io_tar_write_data io_tar_write_finalize reset has_index export math_md5 math_bcrypt_gen_salt math_bcrypt_hashpw io_scan_line

syntax keyword fhConstant true false null

" Comments
syn region fhComment start="#" end="$" contains=fhTodo keepend
syn region fhCommentL start="#-" end="-#" keepend

" Integer Numbers
syn case ignore
syn match fhDecimalInt display "\<\(0\|[1-9]\d*\)\(u\=l\{0,2}\|ll\=u\)\>"

" Float Numbers
syn match fhFloat display "\<\d\+\.\(e[+-]\=\d\+\)\=d\="
syn match fhFloat display "\<\.\d\+\(e[+-]\=\d\+\)\=d\="
syn match fhFloat display "\<\d\+e[+-]\=\d\+d\="
syn match fhFloat display "\<\d\+\.\d\+\(e[+-]\=\d\+\)\=d\="
syn case match

" Strings
syn match  fhSpecialError     contained "\\."
syn match  fhSpecialCharError contained "[^']"
syn match  fhSpecial          contained display "\\\(x\x\+\|\o\{1,3}\|.\|$\)"
syn match  fhSpecialChar      contained +\\["\\'0abfnrtvx]+
syn region fhString           start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=fhSpecial,fhSpecialError
syn match  fhCharacter        "L\='[^\\]'"
syn region  fhCharacter        start=+'+ end=+'+ contains=fhSpecial

highlight default link fhConditional  Conditional
highlight default link fhRepeat       Repeat
highlight default link fhStatement    Statement
highlight default link fhCommentL     fhComment
highlight default link fhCommentStart fhComment
highlight default link fhComment      Comment
highlight default link fhDecimalInt   Number
highlight default link fhInteger      Number
highlight default link fhFloat        Float
highlight default link fhKeyword       Keyword
highlight default link fhSimpleKeyword      Keyword
highlight default link fhConstant     Constant

highlight default link fhSpecialError     Error
highlight default link fhSpecialCharError Error
highlight default link fhString           String
highlight default link fhCharacter        Character
highlight default link fhSpecial          SpecialChar
highlight default link fhSpecialChar      SpecialChar

hi def link fhTodo Todo

let b:current_syntax = "fh"

