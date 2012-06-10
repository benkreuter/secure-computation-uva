/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 1 "parser.y"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "gate.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

extern int yylineno;
extern int use_binary;
extern uint64_t total_emitted_gates;
extern uint64_t xor, nxor;

FILE *  inputs_file;

/* 
 * Function scopes and conditional scopes must be treated
 * differently.  If we cannot find a variable in the current
 * function's scope, we do not want to look for it in the scope of
 * the calling function; we will just jump to the top level.  For a
 * conditional scope, however, we will jump to the next level until
 * we reach a function scope, and then jump to the top level.
 */
 enum {
   SCOPE_COND, SCOPE_FUN
 };

 /* 
  * Scopes are represented using a stack; when a scope is closed, we
  * pop it off of the stack.  This stack will also be used as a
  * function call stack, but in this case we have no need for
  * activation records; we are only concerned with local variables.
  */
struct scope_stack
{
  struct node * variables;
  struct scope_stack * next;
  int type;
};

enum symtypes {
  SYM_VAR = 0,
  SYM_FUN
};
struct ast;

/*
 * Function arguments are lists of variables.
 */
struct varlist
{
  char * name;
  struct varlist * next;
};

void free_var_list(struct varlist * vars)
{
  if(vars != 0)
    {
      free(vars->name);
      free_var_list(vars->next);
    }
}

struct symbol
{
  char * name;
  
  uint64_t * wires;

  /* When an array index is modified inside of an if statement, we
     should not be forced to multiplex the entire array.  The bytes in
     this array indicate which indices should be multiplexed.
  */
  uint8_t * dirty_idx;
  struct ast * body; /* used for function types */
  struct varlist * args;
  char * retvar;
  /* Width can be used to force constants to be a particular width */
  /* Elements describes how wide array elements are; width determines
     how wide the variable itself is.  Thus the number of array
     elements is (width / elements) */
  uint64_t isconst, width, elements;
  int version;
  int type;
};

/*
 * TODO: This could be turned into a hash table, to improve compiling
 * speed.  However, this is not a bottleneck just yet.
 */
struct node
{
  struct symbol * data;
  struct node * next;
};

 struct node * symlist;

void free_symbols(struct node * nodes)
{
  if(nodes == 0) return;
  if((nodes->data->name!=0) && (strcmp(nodes->data->name,"") != 0))
    {
      free(nodes->data->name);
      free(nodes->data->wires);
    }
  if(nodes->data->elements != 0)
    free(nodes->data->dirty_idx);
  free(nodes->data);
  free_symbols(nodes->next);
  free(nodes);
}

struct symbol * lookup_(struct node * cur, const char * name, int ex)
 {
   if(cur == 0)
     {
       if(ex != 0)
         {
           fprintf(stderr, "Variable %s not defined on line %d\n",name, yylineno);
           exit(-1);
         }
       return 0;
     }
   else if(strcmp(cur->data->name, name) == 0)
     {
       return cur->data;
     }
   else
     {
       return lookup_(cur->next, name, ex);
     }
 }

struct symbol * lookup(const char * name)
 {
   return lookup_(symlist, name, -1);
 }

struct symbol * func_lookup(const char * name)
{
  struct symbol * result = lookup(name);
  if(result->type != SYM_FUN)
    {
      fprintf(stderr, "%s is not a function\n",name);
      exit(-1);
      return 0;
    }
  return result;
}

struct symbol * funcassign_(struct node * cur, const char * name, struct ast * body, struct varlist * args, const char * retvar)
{
  assert(cur != 0);
  if(strcmp(cur->data->name, name) == 0)
    {
      /* Cannot redefine functions; maybe we will add support for
         overloading later. */
      fprintf(stderr, "Name %s is already used to define a %s\n", name, (cur->data->type == SYM_FUN) ? "function" : "variable");
      exit(-1);
      return 0;
    }
  else if(cur->next == 0)
    {
      struct node * newnode = malloc(sizeof(struct node));
      struct symbol * newdata = malloc(sizeof(struct symbol));
      newnode->data = newdata;
      newdata->type = SYM_FUN;
      newdata->name = malloc(strlen(name) + 1);
      newdata->retvar = malloc(strlen(retvar)+1);
      strcpy(newdata->retvar, retvar);
      strcpy(newdata->name, name);
      newdata->body = body;
      newdata->args = args;
      newnode->data->isconst = -1;
      cur->next = newnode;
      return newdata;
    }
  else
    {
      return funcassign_(cur->next, name, body, args, retvar);
    }

}

//struct symbol * constassign_(struct node * cur, const char * name, uint64_t start_wire, uint64_t end_wire, uint64_t width)
 struct symbol * constassign_(struct node * cur, const char * name, uint64_t * wires, uint64_t width)
{
  assert(cur != 0);
  if(strcmp(cur->data->name, name) == 0)
    {
      cur->data->width = width;

      /* The contract here is for the data to be allocated before this
         function is called*/
      free(cur->data->wires);
      cur->data->wires = wires;
      cur->data->isconst = -1;
      if(cur->data->elements != 0) free(cur->data->dirty_idx);
      cur->data->elements = 0;
      cur->data->dirty_idx = 0;

      return cur->data;
    }
  else if(cur->next == 0)
    {
      struct node * newnode = malloc(sizeof(struct node));
      struct symbol * newdata = malloc(sizeof(struct symbol));
      newdata->name = malloc(strlen(name) + 1);
      newdata->type = SYM_VAR;
      strcpy(newdata->name, name);

      newdata->wires = wires;
      /* newdata->start_wire = start_wire; */
      /* newdata->end_wire = end_wire; */
      //cur->data->wires = wires;
      newnode->data = newdata;
      newnode->next = 0;
      newnode->data->isconst = -1;
      newnode->data->width = width;
      newnode->data->elements = 0;
      newnode->data->dirty_idx = 0;
      cur->next = newnode;
      return newdata;
    }
  else
    {
      return constassign_(cur->next, name, wires, width);
    }
}

 //void constassign_scope(struct scope_stack * stack, const char * name, uint64_t start_wire, uint64_t end_wire, uint64_t width)
void constassign_scope(struct scope_stack * stack, const char * name, uint64_t *wire, uint64_t width)
{
  /* if(stack==0) constassign_(symlist,name,start_wire,end_wire,width); */
  /* else constassign_(stack->variables,name,start_wire,end_wire,width); */
  if(stack==0) constassign_(symlist,name,wire,width);
  else constassign_(stack->variables,name,wire,width);
}

 struct symbol * assign_(struct node * cur, const char * name, uint64_t *wire, uint64_t width, uint64_t elements, uint8_t * dirty_idx)
{
  assert(cur != 0);
  if(strcmp(cur->data->name, name) == 0)
    {
      if(cur->data->isconst != 0)
        {
          fprintf(stderr, "Attempt to assign to a constant variable.\n");
          exit(-1);
        }
      /* cur->data->start_wire = start_wire; */
      /* cur->data->end_wire = end_wire; */

      free(cur->data->wires);
      cur->data->wires = wire;
      cur->data->width = width;
      if(cur->data->elements != 0)
        free(cur->data->dirty_idx);
      if(elements != 0)
        {
          cur->data->dirty_idx = malloc(sizeof(uint8_t)*(width/elements));

          /*
           * Assignments to an array index will result in this
           * function being called, with an updated dirty_idx.
           */

          uint64_t i;
          for(i = 0; i < (width/elements); i++)
            if(dirty_idx != 0)
              cur->data->dirty_idx[i] = dirty_idx[i];
            else
              cur->data->dirty_idx[i] = 0;
        }
      else
        cur->data->dirty_idx = 0;
      cur->data->elements = elements;
      return cur->data;
    }
  else if(cur->next == 0)
    {
      struct node * newnode = malloc(sizeof(struct node));
      struct symbol * newdata = malloc(sizeof(struct symbol));
      newdata->name = malloc(strlen(name) + 1);
      newdata->type = SYM_VAR;
      strcpy(newdata->name, name);
      /* newdata->start_wire = start_wire; */
      /* newdata->end_wire = end_wire; */
      newdata->wires = wire;
      newdata->width = width;
      newnode->data = newdata;
      newnode->next = 0;
      newnode->data->isconst = 0;
      newnode->data->elements = elements;

      if(elements != 0)
        {
          uint64_t i;
          newnode->data->dirty_idx = malloc(sizeof(uint8_t) * (width / elements));
          for(i = 0; i < (width / elements) - 1; i++)
            if(dirty_idx != 0)
              newnode->data->dirty_idx[i] = dirty_idx[i];
            else
              newnode->data->dirty_idx[i] = 0;
        }
      else
        newnode->data->dirty_idx = 0;

      cur->next = newnode;
      return newdata;
    }
  else
    {
      return assign_(cur->next, name, wire, width, elements, dirty_idx);
    }
}

 /* void new(struct scope_stack * stack, char * name, int sw, int ew) */
void new(struct scope_stack * stack, char * name,uint64_t * wires, uint64_t width,uint64_t elements)
 {
   assign_(stack->variables, name, wires, width, elements, 0);
 }

 void * push_scope(struct scope_stack * stack)
 {
   struct scope_stack * newstack = malloc(sizeof(struct scope_stack));
   newstack->variables = malloc(sizeof(struct node));
   newstack->variables->next = 0;
   newstack->variables->data = malloc(sizeof(struct symbol));
   newstack->variables->data->name = "";
   newstack->variables->data->elements = 0;
   newstack->variables->data->dirty_idx = 0;
   newstack->type = SCOPE_COND;
   newstack->next = stack;
   return newstack;
 }

void * push_scope_fun(struct scope_stack * stack)
{
   struct scope_stack * newstack = malloc(sizeof(struct scope_stack));
   newstack->variables = malloc(sizeof(struct node));
   newstack->variables->next = 0;
   newstack->variables->data = malloc(sizeof(struct symbol));
   newstack->variables->data->name = "";

   newstack->variables->data->elements = 0;
   newstack->variables->data->dirty_idx = 0;
   newstack->type = SCOPE_FUN;
   newstack->next = stack;
   return newstack;
}

 struct symbol * get(struct scope_stack *, char *);

struct symbol * get_from_global_scope(struct scope_stack * stack, char * name)
 {
   if(stack == 0) return lookup(name);
   else
     {
       /* Find the highest scope that is not enclosed in a function */
       struct scope_stack * cur = stack;
       struct scope_stack * top = cur;

       /* For each function call scope that we find, set the candidate
          for the top level to be the enclosing scope.  When we are at
          the global level, that will be the highest level of
          conditional scoping before the function call. */
       while(cur != 0)
         {
           if(cur->type == SCOPE_FUN)
             {
               top = cur->next;
             }
           cur = cur->next;
         }
       return get(top, name);
     }
 }

struct symbol * get(struct scope_stack * stack, char * name)
{
  struct symbol * sym = 0;
  if(stack == 0)
    {
      return lookup(name);
    }
  sym = lookup_(stack->variables, name, 0);
  if(sym == 0)
    {
      /* If we are in a function, we should not fetch symbols from
         it's calling scope unless that is the global scope */
      if(stack->type != SCOPE_FUN)
        return get(stack->next, name);
      else if(check_sym(name) == 0) // Variable does not exist in the global scope
        {
          fprintf(stderr, "Undefined variable %s\n", name);
          return 0;
        }
      else
        /* We might be inset of a conditional statement, which has an
           assignment to a non-global variable. */
        return get_from_global_scope(stack->next, name);
    }
  else return sym;
}

int check_sym(const char * name)
{
  /* Look for a symbol in the global scope */
  struct node * v = symlist;
  while(v != 0)
    {
      if(strcmp(v->data->name,name) == 0)
        return -1;
      v = v->next;
    }
  return 0;
}

 struct symbol * assign(const char * name, uint64_t *start_wire, uint64_t end_wire, uint64_t elements, uint8_t * dirty_idx)
{
  return assign_(symlist, name, start_wire, end_wire, elements, dirty_idx);
}

