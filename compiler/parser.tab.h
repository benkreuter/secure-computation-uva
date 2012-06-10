/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     NUMBER = 258,
     SLASH = 259,
     MINUS = 260,
     XOR = 261,
     AND = 262,
     STAR = 263,
     PLUS = 264,
     EOL = 265,
     SEM = 266,
     DBGMSG = 267,
     MSGTXT = 268,
     INPUT = 269,
     OUTPUT = 270,
     DOT = 271,
     FOR = 272,
     FROM = 273,
     TO = 274,
     LOOP = 275,
     END = 276,
     OB = 277,
     CB = 278,
     DEFVAR = 279,
     OCURL = 280,
     CCURL = 281,
     ARRAY = 282,
     DEFUN = 283,
     IF = 284,
     THEN = 285,
     ELSE = 286,
     G = 287,
     L = 288,
     GE = 289,
     LE = 290,
     EQ = 291,
     COMMA = 292,
     SHIFTL = 293,
     SHIFTR = 294,
     VAR = 295,
     RET = 296,
     STEP = 297,
     EQUALS = 298
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 2155 "parser.y"

  int val;
  struct symbol * wire;
  struct ast * tree;
  struct varlist * vlist;
  char sym[LINE_MAX];
  char msg[LINE_MAX];



/* Line 2068 of yacc.c  */
#line 104 "parser.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


