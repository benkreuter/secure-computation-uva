%{
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


%}

%union {
  int val;
  struct symbol * wire;
  struct ast * tree;
  struct varlist * vlist;
  char sym[LINE_MAX];
  char msg[LINE_MAX];
}

%token <val> NUMBER
%left PLUS STAR AND XOR MINUS SLASH
%token EOL SEM
%token DBGMSG 
%token <msg> MSGTXT
%token INPUT OUTPUT DOT
%token FOR FROM TO LOOP END
%token OB CB
%token DEFVAR OCURL CCURL ARRAY
%token DEFUN 
%token IF 
%nonassoc THEN 
%nonassoc ELSE // Need to make this a %nonassoc
%token G L GE LE EQ COMMA
%token SHIFTL SHIFTR
%token <sym> VAR
%token RET
%token STEP
%right EQUALS
%type <tree> calclist exp factor term lval block cond numeric shift cat funcdef
%type <vlist> varlist

/*
 * We need to create a visitor pattern to deal with if/then/else
 * blocks.  Basically, if we come to an assignment action in the
 * if/then/else body, we must create a mux for the assignment, and set
 * the variable to the output wires of the mux.
 */

%%
start_state:
| inputs SEM start_state
| funcdef start_state
| calclist {
  struct ast * tree = (struct ast *) $1;
  if(tree != 0)
    {
      tree->action(tree);
      delete_tree(tree);
    }
  /*  $$ = 0;*/
 }
/*calclist {*/
/* }*/
;

funcdef: DEFUN VAR '(' varlist ')' OCURL calclist RET VAR SEM CCURL {
  /* We should store the tree returned by $7 and the list returned by
     $4.  When the function is called, we will fetch the variables and
     create a new scope with $4 assignments appropriately made, then
     go through the $7 tree creating the circuit.

     Will need to add something to the calclist nonterminal to allow
     calling functions.
   */
  struct ast * body = $7;
  struct varlist * args = $4;
  char * retvar = $9;
  funcassign_(symlist, $2, body, args, retvar);
  $$ = 0;
 }
;

varlist: VAR {
  struct varlist * vl = (struct varlist *)malloc(sizeof(struct varlist));
  vl->name = (char*)malloc(strlen($1)+1);
  strcpy(vl->name, $1);
  vl->next = 0;
  $$ = vl;
 }
| varlist COMMA VAR {
  struct varlist * vl = (struct varlist *)malloc(sizeof(struct varlist));
  vl->name = (char*)malloc(strlen($3)+1);
  strcpy(vl->name, $3);
  vl->next = $1;
  $$ = vl;
 }
;

inputs: DEFVAR VAR EQUALS INPUT DOT NUMBER OCURL NUMBER CCURL {
  uint64_t i;
  //  uint64_t s_wire = next_gate + 1;
  uint64_t * s_wire = malloc(sizeof(uint64_t) * ($8));
  //  s_wire[i] = next_gate+1;
  for(i = 0; i < $8; i++)
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
          fprintf(inputs_file, "%lu input //output$input.%s$%lu\n", next_gate, $6 == 0 ? "alice" : "bob", i);
          fflush(inputs_file);
          //          fputs("\n", inputs_file);
          //          fprintf(map_file, "%lu %lu\n", next_gate, next_gate);
          map_file_print(next_gate, next_gate);
        }
      else
        {
          uint8_t tag = ($6 == 1) ? 0x20 : 0x40;
          uint8_t xcnt = 1;
          fwrite(&tag, sizeof(uint8_t), 1, stdout);
          fflush(stdout);
          fwrite(&xcnt, sizeof(uint8_t),1,stdout);
          fflush(stdout);
        }
      inputs++;
    }
  assign($2, s_wire, $8,0,0);//next_gate); 
 }
;


calclist: block {$$ = $1;}
| calclist SEM {$$ = $1;}
| calclist block {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->data = 0;
  newnode->left = $2;
  newnode->right = $1;
  newnode->destroy = null_destroy;
  newnode->action = command_action;
  //  newnode->accept = command_accept;
  newnode->line = yylineno;
  $$ = newnode;
 }