/*
 * This is an assignment operation that is scope-aware; it changes the
 * value of a variable at the most recent enclosing scope or adds the
 * variable to the current scope.
 *
 * We are going to allow functions to have side-effects.  This is
 * probably what people want, because they are used to languages where
 * this is the case. 
 */
 void * assign_scope(struct scope_stack * scope, const char * name, uint64_t* start_wire, uint64_t end_wire, uint64_t elements, uint8_t * dirty_idx)
{
  if(scope == 0)
    {
      /* No dirty indices in the top level scope */
      return assign(name, start_wire, end_wire, elements, 0);
    }

  /* Go to the most recent if block, make assignment there. */
  if(scope->type == SCOPE_COND)
    {
      /* We are in a conditional scope*/
      /* If this is the first assigment in this scope, the dirty_idx
         will be zeroed out */
      return assign_(scope->variables, name, start_wire, end_wire, elements, dirty_idx);
    }

  /* TODO: Does this check need to be performed every time? */
  if(check_sym(name) == 0)
    {
      /* Variable does not exist in the global scope; it is safe to
         simply assign it a new value at the top level. */
      return assign_(scope->variables, name, start_wire, end_wire, elements, dirty_idx);
    }

  /* The variable exists in the global scope, but we might be in a
     conditional statement.  Descend the stack of scopes until we
     reach the global level or a conditional scope. */
  return assign_scope(scope->next,name,start_wire,end_wire, elements, dirty_idx);
}

uint64_t next_gate = -1;

struct ast
{
  struct ast * left, * right;
  void * (*action)(struct ast *);
  //  void * (*accept)(struct ast *, struct visitor *);
  void (*destroy)(struct ast *);
  void * data;

  //  char line[LINE_MAX];
  uint64_t line;
};

struct loop_data
{
  char * varname;
  uint64_t start, end;

  uint64_t step;
};

struct scope_stack * st = 0;

struct action_return
{
  uint64_t * wires;
  uint64_t width;
  uint64_t elements;
  uint8_t * dirty_idx;
};

struct action_return * copy_action_return(struct action_return * r)
{
  uint64_t i;
  struct action_return * ret = malloc(sizeof(struct action_return));
  ret->wires = malloc(sizeof(uint64_t)*(r->width));
  for(i = 0; i < r->width; i++)
    {
      ret->wires[i] = r->wires[i];
    }
  ret->width = r->width;
  ret->elements = r->elements;
  if(r->elements != 0)
    {
      ret->dirty_idx = malloc(sizeof(uint8_t)*(ret->width / ret->elements));
      for(i = 0; i < (r->width / r->elements); i++)
        ret->dirty_idx[i] = r->dirty_idx[i];
    }
  return ret;
}

void delete_action_return(struct action_return * r)
{
  free(r->wires);
  if(r->elements != 0) free(r->dirty_idx);
  free(r);
}

 void emit_subtractor(struct action_return * left_wires, struct action_return * right_wires, struct action_return * ret, struct ast*this);

void * loop_action(struct ast * tree)
{
  struct loop_data * data = tree->data;
  uint64_t i;
  void * d;

  for(i = data->start; i <= data->end; i+=data->step)
    {
      //  next_gate++;
      /* TODO: This needs to be fixed */
      /* TODO: Need to be able to assign width here */
      uint64_t * w = malloc(sizeof(uint64_t));
      w[0] = i;
      constassign_scope(st, data->varname, w, 1);//, 0);
      d = tree->left->action(tree->left);

      /* This was allocated dynamically */
      if(d != 0) delete_action_return(d); //free(d);
    }
  return 0;
}

void loop_destroy(struct ast * tree)
{
  struct loop_data* data = tree->data;
  free(data->varname);
  free(data);
}

void delete_tree(struct ast * tree)
{
  if(tree != 0)
    {
      struct ast * l = tree->left;
      struct ast * r = tree->right;
      tree->destroy(tree);
      delete_tree(l);
      delete_tree(r);
    }
}

/*
 * Assignments have no left subtree, only right subtrees.
 */
void * variable_assignment_action(struct ast * this)
{
  char * name = (char *)this->data;
  uint64_t i;
  //  uint64_t * wires = this->right->action(this->right);

  //  struct action_return * ww = 
  struct action_return * wires = this->right->action(this->right);//copy_action_return(ww);
  uint64_t * ww = malloc(sizeof(uint64_t)*(wires->width));
  for(i = 0; i < wires->width; i++) ww[i] = wires->wires[i];

  // All indices are modified here.
  uint8_t dirty_idx = 0;
  if(wires->elements != 0)
    {
      uint8_t * dirty_idx = (uint8_t*)(malloc(sizeof(uint8_t)*(wires->width / wires->elements)));
      for(i = 0; i < (wires->width / wires->elements); i++)
        dirty_idx[i] = 255;
    }

  assign_scope(st,name,ww, wires->width, wires->elements, dirty_idx);
  //  free(ww); // Hopefully this works
  //  free(wires);
  /*  free(wires);*/
  return wires;
}

void variable_assignment_destroy(struct ast * this)
{
  free(this->data);
}

void * variable_fetch_action(struct ast * this)
{
  uint64_t i, val;
  struct symbol * var = get(st, (char*)this->data); //lookup((char*)this->data);
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));
  if(var->isconst == 0)
    {
      /* ret[0] = var->start_wire; */
      /* ret[1] = var->end_wire; */
      ret->wires = malloc(sizeof(uint64_t)*(var->width));
      ret->width = var->width;
      ret->elements = var->elements;

      for(i = 0; i < var->width; i++)
        ret->wires[i] = var->wires[i];

      if(var->elements != 0)
        {
          ret->dirty_idx = malloc(sizeof(uint8_t)*(var->width / var->elements));
          for(i = 0; i < (var->width / var->elements); i++)
            ret->dirty_idx[i] = var->dirty_idx[i];
        }
    }
  else
    {
      /* Generate wires */
      uint64_t inputs[] = {1,1};
      //      ret[0] = next_gate+1;
      //      val = var->start_wire;
      val = var->wires[0];
      for(i = 0; val >= 2; i++) val /= 2;

      ret->width = i+2;
      if(var->width > 1) ret->width = var->width;

      ret->wires = malloc(sizeof(uint64_t)*(ret->width));
      val = var->wires[0];
      for(i = 0; val >= 2; i++)
        {

          next_gate++;
          /* 11b = 3 */
          add_gate(next_gate, 2, (val%2 == 0) ? 0 : 0xF, inputs, 0,0);
          ret->wires[i] = next_gate;
          val /= 2;
        }
      next_gate++;
      add_gate(next_gate, 2, (val%2 == 0) ? 0 : 0xF, inputs, 0,0);
      ret->wires[i+1] = next_gate;

      if(var->width > 1)
        {
          uint64_t sz = i+2; //next_gate - ret[0];
          for(i = 0; i < var->width - sz; i++)
            {
              next_gate++;
              add_gate(next_gate, 2, 0, inputs, 0,0);
              ret->wires[i + sz] = next_gate;
            }
        }
      assert(ret->width == (ret->wires[ret->width-1] - ret->wires[0]));
      ret->elements = 0;
      //      ret[1] = next_gate;
    }
  return ret;
}

void variable_fetch_destroy(struct ast * this)
{
  free(this->data);
}

void null_destroy(struct ast * this)
{
}

void * or_action(struct ast * this)
{
  uint64_t j;
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));
  /* uint64_t * left_wires = this->left->action(this->left); */
  /* uint64_t * right_wires = this->right->action(this->right); */
  struct action_return * left_wires = this->left->action(this->left);
  struct action_return * right_wires = this->right->action(this->right);
  //  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
  if((left_wires->width != right_wires->width) || (left_wires->elements != right_wires->elements))
    {
      fprintf(stderr, "Operation size mismatch in XOR %lu vs. %lu\n", left_wires->width, right_wires->width); //left_wires[1] - left_wires[0], right_wires[1] - right_wires[0]);
      exit(-1);
    }
  //  ret[0] = next_gate+1;
  ret->elements = left_wires -> elements;
  ret->width = left_wires->width;
  ret->wires = malloc(sizeof(uint64_t)*(left_wires->width));
  if(left_wires->elements != 0)
    {
      ret->dirty_idx = malloc(sizeof(uint8_t)*(left_wires->width / left_wires->elements));
      for(j = 0; j < (left_wires->width / left_wires->elements); j++)
        ret->dirty_idx[j] = 255;
    }
  //  for(j = 0; j <=  left_wires[1] - left_wires[0]; j++)
  for(j = 0; j < ret->width; j++)
    {
      /* uint64_t inputs[] = {left_wires[0]+j,right_wires[0]+j}; */
      uint64_t inputs[] = {left_wires->wires[j],right_wires->wires[j]};
      next_gate++;

      add_gate(next_gate, 2, 0x6, inputs, 0,0);
      ret->wires[j] = next_gate;
    }
  /* free(left_wires); */
  /* free(right_wires); */
  /* ret[1] = next_gate; */
  delete_action_return(left_wires);
  delete_action_return(right_wires);
  //  ret->elements = 0;
  return ret;
}

void * and_action(struct ast * this)
{
  uint64_t i;
  /* uint64_t * ret = malloc(sizeof(uint64_t)*2); */
  /* uint64_t * left_wires = this->left->action(this->left); */
  /* uint64_t * right_wires = this->right->action(this->right); */
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct action_return * left_wires = this->left->action(this->left);
  struct action_return * right_wires = this->right->action(this->right);
  //  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
  if((left_wires->width != right_wires->width)|| (left_wires->elements != right_wires->elements))
    {
      fprintf(stderr, "Operation size mismatch in AND on line %d\n", this->line);
      exit(-1);
    }
  //  ret[0] = next_gate+1;
  ret->width = left_wires->width;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  ret->elements = left_wires->elements;
  if(left_wires->elements != 0)
    {
      ret->dirty_idx = malloc(sizeof(uint8_t)*(left_wires->width / left_wires->elements));
      uint64_t j;
      for(j = 0; j < (left_wires->width / left_wires->elements); j++)
        ret->dirty_idx[j] = 255;
    }

  //  for(i = 0; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 0; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i};
      uint64_t inputs[] = {left_wires->wires[i], right_wires->wires[i]};
      next_gate++;     

      add_gate(next_gate, 2, 1, inputs, 0,0);
      ret->wires[i] = next_gate;
    }

  /* free(left_wires); */
  /* free(right_wires); */
  /* ret[1] = next_gate; */
  delete_action_return(left_wires);
  delete_action_return(right_wires);
  //printf("returning %d %d\n", ret[0], ret[1]);

  //  ret->elements = 0;
  return ret;
}

struct output_data
{
  char * var;
  uint64_t party;
};

void * output_action(struct ast * this)
{
  struct output_data * data = this->data;
  struct symbol * s = get(st, data->var); //lookup(data->var);
  uint64_t i;
  //  for(i = 0; i <= s->end_wire - s->start_wire; i++)
  for(i = 0; i < s->width; i++)
    {
      //      uint64_t inputs[] = {s->start_wire+i, 0};
      uint64_t inputs[] = {s->wires[i],0};
      char comment[LINE_MAX+1];
      sprintf(comment, "//output$output.%s$%d", (data->party == 0) ? "alice" : "bob", (int)i);
      //char * parties[]
      next_gate++;
 
      add_gate(next_gate, 1, 1, inputs, -1, data->party);
    }
  return 0;
}

void output_destroy(struct ast * this)
{
  struct output_data * data = this->data;
  free(data->var);
  free(this->data);
}

void * command_action(struct ast * this)
{
  if(this->right != 0)
    {
      //      void * v = this->right->action(this->right);
      struct action_return * v = this->right->action(this->right);
      if(v != 0)
        delete_action_return(v);
        //        free(v);
    }
  if(this->left != 0)
    {
      //      void * v = this->left->action(this->left);
      struct action_return * v = this->left->action(this->left);
            if(v != 0)
              delete_action_return(v);
        //        free(v);
    }
  return 0;
}

