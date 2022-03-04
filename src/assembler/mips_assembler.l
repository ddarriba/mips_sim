%option header-file="src/assembler/mips_parser_lex.hpp"
%option noyywrap

%{
#include "../utils.h"
#include "mips_parser.hpp"

#include <iostream>

using namespace std;
using namespace mips_sim;

extern "C"
{
  int yylex( void );
}
%}

REGISTER (\$(zero|0|at|gp|sp|fp|ra|[kv][0-1]|a[0-3]|t[0-9]|s[0-7]))
INTVALUE (?:0x[0-9a-fA-F]+|0|[-|+]?[|1-9][0-9]*)
TEXTVALUE (\".*\")
FLOATVALUE ([0-9]+[\.]?[0-9]*)
LABELJUMP ([a-zA-Z][0-9A-zA-Z]+)
LABELTAG ([a-zA-Z][0-9A-zA-Z]+:)

DATA_TAG (".word"|".float"|".double"|".space"|".asciiz")
R_INSTRUCTION("add"|"sub"|"mul"|"div"|"and"|"or"|"xor"|"nor"|"slt"|"sll"|"srl"|"sra")
LUI_INSTRUCTION ("lui")
I_INSTRUCTION ("addi"|"subi"|"andi"|"ori"|"xori"|"beq"|"bneq")
I_MEM_INSTRUCTION ("lw"|"sw"|"lh"|"sh"|"lb"|"sb"|"lwc1"|"swc1")
J_INSTRUCTION ("j"|"jal"|"jr"|"jalr")
LA_INSTRUCTION ("la")

/* TODO */
FP_INSTRUCTION ("add.s"|"sub.s"|"mul.s"|"div.s"|"add.d"|"sub.d"|"mul.d"|"div.d")
FP_TWOREG ("mov.s"|"c.lt.s"|"c.le.s"|"c.eq.s"|"c.ne.s"|"c.ge.s"|"c.gt.s"|"c.lt.d"|"c.le.d"|"c.eq.d"|"c.ne.d"|"c.ge.d"|"c.gt.d")
FP_BRANCH ("bc1t"|"bc1f")

SYSCALL_INSTRUCTION ("syscall")

%%
".text"               { return TEXT_SECTION; }
".data"               { return DATA_SECTION; }
{DATA_TAG}            { yylval.tval = strdup(yytext+1); return DATATAG; }
{R_INSTRUCTION}	 	    { yylval.tval = strdup(yytext); return ROPCODE; }
{J_INSTRUCTION}		    { yylval.tval = strdup(yytext); return JOPCODE; }
{LA_INSTRUCTION}	    { return LAOPCODE; }
{I_INSTRUCTION}		    { yylval.tval = strdup(yytext); return IOPCODE; }
{I_MEM_INSTRUCTION}	  { yylval.tval = strdup(yytext); return IMOPCODE; }
{LUI_INSTRUCTION}		  { return LUIOPCODE; }
{SYSCALL_INSTRUCTION} { return SYSCALL; }

{REGISTER}	{ //yylval.tval = strdup(yytext);
              yylval.ival = Utils::find_register_by_name(yytext);
              return REGISTER; }
{INTVALUE}	{
              if (yytext[0] == '0' && yytext[1] == 'x')
              {
                /* hex */
                yylval.ival = (int)strtol(yytext, nullptr, 16);
              }
              else
              {
                yylval.ival = atoi(yytext);
              }
              return INTVALUE;
            }
{FLOATVALUE}	{ yylval.tval = strdup(yytext); return FLOATVALUE; }
{TEXTVALUE}	{ yylval.tval = strdup(yytext); return TEXTVALUE; }
{LABELTAG}  { yylval.tval = strdup(yytext); yylval.tval[strlen(yylval.tval)-1]='\0'; return LABELTAG; }
{LABELJUMP} { yylval.tval = strdup(yytext); return LABELJUMP; }
[ \t\n]+			{ /* Ignoring whitespaces and tabs */ }
"#".*       { /* Ignore comments */ }
\(					{ return OBRACKET; }
\)					{ return CBRACKET; }
,           { return COMMA; }
.					  { cerr << "[parser] undefined symbol " << yytext << endl; }
%%