;

block: lval SEM {$$ = $1;}
| FOR VAR FROM NUMBER TO NUMBER LOOP calclist END {
  uint64_t i;
  struct ast * tree = (struct ast *) $8;
  struct loop_data * data = malloc(sizeof(struct loop_data));
  data->varname = malloc(strlen($2)+1);
  strcpy(data->varname, $2);
  data->start = $4;
  data->end = $6;
  data->step = 1;
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->action = loop_action;
  newnode->destroy = loop_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = data;
  newnode->left = tree;
  newnode->right = 0;
  newnode->line = yylineno;
  $$ = newnode;
 }
| FOR VAR FROM NUMBER TO NUMBER STEP NUMBER LOOP calclist END {
  uint64_t i;
  struct ast * tree = (struct ast *) $10;
  struct loop_data * data = malloc(sizeof(struct loop_data));
  data->varname = malloc(strlen($2)+1);
  strcpy(data->varname, $2);
  data->start = $4;
  data->end = $6;
  data->step = $8;
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->action = loop_action;
  newnode->destroy = loop_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = data;
  newnode->left = tree;
  newnode->right = 0;
  newnode->line = yylineno;
  $$ = newnode;
 }
| IF cond THEN calclist END {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = $2;
  newnode->right = $4;
  newnode->action = if_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| IF cond THEN calclist ELSE calclist END {
  struct ast * newnode1 = malloc(sizeof(struct ast));
  newnode1->left = $2;
  newnode1->right = $4;
  newnode1->action = if_action;
  //  newnode1->accept = accept_standard;
  newnode1->destroy = null_destroy;
  newnode1->line = yylineno;

  struct ast * newnode2 = malloc(sizeof(struct ast));
  newnode2->left = 0;
  newnode2->data = $2;
  newnode2->right = $6;
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

  $$ = newnode;
 }
| ARRAY '(' VAR COMMA NUMBER')' {
  /*
   * Create $3 as an array with elements of width $5
   */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct array_maker_data * data = (struct array_maker_data*)malloc(sizeof(struct array_maker_data));
  data->name = (char*)malloc(strlen($3)+1);
  strcpy(data->name, $3);
  data->elements = $5;
  newnode->data= data;
  newnode->destroy = null_destroy;
  newnode->action = array_maker_action;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
  }
;

cond: cat EQ cat {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = eq_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| cat L cat {
  struct ast* newnode = malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = l_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| cat G cat {
  struct ast* newnode = malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = g_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| '!' cond {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = $2;
  newnode->action = not_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
  }
| '!' '(' cond ')' {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = $3;
  newnode->action = not_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
  }
;

lval: cat
| VAR EQUALS cat {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = (struct ast *) $3;
  newnode->action = variable_assignment_action;
  newnode->destroy = variable_assignment_destroy;
  //  newnode->accept = accept_assign;
  newnode->data = malloc(strlen($1)+1);
  strcpy((char*)newnode->data, $1);
  newnode->line = yylineno;
  $$ = newnode;
 }      //{$$ = assign($1, $3, $3)->start_wire;}
| VAR OB NUMBER CB EQUALS cat {
  char * name = malloc(strlen($1) + 11); /* Assuming that we will
                                            never need more than 10
                                            digits for an array
                                            index */
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  //sprintf(name, "$$$%s%d", $1, $3);
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->varidx = 0;
  data->var = malloc(strlen($1)+1);
  strcpy(data->var, $1);
  data->idx = $3;
  newnode->left = 0;
  newnode->right = (struct ast *) $6;
  newnode->action = array_assign_action;//variable_assignment_action;
  newnode->destroy = array_fetch_destroy;
  //  newnode->accept = array_assign_accept;
  newnode->data = data;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OB VAR CB EQUALS cat {
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen($1) + 1);
  strcpy(data->var, $1);
  data->varidx = -1;
  data->index = malloc(strlen($3) + 1);
  strcpy(data->index, $3);

  newnode->data = data;
  newnode->left = 0;
  newnode->right = $6;
  newnode->action = array_assign_action;
  newnode->destroy = array_fetch_destroy;
  //  newnode->accept = array_assign_accept;
  newnode->line = yylineno;

  $$ = newnode;

 }
| VAR OCURL VAR CCURL EQUALS cat {
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  data->extra_data = malloc(strlen($3)+1);
  strcpy((char*)data->extra_data, $3);
  data->setup = setup_var;

  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);

  newnode->left = 0;
  newnode->right = $6;

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  $$ = newnode;

 }
| VAR OCURL VAR ':' NUMBER CCURL EQUALS cat{
  /* Similar to above */
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  data->extra_data = malloc(strlen($3)+1);
  strcpy((char*)data->extra_data, $3);
  data->setup = setup_var;

  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);

  data->offset = $5;
  data->end_wire = 0;

  newnode->left = 0;
  newnode->right = $8;

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;
  $$ = newnode;

 }

| VAR OCURL NUMBER CCURL EQUALS cat {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup =0;
  data->extra_data = 0;
  data->setup = setup_default;
  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->start_wire = data->end_wire = $3;
  newnode->left = 0;
  newnode->right = $6;

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  $$ = newnode;
 }
| VAR OCURL NUMBER COMMA NUMBER CCURL EQUALS cat {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;
  data->setup = setup_default;
  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->start_wire = $3;
  data->end_wire = $5;
  newnode->left = 0;
  newnode->right = $8;

  newnode->data = data;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  $$ = newnode;
 }
| VAR OCURL NUMBER ':' NUMBER CCURL EQUALS cat {
  /* x{5:10} gets x{5}, x{6}, x{7}, etc. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;

  data->setup = setup_default;

  newnode->left=0;
  newnode->right = $8;
  newnode->data = data;
  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->start_wire = $3;
  data->end_wire = $3 + $5;
  newnode->action = bit_assign_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;
  $$ = newnode;  
 }
| OUTPUT DOT NUMBER EQUALS VAR {
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct output_data * data = (struct output_data *) malloc(sizeof(struct output_data));
  newnode->left = 0;
  newnode->right = 0;
  data->var = malloc(strlen($5)+1);
  strcpy(data->var, $5);
  data->party = $3;
  newnode->data = data;
  newnode->action = output_action;
  newnode->destroy = output_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
;

cat: exp
| cat DOT exp {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->data = 0;
  newnode->left = $1;
  newnode->right = $3;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->action = concat_action;
  newnode->line = yylineno;
  $$ = newnode;
 }
;

exp: factor
| exp XOR factor {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = or_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  $$ = newnode;
 }
| exp PLUS factor {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = integer_add_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  $$ = newnode;
 }
     //{next_gate++; printf("%d gate arity 2 table [0 1 1 1] inputs [%d %d]\n", next_gate, $1, $3); $$ = next_gate;}
| exp MINUS factor {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = integer_sub_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  $$ = newnode;
 }
;

factor: shift
| factor AND shift  {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = and_action;
  newnode->destroy = null_destroy;
  //  newnode->accept = accept_standard;
  newnode->data = 0;
  newnode->line = yylineno;
  $$ = newnode;
 }
;

shift: term
| term SHIFTL NUMBER {
  struct ast * newnode = (struct ast*)malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = $1;
  newnode->data = malloc(sizeof(uint64_t));
  *((uint64_t*)newnode->data) = $3;
  newnode->action = shift_left_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = shift_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| term SHIFTR NUMBER {
  struct ast * newnode = (struct ast*)malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = $1;
  newnode->data = malloc(sizeof(uint64_t));
  *((uint64_t*)newnode->data) = $3;
  newnode->action = shift_right_action;
  //  newnode->accept = accept_standard;
  newnode->destroy = shift_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
;

term: numeric
| VAR {
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  newnode->left = newnode->right = 0;
  newnode->data = malloc(strlen($1)+1);
  strcpy((char*)newnode->data, $1);

  newnode->action = variable_fetch_action;
  newnode->destroy = variable_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OB NUMBER CB {
  /* Rename var to reflect array index */
  /* This puts arrays in a different scope from other variables :( */
  char * name = malloc(strlen($1) + 11); /* Assuming that we will
                                            never need more than 10
                                            digits for an array
                                            index */
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen($1) + 1);
  strcpy(data->var, $1);

  data->varidx = 0;
  data->idx = $3;

  //sprintf(name, "$$$%s%d", $1, $3);
  newnode->left = newnode->right = 0;
  newnode->data = data;
  newnode->action = array_fetch_action;//variable_fetch_action;
  newnode->destroy = array_fetch_destroy;//variable_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OB VAR CB {
  /* Must perform a semantic check -- $3 must be const */
  /* WISH: Create non-const index lookups */
  /* TODO: Need a new type of node */
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen($1) + 1);
  strcpy(data->var, $1);
  data->varidx = -1;
  data->index = malloc(strlen($3) + 1);
  strcpy(data->index, $3);

  newnode->data = data;
  newnode->left = newnode->right = 0;
  newnode->action = array_fetch_action;
  newnode->destroy = array_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OCURL NUMBER COMMA NUMBER CCURL {
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
  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->start_wire = $3;
  data->end_wire = $5;

  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OCURL NUMBER ':' NUMBER CCURL {
  /* x{5:10} gets x{5}, x{6}, x{7}, etc. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup=0;
  data->extra_data = 0;

  data->setup = setup_default;

  newnode->left=0;
  newnode->right = 0;
  newnode->data = data;
  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->start_wire = $3;
  data->end_wire = $3 + $5;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;  
 }
| VAR OCURL NUMBER CCURL {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  
  data->setup = setup_default;

  newnode->left=0;
  newnode->right = 0;
  newnode->data = data;
  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->start_wire = $3;
  data->end_wire = $3;
  data->extra_data = 0;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OCURL VAR ':' NUMBER CCURL {
  /* Similar to above */
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup = 0;
  data->extra_data = malloc(strlen($3)+1);
  strcpy((char*)data->extra_data, $3);
  data->setup = setup_var;

  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);

  data->offset = $5;
  data->end_wire = 0;

  newnode->left = 0;
  newnode->right = 0;

  newnode->data = data;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;

 }
| VAR OCURL VAR CCURL {
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
  data->_setup =0;
  data->extra_data = malloc(strlen($3)+1);
  strcpy((char*)data->extra_data, $3);
  data->setup = setup_var;

  data->var = malloc(strlen($1)+1);
  strcpy((char*)data->var, $1);
  data->offset = 0;

  newnode->left = 0;
  newnode->right = 0;

  newnode->data = data;
  newnode->action = bit_fetch_action;
  newnode->destroy = bit_fetch_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;

 }
| VAR '(' varlist ')' {
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
  data->fname = malloc(strlen($1)+1);
  strcpy(data->fname, $1);

  data->vars = $3;

  newnode->left = 0;
  newnode->right = 0;

  newnode->data = data;
  newnode->action = funcall_action;
  newnode->destroy = funcall_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
  }
| '(' cat ')' {$$ = $2;}
;

numeric: NUMBER {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = 0;
  newnode->data = malloc(sizeof(uint64_t));
  *((uint64_t*)newnode->data) = $1;
  newnode->action = constant_action;
  newnode->destroy = constant_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| NUMBER OCURL NUMBER CCURL {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = 0;
  newnode->data = malloc(sizeof(uint64_t)*2);
  ((uint64_t*)newnode->data)[0] = $1;
  ((uint64_t*)newnode->data)[1] = $3;
  newnode->action = constant_action_2;
  newnode->destroy = constant_destroy;
  //  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
;
%%

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