void * integer_add_action(struct ast * this)
{
  uint64_t i, carry_s, carry_e, x, y, z;
  /* uint64_t * ret = malloc(sizeof(uint64_t)*2); */
  /* uint64_t * left_wires = this->left->action(this->left); */
  /* uint64_t * right_wires = this->right->action(this->right); */
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct action_return * left_wires = this->left->action(this->left);
  struct action_return * right_wires = this->right->action(this->right);

  if(left_wires->elements != 0)
    {
      ret->dirty_idx = malloc(sizeof(uint8_t)*(left_wires->width / left_wires->elements));
      uint64_t j;
      for(j = 0; j < (left_wires->width / left_wires->elements); j++)
        ret->dirty_idx[j] = 255;
    }

  //  uint64_t tmp[] = {left_wires[0], right_wires[0]};
  uint64_t tmp[] = {left_wires->wires[0], right_wires->wires[0]};
  //  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
  if((left_wires ->width != right_wires->width) || (left_wires->elements != right_wires->elements))
    {
      fprintf(stderr, "Operation size mismatch in addition on line %d\n", this->line);
      exit(-1);
    }
  ret->elements = left_wires->elements;
  carry_s = next_gate+1;
  /* First adder has no carry in */
  next_gate++;


  add_gate(next_gate, 2, 1, tmp, 0, 0);

  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      /* uint64_t inputa[] = {left_wires[0]+i, right_wires[0]+i}; */
      /* uint64_t inputb[] = {left_wires[0]+i, next_gate}; */
      uint64_t inputa[] = {left_wires->wires[i], right_wires->wires[i]};
      uint64_t inputb[] = {left_wires->wires[i], next_gate};
      uint64_t inputc[] = {0,0};
      uint64_t inputd[] = {0,0};
      next_gate++;

      //      add_gate(next_gate, 3, "0 0 0 1 0 1 1 1", inputs, 0, 0);
      add_gate(next_gate, 2, 6, inputa, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputb, 0, 0);
      next_gate++;

      inputc[0] = next_gate-2;
      inputc[1] = next_gate-1;
      add_gate(next_gate, 2, 1, inputc, 0, 0);

      next_gate++;
      inputd[0] = next_gate-1;
      inputd[1] = left_wires->wires[i];
      add_gate(next_gate, 2, 6, inputd, 0, 0);
    }
  carry_e = next_gate+1;
  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i};
      uint64_t inputs[] = {left_wires->wires[i], right_wires->wires[i]};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }
  //  ret[0] = next_gate+1;
  ret->width = left_wires->width;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  next_gate++;

  add_gate(next_gate, 2, 6, tmp, 0, 0);
  ret->wires[0] = next_gate;

  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      uint64_t inputs[] = {carry_e + i - 1, carry_s + 4*(i - 1)};//{left_wires[0]+i, right_wires[0]+i, carry_s + i - 1};
      next_gate++;
      // Inputs:  a, b, carry_in
      // Outputs: sum, carry_out
      add_gate(next_gate, 2, 6, inputs, 0, 0);
      //      add_gate(next_gate, 3, "0 1 1 0 1 0 0 1", inputs, 0, 0);
      ret->wires[i] = next_gate;
    }
  /* free(left_wires); */
  /* free(right_wires); */
  /* ret[1] = next_gate; */
  delete_action_return(left_wires);
  delete_action_return(right_wires);
  //printf("returning %d %d\n", ret[0], ret[1]);

  return ret;
}

void emit_subtractor(struct action_return * left_wires, struct action_return * right_wires, struct action_return * ret, struct ast * this)
{
  uint64_t i, carry_s, carry_e, x, y, z, a, b, one_wire, zero_wire;
  uint64_t tmp[] = {left_wires->wires[0], right_wires->wires[0]};
  uint64_t dummy_inputs[] = {0,0};
  //  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
  if((left_wires->width != right_wires->width) || (left_wires->elements != right_wires->elements))
    {
      fprintf(stderr, "Operation size mismatch in subtraction non line %d\n", this->line);
      exit(-1);
    }
  ret->elements = left_wires->elements;
  if(left_wires->elements != 0)
    {
      ret->dirty_idx = malloc(sizeof(uint8_t)*(left_wires->width / left_wires->elements));
      uint64_t j;
      for(j = 0; j < (left_wires->width / left_wires->elements); j++)
        ret->dirty_idx[j] = 255;
    }

  one_wire = next_gate+1;
  next_gate++;
  add_gate(next_gate, 2, 0xF, dummy_inputs, 0, 0);

  zero_wire = next_gate+1;
  next_gate++;
  add_gate(next_gate, 2, 0x0, dummy_inputs, 0, 0);

  /* Invert and add one */
  a = next_gate+1;
  //  for(i = 0; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 0; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {right_wires[0]+i, one_wire};
      uint64_t inputs[] = {right_wires->wires[i], one_wire};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }

  carry_s = next_gate+1;
  /* First adder has no carry in */
  next_gate++;

  tmp[0] = a;
  tmp[1] = one_wire;
  add_gate(next_gate, 2, 1, tmp, 0, 0);

  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      uint64_t inputa[] = {zero_wire, a+i};
      uint64_t inputb[] = {zero_wire, next_gate};
      uint64_t inputc[] = {0,0};
      uint64_t inputd[] = {0,0};
      next_gate++;

      //      add_gate(next_gate, 3, "0 0 0 1 0 1 1 1", inputs, 0, 0);
      add_gate(next_gate, 2, 6, inputa, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputb, 0, 0);
      next_gate++;

      inputc[0] = next_gate-2;
      inputc[1] = next_gate-1;
      add_gate(next_gate, 2, 1, inputc, 0, 0);

      next_gate++;
      inputd[0] = next_gate-1;
      inputd[1] = zero_wire;
      add_gate(next_gate, 2, 6, inputd, 0, 0);
    }
  carry_e = next_gate+1;
  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      uint64_t inputs[] = {zero_wire, a+i};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }
  b = next_gate+1;
  next_gate++;
  tmp[0] = a;
  tmp[1] = one_wire;

  add_gate(next_gate, 2, 6, tmp, 0, 0);


  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      uint64_t inputs[] = {carry_e + i - 1, carry_s + 4*(i - 1)};//{left_wires[0]+i, right_wires[0]+i, carry_s + i - 1};
      next_gate++;
      // Inputs:  a, b, carry_in
      // Outputs: sum, carry_out
      add_gate(next_gate, 2, 6, inputs, 0, 0);
      //      add_gate(next_gate, 3, "0 1 1 0 1 0 0 1", inputs, 0, 0);
    }


  carry_s = next_gate + 1;
  tmp[0] = left_wires->wires[0];
  tmp[1] = b;
  next_gate++;
  add_gate(next_gate, 2, 1, tmp, 0, 0);

  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      /* uint64_t inputa[] = {left_wires[0]+i, b+i}; */
      /* uint64_t inputb[] = {left_wires[0]+i, next_gate}; */
      uint64_t inputa[] = {left_wires->wires[i], b+i};
      uint64_t inputb[] = {left_wires->wires[i], next_gate};
      uint64_t inputc[] = {0,0};
      uint64_t inputd[] = {0,0};
      next_gate++;

      //      add_gate(next_gate, 3, "0 0 0 1 0 1 1 1", inputs, 0, 0);
      add_gate(next_gate, 2, 6, inputa, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputb, 0, 0);
      next_gate++;

      inputc[0] = next_gate-2;
      inputc[1] = next_gate-1;
      add_gate(next_gate, 2, 1, inputc, 0, 0);

      next_gate++;
      inputd[0] = next_gate-1;
      inputd[1] = left_wires->wires[i];
      add_gate(next_gate, 2, 6, inputd, 0, 0);
    }
  carry_e = next_gate+1;
  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, b+i};
      uint64_t inputs[] = {left_wires->wires[i],b+i};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }

  //  ret[0] = next_gate+1;
  ret->width = left_wires->width;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));

  next_gate++; 
  tmp[0] = left_wires->wires[0];
  tmp[1] = b;
  add_gate(next_gate, 2, 6, tmp, 0, 0);
  ret->wires[0] = next_gate;

  /* next_gate++;  */
  /* tmp[0] = next_gate - 1; */
  /* tmp[1] = one_wire; */
  /* add_gate(next_gate, 2, 6, tmp, 0, 0); */

  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      uint64_t inputs[] = {carry_e + i - 1, carry_s + 4*(i - 1)};//{left_wires[0]+i, right_wires[0]+i, carry_s + i - 1};
      next_gate++;
      // Inputs:  a, b, carry_in
      // Outputs: sum, carry_out
      add_gate(next_gate, 2, 6, inputs, 0, 0);
      ret->wires[i] = next_gate;
      //      add_gate(next_gate, 3, "0 1 1 0 1 0 0 1", inputs, 0, 0);
    }
  /* free(left_wires); */
  /* free(right_wires); */
  /* ret[1] = next_gate; */
  delete_action_return(left_wires);
  delete_action_return(right_wires);
  //printf("returning %d %d\n", ret[0], ret[1]);
}

void * integer_sub_action(struct ast * this)
{
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct action_return * left_wires = this->left->action(this->left);
  struct action_return * right_wires = this->right->action(this->right);

  emit_subtractor(left_wires, right_wires, ret, this);
  return ret;
}


void * constant_action(struct ast * tree)
{
  uint64_t i, bit,a,b;
  uint64_t num = *((uint64_t*)tree->data);
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));
  uint64_t inputs[] = {1,1};
  //  ret[0] = next_gate+1;
  a = next_gate+1;
  for(i = 0; num >= 2; i++)
    {

      next_gate++;

      add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);
      num /= 2;
    }
  next_gate++;

  add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);
  b = next_gate;
  //  ret[1] = next_gate;

  ret->width = b-a+1;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  for(i = 0; i < ret->width; i++)
    {
      ret->wires[i] = a+i;
    }
  ret->elements = 0;
  return ret;
}

void * constant_action_2(struct ast * tree)
{
  uint64_t i, bit,a,b;
  uint64_t num = ((uint64_t*)tree->data)[0];
  uint64_t width = ((uint64_t*)tree->data)[1];
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));
  uint64_t inputs[] = {1,1};

  //  ret[0] = next_gate+1;
  a = next_gate+1;
  for(i = 0; num >= 2; i++)
    {
      next_gate++;

      add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);
      num /= 2;
    }
  next_gate++; 

  add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);

  //  bit = next_gate - ret[0]+1;
  bit = next_gate - a + 1;

  for(i = 0; i < width - bit; i++)
    {
      next_gate++;

      add_gate(next_gate, 2, 0, inputs, 0, 0);
    }

  //  ret[1] = next_gate;
  b = next_gate;

  ret->width = width;//b-a+1;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  for(i = 0; i < ret->width; i++)
    {
      ret->wires[i] = a+i;
    }
  ret->elements = 0;
  return ret;
}

void constant_destroy(struct ast * tree)
{
  free(tree->data);
}

void mux(struct node * vars, struct scope_stack * s, uint64_t wire)
{
  if(vars == 0) return;
  else if(strcmp(vars->data->name,"") != 0)
    {
      //  fprintf(stderr, "muxing\n");
      struct symbol * sym = get(s, vars->data->name);

      if(vars->data->elements != sym->elements)
        {
          fprintf(stderr, "Error: attempt to change array size in conditional statement\n");
          exit(-1);
        }

      uint64_t i,sw,ew,w,x,y,z,j;

      uint64_t width;

      if(sym->elements == 0)
        {
          // Not an array, just mux as normal

          w = next_gate+1;
          //      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
          for(i = 0; i < sym->width; i++)
            {
              //          uint64_t inputs[] = {sym->start_wire+i, vars->data->start_wire+i};
              uint64_t inputs[] = {sym->wires[i],vars->data->wires[i]};
              next_gate++;
              add_gate(next_gate, 2, 6, inputs, 0,0);
            }
          x = next_gate;
          y = next_gate+1;
          //      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
          for(j = 0; j < sym->width; j++)
            {
              assert(j < sym->width);
              uint64_t inputs[] = {w+j, wire};
              next_gate++;
              add_gate(next_gate, 2, 1, inputs, 0, 0);
            }
          z = next_gate;
          sw = next_gate+1;
          //      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
          for(i = 0; i < sym->width; i++)
            {
              //      fprintf(stderr, "here %lld %lld\n", sym->start_wire, sym->end_wire);
              //          uint64_t inputs[] = {y+i,sym->start_wire+i}; //{sym->start_wire+i,vars->data->start_wire+i,wire};
              uint64_t inputs[] = {y+i, sym->wires[i]};
              next_gate++;
              add_gate(next_gate, 2, 6, inputs, 0,0);
              //          add_gate(next_gate, 3, "0 1 0 1 0 0 1 1", inputs,0,0);
            }
          ew = next_gate;
          uint64_t * wires = malloc(sizeof(uint64_t)*(ew-sw+1));
          for(i = 0; i <= ew-sw; i++)
            {
              wires[i] = sw+i;
            }
          //      if(s != 0) assign_(s->variables, vars->data->name,sw,ew);
          if(s!=0) assign_(s->variables, vars->data->name, wires, ew-sw+1, vars->data->elements, 0);
          //      else assign(vars->data->name,sw,ew);
          else assign(vars->data->name, wires, ew-sw+1, vars->data->elements, 0);
          //    mux(vars->next, s, wire);
        }
      else
        {
          // Only mux the dirty indices
          uint64_t * wires = malloc(sizeof(uint64_t)*(sym->width));
          uint64_t xxxxx;
          for(xxxxx = 0; xxxxx < sym->width; xxxxx++) wires[xxxxx] = sym->wires[xxxxx];

          for(xxxxx = 0; xxxxx < (sym->width / sym->elements); xxxxx++)
            {
              if(vars->data->dirty_idx[xxxxx] != 0)
                {
                  // This index was modified and should be muxed
                  uint64_t tsw = sym->elements * xxxxx;
                  w = next_gate+1;
                  //      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
                  for(i = 0; i < sym->elements; i++)
                    {
                      //          uint64_t inputs[] = {sym->start_wire+i, vars->data->start_wire+i};
                      uint64_t inputs[] = {sym->wires[tsw + i],vars->data->wires[tsw + i]};
                      next_gate++;
                      add_gate(next_gate, 2, 6, inputs, 0,0);
                    }
                  x = next_gate;
                  y = next_gate+1;
                  //      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
                  for(j = 0; j < sym->elements; j++)
                    {
                      //assert(j < sym->width);
                      uint64_t inputs[] = {w+j, wire};
                      next_gate++;
                      add_gate(next_gate, 2, 1, inputs, 0, 0);
                    }
                  z = next_gate;
                  sw = next_gate+1;
                  //      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
                  for(i = 0; i < sym->elements; i++)
                    {
                      //      fprintf(stderr, "here %lld %lld\n", sym->start_wire, sym->end_wire);
                      //          uint64_t inputs[] = {y+i,sym->start_wire+i}; //{sym->start_wire+i,vars->data->start_wire+i,wire};
                      uint64_t inputs[] = {y+i, sym->wires[tsw + i]};
                      next_gate++;
                      add_gate(next_gate, 2, 6, inputs, 0,0);
                      //          add_gate(next_gate, 3, "0 1 0 1 0 0 1 1", inputs,0,0);
                    }
                  ew = next_gate;
                  //                  uint64_t * wires = malloc(sizeof(uint64_t)*(ew-sw+1));
                  for(i = 0; i <= ew-sw; i++)
                    {
                      wires[tsw + i] = sw+i;
                    }
                }
            }
          struct symbol * sym_;
          uint8_t * dirty_idx = malloc(sizeof(uint8_t) * (sym->width / sym->elements));
          for(i = 0; i < (sym->width / sym->elements); i++)
            if(sym->dirty_idx[i] != 0)// || (vars->data->dirty_idx[i] != 0))
              dirty_idx[i] = 255;
            else
              dirty_idx[i] = 0;
          //          if(s!=0) sym_ = assign_(s->variables, vars->data->name, wires, ew-sw+1, vars->data->elements, dirty_idx);
          if(s!=0) sym_ = assign_(s->variables, vars->data->name, wires, sym->width, vars->data->elements, dirty_idx);
          //      else assign(vars->data->name,sw,ew);
          //          else sym_ = assign(vars->data->name, wires, ew-sw+1, vars->data->elements, dirty_idx);

          // Really should use assign_scope here

          else sym_ = assign(vars->data->name, wires, sym->width, vars->data->elements, 0);//dirty_idx);
          free(dirty_idx);
        }
    }
  mux(vars->next,s,wire);
}


