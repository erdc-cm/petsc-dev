#!/usr/bin/env python
#!/bin/env python
# ----------------------------------------------------------------------
#
# ----------------------------------------------------------------------

import sys
from sys import *
import lex
import re
# Reserved words

tokens = (
    # Literals (identifier, integer constant, float constant, string constant, char const)
    'ID', 'TYPEID', 'ICONST', 'FCONST', 'SCONST', 'CCONST',

    # Operators (+,-,*,/,%,|,&,~,^,<<,>>, ||, &&, !, <, <=, >, >=, ==, !=)
    'PLUS', 'MINUS', 'TIMES', 'DIVIDE', 'MOD',
    'OR', 'AND', 'NOT', 'XOR', 'LSHIFT', 'RSHIFT',
    'LOR', 'LAND', 'LNOT',
    'LT', 'LE', 'GT', 'GE', 'EQ', 'NE',
    
    # Assignment (=, *=, /=, %=, +=, -=, <<=, >>=, &=, ^=, |=)
    'EQUALS', 'TIMESEQUAL', 'DIVEQUAL', 'MODEQUAL', 'PLUSEQUAL', 'MINUSEQUAL',
    'LSHIFTEQUAL','RSHIFTEQUAL', 'ANDEQUAL', 'XOREQUAL', 'OREQUAL',

    # Increment/decrement (++,--)
    'PLUSPLUS', 'MINUSMINUS',

    # Structure dereference (->)
    'ARROW',

    # Conditional operator (?)
    'CONDOP',
    
    # Delimeters ( ) [ ] { } , . ; :
    'LPAREN', 'RPAREN',
    'LBRACKET', 'RBRACKET',
    'LBRACE', 'RBRACE',
    'COMMA', 'PERIOD', 'SEMI', 'COLON',

    # Ellipsis (...)
    'ELLIPSIS',

    'SPACE',

    'SLASH',

    'DOLLAR',
    'AT',
    'SHARP',
    'DOUBLEQUOTE'
    'QUOTE',
    'BACKQUOTE',
    'HAT'
    )

# Completely ignored characters
t_ignore           = '\t\x0c'

    
# Operators
t_PLUS             = r'\+'
t_MINUS            = r'-'
t_TIMES            = r'\*'
t_DIVIDE           = r'/'
t_MOD              = r'%'
t_OR               = r'\|'
t_AND              = r'&'
t_NOT              = r'~'
t_XOR              = r'\^'
t_LSHIFT           = r'<<'
t_RSHIFT           = r'>>'
t_LOR              = r'\|\|'
t_LAND             = r'&&'
t_LNOT             = r'!'
t_LT               = r'<'
t_GT               = r'>'
t_LE               = r'<='
t_GE               = r'>='
t_EQ               = r'=='
t_NE               = r'!='
t_SLASH            = r'\\'
t_DOLLAR           = r'\$'
t_AT               = r'@'
t_SHARP            = r'\#'
t_DOUBLEQUOTE      = r'"'
t_QUOTE            = r'\''
t_BACKQUOTE        = r'`'
t_HAT              = r'\^'

# Assignment operators

t_EQUALS           = r'='
t_TIMESEQUAL       = r'\*='
t_DIVEQUAL         = r'/='
t_MODEQUAL         = r'%='
t_PLUSEQUAL        = r'\+='
t_MINUSEQUAL       = r'-='
t_LSHIFTEQUAL      = r'<<='
t_RSHIFTEQUAL      = r'>>='
t_ANDEQUAL         = r'&='
t_OREQUAL          = r'\|='
t_XOREQUAL         = r'^='

# Increment/decrement
t_PLUSPLUS         = r'\+\+'
t_MINUSMINUS       = r'--'

# ->
t_ARROW            = r'->'

# ?
t_CONDOP           = r'\?'

# Delimeters
t_LPAREN           = r'\('
t_RPAREN           = r'\)'
t_LBRACKET         = r'\['
t_RBRACKET         = r'\]'
t_LBRACE           = r'\{'
t_RBRACE           = r'\}'
t_COMMA            = r','
t_PERIOD           = r'\.'
t_SEMI             = r';'
t_COLON            = r':'
t_ELLIPSIS         = r'\.\.\.'

t_SPACE            = r'\ +'
t_NEWLINE          = r'\n'

# Identifiers and reserved words

def t_ID(t):
    r'[A-Za-z_][\w_]*'
    return t

# Integer literal
t_ICONST = r'\d+([uU]|[lL]|[uU][lL]|[lL][uU])?'

# Floating literal
t_FCONST = r'((\d+)(\.\d+)(e(\+|-)?(\d+))? | (\d+)e(\+|-)?(\d+))([lL]|[fF])?'

# String literal
t_SCONST = r'\"([^\\\n]|(\\.))*?\"'

# Character constant 'c' or L'c'
t_CCONST = r'(L)?\'([^\\\n]|(\\.))*?\''


def t_error(t):
    print "Illegal character %s" % repr(t.value[0])
    t.skip(1)
#     return t
    
lexer = lex.lex(optimize=1)
if __name__ == "__main__":
#
#  Read in mapping of names to manual pages
#
    reg = re.compile('man:\+([a-zA-Z_0-9]*)\+\+([a-zA-Z_0-9 .:]*)\+\+\+\+man\+([a-zA-Z_0-9#./:]*)')
    fd = open("htmlmap")
    lines = fd.readlines()
    n = len(lines)
    mappedstring = { }
    mappedlink   = { }
    for i in range(0,n):
	fl = reg.search(lines[i])
	if not fl:
           print 'Bad line in manualpages.cit',lines[i]
        else:
            tofind = fl.group(1)
#   replace all _ in tofind with \_
            m     = len(tofind)
            tfind = tofind[0]
	    for j in range(1,m):
		if tofind[j] == '_':
		    tfind = tfind+'\\'
                tfind = tfind+tofind[j]
	    mappedstring[tfind] = fl.group(2)
	    mappedlink[tfind]   = fl.group(3)

#
#   Read in file that is to be mapped
    lines = sys.stdin.read()
    lex.input(lines)

    text    = ''
    bracket = 0
    while 1:
        token = lex.token()       # Get a token
        if not token: break        # No more tokens
	if token.type == 'NEWLINE':
	    print text
	    text = ''
	else:
	    if token.value[0] == '{':
		bracket = bracket + 1;
            if bracket == 0:
		value = token.value
		if mappedstring.has_key(value):
		    value = '\\trllink{'+mappedlink[value]+'}{'+mappedstring[value]+'}'
            else:
		value = token.value
	    if token.value[0] == '}':
		bracket = bracket - 1;
	    text = text+value

    