void * if_action(struct ast * tree)
{
  /*
   * The left subtree will be the conditional expression, the right
   * subtree will be the body.
   */
  struct scope_stack * tmp;
  void * right_ret;
  struct action_return * cond_wires = tree->left->action(tree->left);

  st = push_scope(st);

  right_ret = tree->right->action(tree->right);
  if(right_ret != 0) delete_action_return(right_ret);

  tmp = st;
  st = st->next;

  mux(tmp->variables, st,cond_wires->wires[0]);

  free_symbols(tmp->variables);
  free(tmp);

  delete_action_return(cond_wires);
  return 0;
}

void * else_action(struct ast * tree)
{
  /*
   * The left subtree will be the conditional of the if block, the
   * right subtree will be the body of the else.
   */
  struct scope_stack * tmp;
  void * right_ret;

  struct action_return * cond_wires = ((struct ast*)tree->data)->action(tree->data);

  uint64_t inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0xF, inputs, 0, 0);


  inputs[0] = cond_wires->wires[0];
  inputs[1] = next_gate;

  next_gate++;

  add_gate(next_gate, 2, 6, inputs, 0, 0);

  cond_wires->wires[0] = next_gate;


  st = push_scope(st);

  right_ret = tree->right->action(tree->right);
  if(right_ret != 0) delete_action_return(right_ret);

  tmp = st;
  st = st->next;

  mux(tmp->variables, st,cond_wires->wires[0]);

  free_symbols(tmp->variables);
  free(tmp);

  delete_action_return(cond_wires);
  return 0;
}

void * eq_action(struct ast * tree)
{
  uint64_t i;
  struct action_return * left_wires = tree->left->action(tree->left);
  struct action_return * right_wires = tree->right->action(tree->right);
  struct action_return * ret = malloc(sizeof(struct action_return));
  //  uint64_t tmp[] = {left_wires[0], right_wires[0]};
  uint64_t tmp[] = {left_wires->wires[0], right_wires->wires[0]};

  //  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
  if(left_wires->width != right_wires->width)
    {
      fprintf(stderr, "Operation size mismatch in comparison on line %d\n", tree->line);
      exit(-1);
    }

  next_gate++;

  add_gate(next_gate, 2, 9, tmp, 0, 0);

  /* Generate an AND tree */
  //  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
  for(i = 1; i < left_wires->width; i++)
    {
      //      uint64_t inputs[] = {left_wires[0] + i, right_wires[0] + i};
      //      uint64_t inputs[] = {left_wires[i], right_wires[i]};
      uint64_t inputs[] = {left_wires->wires[i], right_wires->wires[i]};
      uint64_t inputs2[] = {next_gate+1, next_gate};
      next_gate++;

      add_gate(next_gate, 2, 9, inputs, 0, 0); 

      next_gate++;

      add_gate(next_gate, 2, 1, inputs2, 0, 0);
    } 
  //  ret[0] = ret[1] = next_gate;
  ret->width = 1;
  ret->wires = malloc(sizeof(uint64_t));
  ret->wires[0] = next_gate;
  ret->elements = 0;

  /* free(left_wires); */
  /* free(right_wires); */

  delete_action_return(left_wires);
  delete_action_return(right_wires);

  return ret;
}

void * l_action(struct ast* tree)
{
  /*
   * We want to test if the left subtree evaluates to something less
   * than the right subtree.  What we will do is generate a subtractor
   * to compute LHS - RHS, then return the high order bit.
   *
   * Since all our math is unsigned, we should append a zero to the
   * highest order bit of the two variables
   */

  struct action_return * left_wires = tree->left->action(tree->left);
  struct action_return * right_wires = tree->right->action(tree->right);
  struct action_return * ret = (struct action_return *)(malloc(sizeof(struct action_return)));

  uint64_t inputs[2] = {0,0};

  next_gate++;
  add_gate(next_gate, 2, 0x0, inputs, 0, 0);


  uint64_t * l_wires, * r_wires;
  l_wires = (uint64_t*)(malloc(sizeof(uint64_t)*(left_wires->width + 1)));
  r_wires = (uint64_t*)(malloc(sizeof(uint64_t)*(right_wires->width +1)));

  uint64_t i;
  for(i = 0; i < left_wires->width; i++)
    {
      l_wires[i] = left_wires->wires[i];
      r_wires[i] = right_wires->wires[i];
    }

  l_wires[left_wires->width] = next_gate;
  r_wires[right_wires->width] = next_gate;

  free(left_wires->wires);
  free(right_wires->wires);

  if(left_wires->elements != 0) free(left_wires->dirty_idx);
  if(right_wires->elements != 0) free(right_wires->dirty_idx);
  left_wires->elements = right_wires->elements = 0;

  left_wires->wires = l_wires;
  right_wires->wires = r_wires;

  left_wires->width++;
  right_wires->width++;

  emit_subtractor(left_wires, right_wires, ret, tree);

  /* 
   * Now we have the difference, so we extract the high order bit and
   * return it.  Note that conditional statements will only take the 0
   * element from the wires, so we are free to leave the rest there.
   */
  uint64_t ho_bit = ret->wires[ret->width-1];
  ret->wires[0] = ho_bit;
  ret->width = 1;

  return ret;
}


void * g_action(struct ast* tree)
{
  struct action_return * left_wires = tree->left->action(tree->left);
  struct action_return * right_wires = tree->right->action(tree->right);
  struct action_return * ret = (struct action_return *)(malloc(sizeof(struct action_return)));

  uint64_t inputs[2] = {0,0};

  next_gate++;
  add_gate(next_gate, 2, 0x0, inputs, 0, 0);


  uint64_t * l_wires, * r_wires;
  l_wires = (uint64_t*)(malloc(sizeof(uint64_t)*(left_wires->width + 1)));
  r_wires = (uint64_t*)(malloc(sizeof(uint64_t)*(right_wires->width +1)));

  uint64_t i;
  for(i = 0; i < left_wires->width; i++)
    {
      l_wires[i] = left_wires->wires[i];
      r_wires[i] = right_wires->wires[i];
    }

  l_wires[left_wires->width] = next_gate;
  r_wires[right_wires->width] = next_gate;

  free(left_wires->wires);
  free(right_wires->wires);

  if(left_wires->elements != 0) free(left_wires->dirty_idx);
  if(right_wires->elements != 0) free(right_wires->dirty_idx);
  left_wires->elements = right_wires->elements = 0;

  left_wires->wires = l_wires;
  right_wires->wires = r_wires;

  left_wires->width++;
  right_wires->width++;

  emit_subtractor(right_wires, left_wires, ret, tree);

  /* 
   * Now we have the difference, so we extract the high order bit and
   * return it.  Note that conditional statements will only take the 0
   * element from the wires, so we are free to leave the rest there.
   */
  uint64_t ho_bit = ret->wires[ret->width-1];
  ret->wires[0] = ho_bit;
  ret->width = 1;

  return ret;
}


void * not_action(struct ast * tree)
{
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct action_return * wires = tree->right->action(tree->right);
  uint64_t inputs[] = {1,1};

  next_gate++;
  add_gate(next_gate, 2, 0xF, inputs, 0, 0);

  //  inputs[0] = wires[0];
  inputs[0] = wires->wires[0];
  inputs[1] = next_gate;


  next_gate++;

  add_gate(next_gate, 2, 6, inputs, 0, 0);

  ret->width = 1;
  ret->wires = malloc(sizeof(uint64_t));
  ret->wires[0] = next_gate;
  ret->elements = 0;

  delete_action_return(wires);
  return ret;
}

struct array_fetch_data
{
  char * var, * index;
  uint64_t idx;
  uint8_t varidx;
};

void * array_fetch_action(struct ast * tree)
{
  struct array_fetch_data * data = tree->data;
  char * name = malloc(strlen(data->var) + 11);
  struct symbol * index;
  uint64_t idx;
  if(data->varidx != 0) 
    {
      index = get(st, data->index); //lookup(data->index);
      idx = index->wires[0];
      if(index->isconst == 0)
        {
          fprintf(stderr, "Attempt to use non-constant array index on line %d\n", tree->line);
          exit(-1);
        }

    }
  else
    {
      idx = data->idx;
    }

  struct symbol * sym;
  uint64_t i;
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));

  sym = get(st, data->var); //lookup(name);

  if(sym->elements == 0)
    {
      fprintf(stderr, "Attempt to access elements of a non-array on line %d\n", tree->line);
      exit(-1);
    }
  if((sym->width / sym->elements) <= idx)//index->wires[0])
    {
      fprintf(stderr, "Index out of bounds on line %d\n", tree->line);
      abort();
      exit(-1);
    }
  ret->width = sym->elements;
  ret->elements = 0;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  for(i = 0; i < ret->width; i++)
    {
      ret->wires[i] = sym->wires[i + (idx * sym->elements)];
    }
  free(name);
  return ret;
}

void array_fetch_destroy(struct ast * tree)
{
  struct array_fetch_data* data = tree->data;
  free(data->var);
  free(data->index);
}

void * array_assign_action(struct ast * tree)
{
  struct array_fetch_data* data = tree->data;
  char * name = malloc(strlen(data->var) + 11);
  struct symbol * index;// = get(st, data->index); //lookup(data->index);
  uint64_t idx;
  
  struct symbol * sym = get(st, data->var);
  struct action_return * wires = tree->right->action(tree->right);
  if(data->varidx != 0) 
    {
      index = get(st, data->index); //lookup(data->index);
      if(index->isconst == 0)
        {
          fprintf(stderr, "Attempt to use non-constant array index on line %d\n", tree->line);
          exit(-1);
        }
//      fprintf(stderr, "%s %lu %lu %lu %lu\n", data->index, index->wires[0], sym->elements, sym->width, wires->width);
      idx = index->wires[0];
    }
  else
    {
      idx = data->idx;
    }
  
  if(sym->elements == 0)
    {
      fprintf(stderr, "Attempt to access elements of a non-array on line %d\n", tree->line);
      exit(-1);
    }
  if((sym->width / sym->elements) <= idx)//->wires[0])
    {
      fprintf(stderr, "Index out of bounds on line %d\n", tree->line);
      abort();
      exit(-1);
    }
  // Declares the *width* of the elements
  if(sym->elements != wires->width)
    {
      fprintf(stderr, "Operation size mismatch on line %d\n", tree->line);
    }

  // uint64_t * ww = malloc(sizeof(uint64_t)*(wires->width));
  uint64_t i;
  // Nope
  //  for(i = idx * sym->elements; i < idx * sym->elements + wires->width; i++)
  //    sym->wires[i] = wires->wires[i-idx * sym->elements];
  //assign_scope(st, name, ww, wires->width);
  //  free(wires);

  uint64_t * ww = malloc(sizeof(uint64_t)*sym->width);
  for(i = 0; i < idx*sym->elements; i++) ww[i] = sym->wires[i];
  for(i = idx*sym->elements; i < (idx+1)*sym->elements; i++)
    ww[i] = wires->wires[i-idx * sym->elements];
  for(i = (idx+1)*sym->elements; i < sym->width; i++) ww[i] = sym->wires[i];

  //  sym->dirty_idx[idx] = 255;

  uint8_t * dirty_idx = malloc(sizeof(uint8_t)*(sym->width / sym->elements));
  for(i = 0; i < (sym->width / sym->elements); i++)
    if(i == idx) dirty_idx[i] = 255;
    else dirty_idx[i] = sym->dirty_idx[i];
  struct symbol * sym_ = assign_scope(st, data->var, ww, sym->width, sym->elements, dirty_idx); //sym->dirty_idx);
  free(dirty_idx);
  sym_->dirty_idx[idx] = 255;
  delete_action_return(wires);
  free(name);
  return 0;
}

struct bit_fetch_data
{
  char * var;
  uint64_t start_wire, end_wire;
  uint64_t * wires;
  uint64_t width;
  void (*setup)(struct bit_fetch_data*);
  void * extra_data;
  uint64_t offset;
  uint8_t _setup;
};

void setup_default(struct bit_fetch_data* this)
{
}

void setup_var(struct bit_fetch_data*this)
{
  /* Get var, check for constness, setup wires */
  uint64_t i;
  struct symbol * index = get(st, this->extra_data); //lookup(this->extra_data);
  if(index->isconst == 0)
    {
      fprintf(stderr, "Attempt to use non-constant bit index.\n");
      exit(-1);
    }
  /* if(this->_setup != 0) */
  /*   free(this->wires); */
  this->start_wire = this->end_wire = index->wires[0];
  
  /* this->width = index->width; */
  /* this->wires = malloc(sizeof(uint64_t)*(this->width)); */
  /* for(i = 0; i < this->width; i++) */
  /*   { */
  /*     this->wires[i] = index->wires[i]; */
  /*   } */

  this->_setup = -1;
}

void * bit_fetch_action(struct ast * tree)
{
  struct bit_fetch_data * data = tree->data;
  data->setup(data);

  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct symbol * sym = get(st, data->var); //lookup(data->var);
  assert(sym!=0);
  data->setup(data);
  if(data->extra_data == 0)
    {
      /* ret[0] = sym->start_wire + data->start_wire; */
      /* ret[1] = sym->start_wire + data->end_wire; */
      ret->width = data->end_wire - data->start_wire + 1;
      ret->wires = malloc(sizeof(uint64_t)*(ret->width));
      uint64_t kkk;
      for(kkk = 0; kkk < ret->width; kkk++)
        {
          assert(sym->width > kkk+data->start_wire);
          ret->wires[kkk] = sym->wires[kkk+data->start_wire];
        }
    }
  else
    {
      /* Lookup variable */
      struct symbol * idx = get(st, data->extra_data);
      if(idx->isconst == 0)
        {
          fprintf(stderr, "Error: bit index must be a constant\n");
          exit(-1);
        }
      //      if((sym->end_wire - sym->start_wire) < idx->start_wire)
      if(sym->width < idx->wires[0])
        {
          fprintf(stderr, "Error: index %lu out of range\n", idx->wires[0]);
          exit(-1);
        }
      //      ret[0] = sym->start_wire + idx->start_wire;
      /* To support x{i:5} syntax*/
      //      ret[1] = sym->start_wire + idx->start_wire + data->offset;
      ret->width = data->offset+1;
      ret->wires = malloc(sizeof(uint64_t)*(ret->width));
      uint64_t i;
      for(i = 0; i < ret->width; i++)
        {
          ret->wires[i] = sym->wires[i + idx->wires[0]];
        }
    }
  ret->elements = 0;
  return ret;
}

void bit_fetch_destroy(struct ast * tree)
{
 struct bit_fetch_data * data = tree->data;
 free(data->var);
 if(data->extra_data != 0) {free(data->extra_data);
 free(data);
 }
}

void * bit_assign_action(struct ast * tree)
{
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct bit_fetch_data * data = tree->data;
  //  uint64_t * right_wires = tree->right->action(tree->right);
  struct action_return * right_wires = tree->right->action(tree->right);
  struct symbol * var = get(st, data->var); //lookup(data->var);
  uint64_t i;

  data->setup(data);

  if((data->extra_data) != 0)
    {
      // Making an assignment to a variable index
      struct symbol * idx = get(st, data->extra_data);
      if(idx->isconst == 0)
        {
          fprintf(stderr, "Error: bit index must be a constant\n");
          exit(-1);
        }
      //      if((sym->end_wire - sym->start_wire) < idx->start_wire)
      if(var->width < idx->wires[0])
        {
          fprintf(stderr, "Error: index %lu out of range\n", idx->wires[0]);
          exit(-1);
        }

      ret->width = var->width;
      ret->elements = var->elements;
      if(var->elements != 0)
        {
          ret->dirty_idx = malloc(sizeof(uint8_t)*(var->width / var->elements));
          for(i = 0; i < (var->width / var->elements); i++)
            if((idx->wires[0] <= i) && ((idx->wires[0] + data->offset) >= i))
              ret->dirty_idx[i] = 255;
            else
              ret->dirty_idx[i] = var->dirty_idx[i];
        }
      ret->wires = malloc(sizeof(uint64_t) * (ret->width));

      for(i = 0; i < idx->wires[0]; i++)
        {
          ret->wires[i] = var->wires[i];
        }
      for(i = idx->wires[0]; i < idx->wires[0]+data->offset+1; i++)
        {
          ret->wires[i] = right_wires->wires[i - idx->wires[0]];
        }
      for(i = idx->wires[0]+data->offset+1; i < var->width; i++)
        ret->wires[i] = var->wires[i];
    }
  else
    {
      //  if(data->end_wire - data->start_wire != right_wires[1] - right_wires[0])
      if(data->end_wire - data->start_wire+1 != right_wires->width)
        {
          fprintf(stderr, "Operation size mismatch in bit assignment on line %d\n", tree->line);
          abort();
          exit(-1);
        }

      ret->width = var->width;
      ret->elements = var->elements;

      if(var->elements != 0)
        {
          ret->dirty_idx = malloc(sizeof(uint8_t)*(var->width / var->elements));
          for(i = 0; i < (var->width / var->elements); i++)
            if((data->start_wire <= i) && (data->end_wire >= i))
              ret->dirty_idx[i] = 255;
            else
              ret->dirty_idx[i] = var->dirty_idx[i];
        }

      ret->wires = malloc(sizeof(uint64_t) * (ret->width));

      for(i = 0; i < data->start_wire; i++)
        {
          ret->wires[i] = var->wires[i];
        }
      for(i = data->start_wire; i <= data->end_wire; i++)
        ret->wires[i] = right_wires->wires[i - data->start_wire];
      for(i = data->end_wire+1; i < var->width; i++)
        ret->wires[i] = var->wires[i];
    }
  delete_action_return(right_wires);
  uint64_t * w = malloc(sizeof(uint64_t)*(ret->width));
  for(i = 0; i < ret->width; i++)
    w[i] = ret->wires[i];
  assign_scope(st, data->var, w, ret->width, var->elements, ret->dirty_idx);
  //  assign_scope(st,data->var, ret[0], ret[1]);

  return ret;
}

void shift_destroy(struct ast * tree)
{
  free(tree->data);
}

void * shift_right_action(struct ast * tree)
{
  uint64_t i;
  //  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  //  struct action_return * ret = malloc(sizeof(struct action_return));
  //  uint64_t * wires = tree->right->action(tree->right);
  struct action_return * wires = tree->right->action(tree->right);
  struct action_return * ret = copy_action_return(wires);
  uint64_t amount = *((uint64_t*)tree->data);

  //  if(amount > wires[1] - wires[0])
  if(amount >= wires->width)
    {
      fprintf(stderr, "Attempt to shift more bits than width of term.\n");
      exit(-1);
    }

  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;

  for(i = amount; i < ret->width; i++)
    {
      // Perform the shift
      // We are shifting to the right, so higher order bits will become lower order bits
      ret->wires[i - amount] = wires->wires[i];
    }
  for(i = ret->width - amount; i < ret->width; i++)
    ret->wires[i] = zed;

  delete_action_return(wires);
  ret->elements = 0;
  return ret;
}

void * shift_left_action(struct ast * tree)
{
  uint64_t i;
  /* uint64_t * ret = malloc(sizeof(uint64_t)*2); */
  /* uint64_t * wires = tree->right->action(tree->right); */
  struct action_return * wires = tree->right->action(tree->right);
  struct action_return * ret = copy_action_return(wires);
  uint64_t amount = *((uint64_t*)tree->data);

  //  if(amount > wires[1] - wires[0])
  if(amount >= wires->width)
    {
      fprintf(stderr, "Attempt to shift more bits than width of term: %d vs. %d.\n", amount, wires->width); //wires[1] - wires[0]);
      exit(-1);
    }
  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;

  for(i = amount; i < ret->width; i++)
    ret->wires[i] = wires->wires[i - amount];
  for(i = 0; i < amount; i++)
    ret->wires[i] = zed;
  ret->elements = 0;
  delete_action_return(wires);
  return ret;
}

struct funcall_data
{
  char * fname;
  struct varlist* vars;
};

void * funcall_action(struct ast * tree)
{
  struct scope_stack * tmp;
  /* 
   * Add arguments to scope, with the assigned values from the
   * enclosing scope
   */

  struct funcall_data * data = tree->data;
  struct varlist * v = data->vars;
  struct symbol * fsym = get(st,data->fname);
  struct varlist * a = fsym->args;
  struct symbol * retsym;
  //  uint64_t * ret;
  struct action_return * ret;

  st = push_scope_fun(st);
  while(v != 0 && a != 0)
    {
      struct symbol * sym = get(st->next,v->name);
      /* TODO: all assignments should be perform in-scope */
      /* More precisely, they should happen at the top-most scope
         where the variable is.  Need to create a new assignment
         operation that can deal with scoping, but NOT USE IT HERE --
         we want to mask variables with the same name in an enclosing
         scope. */
      //      assign_(st->variables, a->name, sym->start_wire, sym->end_wire);
      uint64_t * ww = malloc(sizeof(uint64_t)*sym->width);
      uint64_t kkk = 0;
      for(kkk = 0; kkk < sym->width; kkk++) ww[kkk]=sym->wires[kkk];
      assign_(st->variables, a->name, ww, sym->width, sym->elements, 0);
      v = v->next;
      a = a->next;
    }
  if((a != 0) || (v != 0))
    {
      fprintf(stderr, "Wrong number of arguments to function %s\n", data->fname);
      exit(-1);
    }
  /* 
   * Perform action for body
   */
  ret = fsym->body->action(fsym->body);
  //  free(ret);
  if(ret != 0) delete_action_return(ret);
  retsym = get(st,fsym->retvar);
  /* ret = malloc(sizeof(uint64_t)*2); */
  /* ret[0] = retsym->start_wire; */
  /* ret[1] = retsym->end_wire; */
  ret = malloc(sizeof(struct action_return));
  ret->elements = retsym->elements;
  if(retsym->elements != 0)
    {
      ret->dirty_idx = malloc(sizeof(uint8_t)*(retsym->width / retsym->elements));
      uint64_t i;
      for(i = 0; i < (retsym->width / retsym->elements); i++)
        ret->dirty_idx[i] = 0;
    }
  ret->width = retsym->width;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  uint64_t i;
  for(i = 0; i < ret->width; i++)
    ret->wires[i] = retsym->wires[i];
  /* TODO: finish this */

  tmp = st;
  st = st->next;

  free_symbols(tmp->variables);
  free(tmp);
  return ret;
}

void funcall_destroy(struct ast * tree)
{
  struct funcall_data * data = tree->data;
  free(data->fname);
  free_var_list(data->vars);
}

void * concat_action(struct ast * tree)
{
  /* uint64_t * ret = malloc(sizeof(uint64_t)*2); */
  /* uint64_t * left_wires = tree->left->action(tree->left); */
  /* uint64_t * right_wires = tree->right->action(tree->right); */
  struct action_return * ret = malloc(sizeof(struct action_return));
  struct action_return * left_wires = tree->left->action(tree->left);
  struct action_return * right_wires = tree->right->action(tree->right);

  uint64_t i;

  ret->width = left_wires->width + right_wires->width;
  ret->wires = malloc(sizeof(uint64_t)*(ret->width));
  for(i = 0; i < right_wires->width; i++)
    ret->wires[i] = right_wires->wires[i];
  for(i = 0; i < left_wires->width; i++)
    ret->wires[i+right_wires->width] = left_wires->wires[i];

  ret->elements = 0; // Concatenating two arrays creates a non-array

  delete_action_return(left_wires);
  delete_action_return(right_wires);
  return ret;
}

void * debug_action(struct ast * this)
{
  fprintf(stderr, "%s\n", this->data);
  return this->right->action(this->right);
}

void debug_destroy(struct ast * this)
{
  free(this->data);
}

struct array_maker_data
{
  char * name;
  uint64_t elements;
};

void * array_maker_action(struct ast * this)
{
  /* Declares an array */
  struct array_maker_data * data = this->data;
  struct symbol * sym = get(st, data->name);
  //  fprintf(stderr, "Making array %s %lu\n", data->name, data->elements);
  sym->elements = data->elements;
  sym->dirty_idx = malloc(sizeof(uint8_t)*(sym->width / sym->elements));
  uint64_t i;
  for(i = 0; i < (sym->width / sym->elements); i++)
    sym->dirty_idx[i] = 0;
  return 0;
}




/* Line 268 of yacc.c  */
#line 2226 "parser.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


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

/* Line 293 of yacc.c  */
#line 2155 "parser.y"

  int val;
  struct symbol * wire;
  struct ast * tree;
  struct varlist * vlist;
  char sym[LINE_MAX];
  char msg[LINE_MAX];



/* Line 293 of yacc.c  */
#line 2316 "parser.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 2328 "parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  37
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   231

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  48
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  15
/* YYNRULES -- Number of rules.  */
#define YYNRULES  57
/* YYNRULES -- Number of states.  */
#define YYNSTATES  169

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   298

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    46,     2,     2,     2,     2,     2,     2,
      44,    45,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    47,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     8,    11,    13,    25,    27,    31,
      41,    43,    46,    49,    52,    62,    74,    80,    88,    95,
      99,   103,   107,   110,   115,   117,   121,   128,   135,   142,
     151,   158,   167,   176,   182,   184,   188,   190,   194,   198,
     202,   204,   208,   210,   214,   218,   220,   222,   227,   232,
     239,   246,   251,   258,   263,   268,   272,   274
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      49,     0,    -1,    -1,    52,    11,    49,    -1,    50,    49,
      -1,    53,    -1,    28,    40,    44,    51,    45,    25,    53,
      41,    40,    11,    26,    -1,    40,    -1,    51,    37,    40,
      -1,    24,    40,    43,    14,    16,     3,    25,     3,    26,
      -1,    54,    -1,    53,    11,    -1,    53,    54,    -1,    56,
      11,    -1,    17,    40,    18,     3,    19,     3,    20,    53,
      21,    -1,    17,    40,    18,     3,    19,     3,    42,     3,
      20,    53,    21,    -1,    29,    55,    30,    53,    21,    -1,
      29,    55,    30,    53,    31,    53,    21,    -1,    27,    44,
      40,    37,     3,    45,    -1,    57,    36,    57,    -1,    57,
      33,    57,    -1,    57,    32,    57,    -1,    46,    55,    -1,
      46,    44,    55,    45,    -1,    57,    -1,    40,    43,    57,
      -1,    40,    22,     3,    23,    43,    57,    -1,    40,    22,
      40,    23,    43,    57,    -1,    40,    25,    40,    26,    43,
      57,    -1,    40,    25,    40,    47,     3,    26,    43,    57,
      -1,    40,    25,     3,    26,    43,    57,    -1,    40,    25,
       3,    37,     3,    26,    43,    57,    -1,    40,    25,     3,
      47,     3,    26,    43,    57,    -1,    15,    16,     3,    43,
      40,    -1,    58,    -1,    57,    16,    58,    -1,    59,    -1,
      58,     6,    59,    -1,    58,     9,    59,    -1,    58,     5,
      59,    -1,    60,    -1,    59,     7,    60,    -1,    61,    -1,
      61,    38,     3,    -1,    61,    39,     3,    -1,    62,    -1,
      40,    -1,    40,    22,     3,    23,    -1,    40,    22,    40,
      23,    -1,    40,    25,     3,    37,     3,    26,    -1,    40,
      25,     3,    47,     3,    26,    -1,    40,    25,     3,    26,
      -1,    40,    25,    40,    47,     3,    26,    -1,    40,    25,
      40,    26,    -1,    40,    44,    51,    45,    -1,    44,    57,
      45,    -1,     3,    -1,     3,    25,     3,    26,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,  2194,  2194,  2195,  2196,  2197,  2210,  2227,  2234,  2243,
    2281,  2282,  2283,  2296,  2297,  2316,  2335,  2345,  2374,  2392,
    2402,  2412,  2422,  2432,  2444,  2445,  2457,  2478,  2498,  2522,
    2550,  2570,  2591,  2613,  2630,  2631,  2644,  2645,  2656,  2668,
    2681,  2682,  2695,  2696,  2708,  2722,  2723,  2735,  2759,  2779,
    2804,  2826,  2847,  2874,  2898,  2931,  2934,  2946
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NUMBER", "SLASH", "MINUS", "XOR", "AND",
  "STAR", "PLUS", "EOL", "SEM", "DBGMSG", "MSGTXT", "INPUT", "OUTPUT",
  "DOT", "FOR", "FROM", "TO", "LOOP", "END", "OB", "CB", "DEFVAR", "OCURL",
  "CCURL", "ARRAY", "DEFUN", "IF", "THEN", "ELSE", "G", "L", "GE", "LE",
  "EQ", "COMMA", "SHIFTL", "SHIFTR", "VAR", "RET", "STEP", "EQUALS", "'('",
  "')'", "'!'", "':'", "$accept", "start_state", "funcdef", "varlist",
  "inputs", "calclist", "block", "cond", "lval", "cat", "exp", "factor",
  "shift", "term", "numeric", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,    40,    41,    33,    58
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    48,    49,    49,    49,    49,    50,    51,    51,    52,
      53,    53,    53,    54,    54,    54,    54,    54,    54,    55,
      55,    55,    55,    55,    56,    56,    56,    56,    56,    56,
      56,    56,    56,    56,    57,    57,    58,    58,    58,    58,
      59,    59,    60,    60,    60,    61,    61,    61,    61,    61,
      61,    61,    61,    61,    61,    61,    62,    62
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     3,     2,     1,    11,     1,     3,     9,
       1,     2,     2,     2,     9,    11,     5,     7,     6,     3,
       3,     3,     2,     4,     1,     3,     6,     6,     6,     8,
       6,     8,     8,     5,     1,     3,     1,     3,     3,     3,
       1,     3,     1,     3,     3,     1,     1,     4,     4,     6,
       6,     4,     6,     4,     4,     3,     1,     4
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,    56,     0,     0,     0,     0,     0,     0,    46,     0,
       0,     2,     0,     5,    10,     0,    24,    34,    36,    40,
      42,    45,     0,     0,     0,     0,     0,     0,    46,     0,
       0,     0,     0,     0,     0,     0,     0,     1,     4,     2,
      11,    12,    13,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,     0,    25,     7,
       0,    55,     3,    35,    39,    37,    38,    41,    43,    44,
      57,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    21,    20,    19,    47,    48,    51,     0,
       0,    53,     0,     0,    54,    33,     0,     0,     0,     0,
      47,    48,    51,     0,     0,    53,     0,    23,    16,     0,
       0,     0,     0,     0,     0,     0,     0,     8,     0,     0,
      18,     0,     0,     0,     0,     0,    26,    27,    30,    49,
      50,    28,    52,     0,     0,     0,     0,    49,    50,    52,
      17,     0,     0,     0,     0,     0,     0,     0,    31,    32,
      29,    14,     0,     9,     0,     0,     0,    15,     6
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,    10,    11,    70,    12,    13,    14,    30,    15,    16,
      17,    18,    19,    20,    21
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -59
static const yytype_int16 yypact[] =
{
      47,   -10,    17,   -22,   -15,   -17,    23,     1,    -9,    14,
      65,    47,    61,    66,   -59,    69,    74,    94,    98,   -59,
      59,   -59,    89,   106,   107,    84,    88,    87,    64,     2,
      99,   163,     3,     4,    14,    92,    -8,   -59,   -59,    47,
     -59,   -59,   -59,    14,    14,    14,    14,    14,   131,   132,
     113,    97,   141,   154,   122,    92,    11,    19,     1,   -59,
     161,    14,    14,    14,   147,   148,    76,   111,    74,   -59,
     -26,   -59,   -59,    94,    98,    98,    98,   -59,   -59,   -59,
     -59,   133,   158,   165,   179,   -16,   160,   164,   128,   116,
     146,    85,     9,    74,    74,    74,   143,   149,   150,   191,
     194,   155,   197,   162,   -59,   -59,   200,   201,   166,   181,
     -59,   -59,   -59,   204,   205,   -59,   206,   -59,   -59,   161,
      14,    14,    14,   184,   186,    14,   187,   -59,   -11,   189,
     -59,   161,   190,   192,   193,    67,    74,    74,    74,   172,
     174,    74,   177,   161,   218,   219,   109,   -59,   -59,   -59,
     -59,    14,    14,    14,   140,   203,   198,   185,    74,    74,
      74,   -59,   161,   -59,   215,   145,   202,   -59,   -59
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -59,    21,   -59,   175,   -59,   -58,   -13,   -19,   -59,    -6,
     188,    22,   180,   -59,   -59
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      41,    31,    92,    36,     1,     1,    64,    66,    43,   143,
      59,   103,     1,    32,    86,    22,    33,     1,    24,   104,
      40,   103,    88,    31,     2,    25,     3,    26,    68,   109,
     118,   144,    38,    23,    34,    35,     5,    71,     7,    90,
     119,    28,    28,    65,    67,     9,    58,    29,    29,     8,
       1,    87,    91,     9,    28,    93,    94,    95,     9,    89,
      72,   135,     2,    27,     3,    37,    74,    75,    76,     1,
       1,     4,    39,   146,     5,     6,     7,    40,    40,    41,
      42,     2,     2,     3,     3,   154,    56,     8,   150,    57,
      43,     9,    50,     5,     5,     7,     7,    48,    49,    44,
      45,    43,    98,    46,   165,    47,     8,     8,    35,    51,
       9,     9,     1,    99,   136,   137,   138,    61,    62,   141,
      40,    63,    41,   100,     2,    52,     3,    53,    54,    60,
      71,    55,    69,    41,    78,    79,     5,   101,     7,    80,
      81,    41,   115,     1,    82,   158,   159,   160,     1,     8,
     157,    40,    41,     9,   112,     2,    40,     3,   102,    84,
       2,   161,     3,   116,     1,   113,   167,     5,    83,     7,
      96,    97,     5,   105,     7,   114,     2,   106,     3,    43,
       8,   107,   108,   110,     9,     8,   120,   111,     5,     9,
       7,   117,   121,   122,   123,    61,    62,   124,   125,    63,
     126,     8,   127,   128,   129,     9,   131,   132,   133,   134,
     139,   130,   140,   142,   145,   151,   147,   152,   148,   149,
     153,   155,   156,   162,   163,   164,   166,    77,   168,     0,
      85,    73
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-59))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
      13,     7,    60,     9,     3,     3,     3,     3,    16,    20,
      29,    37,     3,    22,     3,    25,    25,     3,    40,    45,
      11,    37,     3,    29,    15,    40,    17,    44,    34,    45,
      21,    42,    11,    16,    43,    44,    27,    45,    29,    58,
      31,    40,    40,    40,    40,    44,    44,    46,    46,    40,
       3,    40,    58,    44,    40,    61,    62,    63,    44,    40,
      39,   119,    15,    40,    17,     0,    44,    45,    46,     3,
       3,    24,    11,   131,    27,    28,    29,    11,    11,    92,
      11,    15,    15,    17,    17,   143,    22,    40,    21,    25,
      16,    44,     3,    27,    27,    29,    29,    38,    39,     5,
       6,    16,    26,     9,   162,     7,    40,    40,    44,     3,
      44,    44,     3,    37,   120,   121,   122,    32,    33,   125,
      11,    36,   135,    47,    15,    18,    17,    43,    40,    30,
      45,    44,    40,   146,     3,     3,    27,    26,    29,    26,
      43,   154,    26,     3,     3,   151,   152,   153,     3,    40,
      41,    11,   165,    44,    26,    15,    11,    17,    47,    37,
      15,    21,    17,    47,     3,    37,    21,    27,    14,    29,
      23,    23,    27,    40,    29,    47,    15,    19,    17,    16,
      40,    16,     3,    23,    44,    40,    43,    23,    27,    44,
      29,    45,    43,    43,     3,    32,    33,     3,    43,    36,
       3,    40,    40,     3,     3,    44,    25,     3,     3,     3,
      26,    45,    26,    26,    25,    43,    26,    43,    26,    26,
      43,     3,     3,    20,    26,    40,    11,    47,    26,    -1,
      55,    43
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,    15,    17,    24,    27,    28,    29,    40,    44,
      49,    50,    52,    53,    54,    56,    57,    58,    59,    60,
      61,    62,    25,    16,    40,    40,    44,    40,    40,    46,
      55,    57,    22,    25,    43,    44,    57,     0,    49,    11,
      11,    54,    11,    16,     5,     6,     9,     7,    38,    39,
       3,     3,    18,    43,    40,    44,    22,    25,    44,    55,
      30,    32,    33,    36,     3,    40,     3,    40,    57,    40,
      51,    45,    49,    58,    59,    59,    59,    60,     3,     3,
      26,    43,     3,    14,    37,    51,     3,    40,     3,    40,
      55,    57,    53,    57,    57,    57,    23,    23,    26,    37,
      47,    26,    47,    37,    45,    40,    19,    16,     3,    45,
      23,    23,    26,    37,    47,    26,    47,    45,    21,    31,
      43,    43,    43,     3,     3,    43,     3,    40,     3,     3,
      45,    25,     3,     3,     3,    53,    57,    57,    57,    26,
      26,    57,    26,    20,    42,    25,    53,    26,    26,    26,
      21,    43,    43,    43,    53,     3,     3,    41,    57,    57,
      57,    21,    20,    26,    40,    53,    11,    21,    26
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 5:

/* Line 1806 of yacc.c  */
#line 2197 "parser.y"
    {
  struct ast * tree = (struct ast *) (yyvsp[(1) - (1)].tree);
  if(tree != 0)
    {
      tree->action(tree);
      delete_tree(tree);
    }
  /*  $$ = 0;*/
 }
    break;

  case 6:

/* Line 1806 of yacc.c  */
#line 2210 "parser.y"
    {
  /* We should store the tree returned by $7 and the list returned by
     $4.  When the function is called, we will fetch the variables and
     create a new scope with $4 assignments appropriately made, then
     go through the $7 tree creating the circuit.

     Will need to add something to the calclist nonterminal to allow
     calling functions.
   */
  struct ast * body = (yyvsp[(7) - (11)].tree);
  struct varlist * args = (yyvsp[(4) - (11)].vlist);
  char * retvar = (yyvsp[(9) - (11)].sym);
  funcassign_(symlist, (yyvsp[(2) - (11)].sym), body, args, retvar);
  (yyval.tree) = 0;
 }
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 2227 "parser.y"
    {
  struct varlist * vl = (struct varlist *)malloc(sizeof(struct varlist));
  vl->name = (char*)malloc(strlen((yyvsp[(1) - (1)].sym))+1);
  strcpy(vl->name, (yyvsp[(1) - (1)].sym));
  vl->next = 0;
  (yyval.vlist) = vl;
 }
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 2234 "parser.y"
    {
  struct varlist * vl = (struct varlist *)malloc(sizeof(struct varlist));
  vl->name = (char*)malloc(strlen((yyvsp[(3) - (3)].sym))+1);
  strcpy(vl->name, (yyvsp[(3) - (3)].sym));
  vl->next = (yyvsp[(1) - (3)].vlist);
  (yyval.vlist) = vl;
 }
    break;

  case 9:

/* Line 1806 of yacc.c  */
#line 2243 "parser.y"
    {
  uint64_t i;
  //  uint64_t s_wire = next_gate + 1;
  uint64_t * s_wire = malloc(sizeof(uint64_t) * ((yyvsp[(8) - (9)].val)));
  //  s_wire[i] = next_gate+1;
  for(i = 0; i < (yyvsp[(8) - (9)].val); i++)
    {
      next_gate++; 
      s_wire[i] = next_gate;
      //      printf("%d input //output$input.%s$%d\n", (int)next_gate, $6 == 0 ? "alice" : "bob", (int)i); 
      if(use_binary == 0)
        {
          /* fprintf(inputs_file, "%lu", next_gate); */
          /* fputs("", inputs_file); */
          /* fputs(, inputs_file); */
          //          fputc('$', inputs_file);
          fprintf(inputs_file, "%lu input //output$input.%s$%lu\n", next_gate, (yyvsp[(6) - (9)].val) == 0 ? "alice" : "bob", i);
          fflush(inputs_file);
          //          fputs("\n", inputs_file);
          //          fprintf(map_file, "%lu %lu\n", next_gate, next_gate);
          map_file_print(next_gate, next_gate);
        }
      else
        {
          uint8_t tag = ((yyvsp[(6) - (9)].val) == 1) ? 0x20 : 0x40;
          uint8_t xcnt = 1;
          fwrite(&tag, sizeof(uint8_t), 1, stdout);
          fflush(stdout);
          fwrite(&xcnt, sizeof(uint8_t),1,stdout);
          fflush(stdout);
        }
      inputs++;
    }
  assign((yyvsp[(2) - (9)].sym), s_wire, (yyvsp[(8) - (9)].val),0,0);//next_gate); 
 }
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 2281 "parser.y"
    {(yyval.tree) = (yyvsp[(1) - (1)].tree);}
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 2282 "parser.y"
    {(yyval.tree) = (yyvsp[(1) - (2)].tree);}
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 2283 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->data = 0;
  newnode->left = (yyvsp[(2) - (2)].tree);
  newnode->right = (yyvsp[(1) - (2)].tree);
  newnode->destroy = null_destroy;
  newnode->action = command_action;
  //  newnode->accept = command_accept;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 2296 "parser.y"
    {(yyval.tree) = (yyvsp[(1) - (2)].tree);}
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 2297 "parser.y"
    {
  uint64_t i;
  struct ast * tree = (struct ast *) (yyvsp[(8) - (9)].tree);
  struct loop_data * data = malloc(sizeof(struct loop_data));
  data->varname = malloc(strlen((yyvsp[(2) - (9)].sym))+1);
  strcpy(data->varname, (yyvsp[(2) - (9)].sym));
  data->start = (yyvsp[(4) - (9)].val);
  data->end = (yyvsp[(6) - (9)].val);
  data->step = 1;
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->action = loop_action;
  newnode->destroy = loop_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = data;
  newnode->left = tree;
  newnode->right = 0;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 2316 "parser.y"
    {
  uint64_t i;
  struct ast * tree = (struct ast *) (yyvsp[(10) - (11)].tree);
  struct loop_data * data = malloc(sizeof(struct loop_data));
  data->varname = malloc(strlen((yyvsp[(2) - (11)].sym))+1);
  strcpy(data->varname, (yyvsp[(2) - (11)].sym));
  data->start = (yyvsp[(4) - (11)].val);
  data->end = (yyvsp[(6) - (11)].val);
  data->step = (yyvsp[(8) - (11)].val);
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->action = loop_action;
  newnode->destroy = loop_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = data;
  newnode->left = tree;
  newnode->right = 0;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 2335 "parser.y"
    {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(2) - (5)].tree);
  newnode->right = (yyvsp[(4) - (5)].tree);
  newnode->action = if_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 2345 "parser.y"
    {
  struct ast * newnode1 = malloc(sizeof(struct ast));
  newnode1->left = (yyvsp[(2) - (7)].tree);
  newnode1->right = (yyvsp[(4) - (7)].tree);
  newnode1->action = if_action;
  //  newnode1->accept = accept_standard;
  newnode1->destroy = null_destroy;
  newnode1->line = yylineno;

  struct ast * newnode2 = malloc(sizeof(struct ast));
  newnode2->left = 0;
  newnode2->data = (yyvsp[(2) - (7)].tree);
  newnode2->right = (yyvsp[(6) - (7)].tree);
  newnode2->action = else_action;
  //  newnode2->accept = accept_standard;
  newnode2->destroy = null_destroy;
  newnode2->line = yylineno;

  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->data = 0;
  newnode->left = newnode2;
  newnode->right = newnode1;
  newnode->destroy = null_destroy;
  newnode->action = command_action;
  //  newnode->accept = command_accept;
  newnode->line = yylineno;

  (yyval.tree) = newnode;
 }
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 2374 "parser.y"
    {
  /*
   * Create $3 as an array with elements of width $5
   */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct array_maker_data * data = (struct array_maker_data*)malloc(sizeof(struct array_maker_data));
  data->name = (char*)malloc(strlen((yyvsp[(3) - (6)].sym))+1);
  strcpy(data->name, (yyvsp[(3) - (6)].sym));
  data->elements = (yyvsp[(5) - (6)].val);
  newnode->data= data;
  newnode->destroy = null_destroy;
  newnode->action = array_maker_action;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
  }
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 2392 "parser.y"
    {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = eq_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 2402 "parser.y"
    {
  struct ast* newnode = malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = l_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 2412 "parser.y"
    {
  struct ast* newnode = malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = g_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 2422 "parser.y"
    {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = (yyvsp[(2) - (2)].tree);
  newnode->action = not_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
  }
    break;

  case 23:

/* Line 1806 of yacc.c  */
#line 2432 "parser.y"
    {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = (yyvsp[(3) - (4)].tree);
  newnode->action = not_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
  }
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 2445 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = (struct ast *) (yyvsp[(3) - (3)].tree);
  newnode->action = variable_assignment_action;
  newnode->destroy = variable_assignment_destroy;
  //  newnode->accept = accept_assign;
  newnode->data = malloc(strlen((yyvsp[(1) - (3)].sym))+1);
  strcpy((char*)newnode->data, (yyvsp[(1) - (3)].sym));
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 2457 "parser.y"
    {
  char * name = malloc(strlen((yyvsp[(1) - (6)].sym)) + 11); /* Assuming that we will
                                            never need more than 10
                                            digits for an array
                                            index */
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  //sprintf(name, "$$$%s%d", $1, $3);
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->varidx = 0;
  data->var = malloc(strlen((yyvsp[(1) - (6)].sym))+1);
  strcpy(data->var, (yyvsp[(1) - (6)].sym));
  data->idx = (yyvsp[(3) - (6)].val);
  newnode->left = 0;
  newnode->right = (struct ast *) (yyvsp[(6) - (6)].tree);
  newnode->action = array_assign_action;//variable_assignment_action;
  newnode->destroy = array_fetch_destroy;
  //  newnode->accept = array_assign_accept;
  newnode->data = data;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 27:

/* Line 1806 of yacc.c  */
#line 2478 "parser.y"
    {
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen((yyvsp[(1) - (6)].sym)) + 1);
  strcpy(data->var, (yyvsp[(1) - (6)].sym));
  data->varidx = -1;
  data->index = malloc(strlen((yyvsp[(3) - (6)].sym)) + 1);
  strcpy(data->index, (yyvsp[(3) - (6)].sym));

  newnode->data = data;
  newnode->left = 0;
  newnode->right = (yyvsp[(6) - (6)].tree);
  newnode->action = array_assign_action;
  newnode->destroy = array_fetch_destroy;
  //  newnode->accept = array_assign_accept;
  newnode->line = yylineno;

  (yyval.tree) = newnode;

 }
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 2498 "parser.y"
    {
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  data->extra_data = malloc(strlen((yyvsp[(3) - (6)].sym))+1);
  strcpy((char*)data->extra_data, (yyvsp[(3) - (6)].sym));
  data->setup = setup_var;

  data->var = malloc(strlen((yyvsp[(1) - (6)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (6)].sym));

  newnode->left = 0;
  newnode->right = (yyvsp[(6) - (6)].tree);

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  (yyval.tree) = newnode;

 }
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 2522 "parser.y"
    {
  /* Similar to above */
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  data->extra_data = malloc(strlen((yyvsp[(3) - (8)].sym))+1);
  strcpy((char*)data->extra_data, (yyvsp[(3) - (8)].sym));
  data->setup = setup_var;

  data->var = malloc(strlen((yyvsp[(1) - (8)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (8)].sym));

  data->offset = (yyvsp[(5) - (8)].val);
  data->end_wire = 0;

  newnode->left = 0;
  newnode->right = (yyvsp[(8) - (8)].tree);

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;
  (yyval.tree) = newnode;

 }
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 2550 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup =0;
  data->extra_data = 0;
  data->setup = setup_default;
  data->var = malloc(strlen((yyvsp[(1) - (6)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (6)].sym));
  data->start_wire = data->end_wire = (yyvsp[(3) - (6)].val);
  newnode->left = 0;
  newnode->right = (yyvsp[(6) - (6)].tree);

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  (yyval.tree) = newnode;
 }
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 2570 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;
  data->setup = setup_default;
  data->var = malloc(strlen((yyvsp[(1) - (8)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (8)].sym));
  data->start_wire = (yyvsp[(3) - (8)].val);
  data->end_wire = (yyvsp[(5) - (8)].val);
  newnode->left = 0;
  newnode->right = (yyvsp[(8) - (8)].tree);

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  (yyval.tree) = newnode;
 }
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 2591 "parser.y"
    {
  /* x{5:10} gets x{5}, x{6}, x{7}, etc. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;

  data->setup = setup_default;

  newnode->left=0;
  newnode->right = (yyvsp[(8) - (8)].tree);
  newnode->data = data;
  data->var = malloc(strlen((yyvsp[(1) - (8)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (8)].sym));
  data->start_wire = (yyvsp[(3) - (8)].val);
  data->end_wire = (yyvsp[(3) - (8)].val) + (yyvsp[(5) - (8)].val);
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;
  (yyval.tree) = newnode;  
 }
    break;

  case 33:

/* Line 1806 of yacc.c  */
#line 2613 "parser.y"
    {
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct output_data * data = (struct output_data *) malloc(sizeof(struct output_data));
  newnode->left = 0;
  newnode->right = 0;
  data->var = malloc(strlen((yyvsp[(5) - (5)].sym))+1);
  strcpy(data->var, (yyvsp[(5) - (5)].sym));
  data->party = (yyvsp[(3) - (5)].val);
  newnode->data = data;
  newnode->action = output_action;
  newnode->destroy = output_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 2631 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->data = 0;
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->action = concat_action;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 2645 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = or_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 38:

/* Line 1806 of yacc.c  */
#line 2656 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = integer_add_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 39:

/* Line 1806 of yacc.c  */
#line 2668 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = integer_sub_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 2682 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = (yyvsp[(1) - (3)].tree);
  newnode->right = (yyvsp[(3) - (3)].tree);
  newnode->action = and_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 2696 "parser.y"
    {
  struct ast * newnode = (struct ast*)malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = (yyvsp[(1) - (3)].tree);
  newnode->data = malloc(sizeof(uint64_t));
  *((uint64_t*)newnode->data) = (yyvsp[(3) - (3)].val);
  newnode->action = shift_left_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = shift_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 44:

/* Line 1806 of yacc.c  */
#line 2708 "parser.y"
    {
  struct ast * newnode = (struct ast*)malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = (yyvsp[(1) - (3)].tree);
  newnode->data = malloc(sizeof(uint64_t));
  *((uint64_t*)newnode->data) = (yyvsp[(3) - (3)].val);
  newnode->action = shift_right_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = shift_destroy;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 2723 "parser.y"
    {
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  newnode->left = newnode->right = 0;
  newnode->data = malloc(strlen((yyvsp[(1) - (1)].sym))+1);
  strcpy((char*)newnode->data, (yyvsp[(1) - (1)].sym));

  newnode->action = variable_fetch_action;
  newnode->destroy = variable_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 2735 "parser.y"
    {
  /* Rename var to reflect array index */
  /* This puts arrays in a different scope from other variables :( */
  char * name = malloc(strlen((yyvsp[(1) - (4)].sym)) + 11); /* Assuming that we will
                                            never need more than 10
                                            digits for an array
                                            index */
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen((yyvsp[(1) - (4)].sym)) + 1);
  strcpy(data->var, (yyvsp[(1) - (4)].sym));

  data->varidx = 0;
  data->idx = (yyvsp[(3) - (4)].val);

  //sprintf(name, "$$$%s%d", $1, $3);
  newnode->left = newnode->right = 0;
  newnode->data = data;
  newnode->action = array_fetch_action;//variable_fetch_action;
  newnode->destroy = array_fetch_destroy;//variable_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 2759 "parser.y"
    {
  /* Must perform a semantic check -- $3 must be const */
  /* WISH: Create non-const index lookups */
  /* TODO: Need a new type of node */
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen((yyvsp[(1) - (4)].sym)) + 1);
  strcpy(data->var, (yyvsp[(1) - (4)].sym));
  data->varidx = -1;
  data->index = malloc(strlen((yyvsp[(3) - (4)].sym)) + 1);
  strcpy(data->index, (yyvsp[(3) - (4)].sym));

  newnode->data = data;
  newnode->left = newnode->right = 0;
  newnode->action = array_fetch_action;
  newnode->destroy = array_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 2779 "parser.y"
    {
  /* TODO: New node type here -- select individual wires */
  /* Need this to be assignable.  This will create some inefficient
     "copy" wires. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;

  data->setup = setup_default;

  newnode->left=0;
  newnode->right = 0;
  newnode->data = data;
  data->var = malloc(strlen((yyvsp[(1) - (6)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (6)].sym));
  data->start_wire = (yyvsp[(3) - (6)].val);
  data->end_wire = (yyvsp[(5) - (6)].val);

  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 2804 "parser.y"
    {
  /* x{5:10} gets x{5}, x{6}, x{7}, etc. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;

  data->setup = setup_default;

  newnode->left=0;
  newnode->right = 0;
  newnode->data = data;
  data->var = malloc(strlen((yyvsp[(1) - (6)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (6)].sym));
  data->start_wire = (yyvsp[(3) - (6)].val);
  data->end_wire = (yyvsp[(3) - (6)].val) + (yyvsp[(5) - (6)].val);
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;  
 }
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 2826 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  
  data->setup = setup_default;

  newnode->left=0;
  newnode->right = 0;
  newnode->data = data;
  data->var = malloc(strlen((yyvsp[(1) - (4)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (4)].sym));
  data->start_wire = (yyvsp[(3) - (4)].val);
  data->end_wire = (yyvsp[(3) - (4)].val);
  data->extra_data = 0;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 2847 "parser.y"
    {
  /* Similar to above */
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  data->extra_data = malloc(strlen((yyvsp[(3) - (6)].sym))+1);
  strcpy((char*)data->extra_data, (yyvsp[(3) - (6)].sym));
  data->setup = setup_var;

  data->var = malloc(strlen((yyvsp[(1) - (6)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (6)].sym));

  data->offset = (yyvsp[(5) - (6)].val);
  data->end_wire = 0;

  newnode->left = 0;
  newnode->right = 0;

  newnode->data = data;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;

 }
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 2874 "parser.y"
    {
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup =0;
  data->extra_data = malloc(strlen((yyvsp[(3) - (4)].sym))+1);
  strcpy((char*)data->extra_data, (yyvsp[(3) - (4)].sym));
  data->setup = setup_var;

  data->var = malloc(strlen((yyvsp[(1) - (4)].sym))+1);
  strcpy((char*)data->var, (yyvsp[(1) - (4)].sym));
  data->offset = 0;

  newnode->left = 0;
  newnode->right = 0;

  newnode->data = data;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;

 }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 2898 "parser.y"
    {
  /* Function invocation 
   *
   * This should create a new scope, then a tree in that scope, and
   * then return that.  $3 should be the data for the AST node.  The
   * function call should use the return variable name as the output
   * wires.
   *
   * Note that functions must have only one return statement, and it
   * must be at the end of the function.
   *
   * TODO: using a varlist here is probably a bad idea, since it
   * implies that a function argument cannot be a constant or an
   * expression.  Will need to change this to a "value list" instead.
   */
  /* TODO: fill this in */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct funcall_data * data = (struct funcall_data*) malloc(sizeof(struct funcall_data));
  data->fname = malloc(strlen((yyvsp[(1) - (4)].sym))+1);
  strcpy(data->fname, (yyvsp[(1) - (4)].sym));

  data->vars = (yyvsp[(3) - (4)].vlist);

  newnode->left = 0;
  newnode->right = 0;

  newnode->data = data;
  newnode->action = funcall_action;
  newnode->destroy = funcall_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
  }
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 2931 "parser.y"
    {(yyval.tree) = (yyvsp[(2) - (3)].tree);}
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 2934 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = 0;
  newnode->data = malloc(sizeof(uint64_t));
  *((uint64_t*)newnode->data) = (yyvsp[(1) - (1)].val);
  newnode->action = constant_action;
  newnode->destroy = constant_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 2946 "parser.y"
    {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = 0;
  newnode->data = malloc(sizeof(uint64_t)*2);
  ((uint64_t*)newnode->data)[0] = (yyvsp[(1) - (4)].val);
  ((uint64_t*)newnode->data)[1] = (yyvsp[(3) - (4)].val);
  newnode->action = constant_action_2;
  newnode->destroy = constant_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  (yyval.tree) = newnode;
 }
    break;



/* Line 1806 of yacc.c  */
#line 4701 "parser.tab.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 2067 of yacc.c  */
#line 2960 "parser.y"


extern float hash_factor;
extern int optimize;

extern FILE  * binary_file;

main(int argc, char ** argv)
{
  FILE * stats_file;

  init_maps();

  symlist = (struct node *) malloc(sizeof(struct node));
  symlist->next = 0;
  symlist->data = (struct symbol *) malloc(sizeof(struct symbol));
  symlist->data->name = "";
  if(argc < 2)
    {
      fprintf(stderr, "Please supply map file name as first argument\n");
      return -1;
    }
  sscanf(argv[7],"%hhd", &store_type);

  // Use memory mapped IO for the circuit files.
  sscanf(argv[8], "%hhd", &use_mmap_io);

  /*
   * The store type can be:
   * 0 -- use db
   * 1 -- use flat file
   */
  if(check_store_type(store_type) !=  0)
    {
      fprintf(stderr, "Invalid store type");
      return -1;
    }
  if(store_type == 2) store_type = 1; // Cannot use STL maps here, we only write to a file
  //  map_file = fopen(argv[1], "w");
  //  map_file = gdbm_open(argv[1], 512, GDBM_WRCREAT, S_IRWXU, 0);
  if(store_type == 0)
    {
      if(db_create(&map_file, NULL, 0) != 0)
        {
          fprintf(stderr, "Cannot open map file for writing: %s\n", db_strerror(errno));
          return -1;
        }

      if(map_file->open(map_file, NULL, argv[1], NULL, DB_BTREE, DB_CREATE, 0664) != 0)
        {
          fprintf(stderr, "%s\n", db_strerror(errno));
          return -1;
        }
    }
  else if(store_type >= 1)
    {
      store_file = fopen(argv[1], "wb+");
    }

  if(stdout == 0)
    {
      fprintf(stderr, "Cannot open output file for writing: %s\n", strerror(errno));
      return -1;
    }
  if(argc > 2)
    {
      sscanf(argv[2], "%f", &hash_factor);
    }
  if(argc > 3)
    {
      window = atoi(argv[3]);
    }
  if(argc > 4)
    {
      inputs_file = fopen(argv[4], "w");
    }
  if(argc > 5)
    {
      stats_file = fopen(argv[5], "w");
    }
  binary_file = fopen(argv[6], "wb+");
  use_binary = 0;
  cir_mapped = 0;

  yyparse();
  fprintf(stderr, "Done parsing, producing output.\n");
  if(optimize != 0)  remove_redundant_gates(circuit);
  print_gate_list(circuit);

  // gdbm_close(map_file);
  if(store_type == 0) map_file->close(map_file,0);
  else if(store_type == 1) fclose(store_file);
  else if(store_type == 3)
    {
      int fildes = fileno(store_file);
      msync(maps[fildes],map_size_inits[fildes],MS_SYNC);
      fclose(store_file);
    }

  if(use_mmap_io != 0)
    {
      msync(cir_buffer,cir_size_init, MS_SYNC);
    }
  fprintf(stderr, "Emitted %ld gates\n", total_emitted_gates);
  fprintf(stats_file, "%lu\n", total_emitted_gates);
  fclose(binary_file);

  fprintf(stderr, "XOR: %ld\nNon-XOR: %ld\n", xor, nxor);
  return 0;
}

yyerror(char * s)
{
  fprintf(stderr, "Error: %s on line %d\n", s, yylineno);
  return 0;
}

