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

extern int yylineno;
 extern int use_binary;
 extern uint64_t total_emitted_gates;

 FILE *  inputs_file;

enum {
  SCOPE_COND, SCOPE_FUN
};

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
  
  /*  uint64_t start_wire, end_wire;*/
  /* This will allow us to avoid identity wires etc., which we
     generate a lot of.  Unfortunately this means rewriting large
     sections of code...*/
  /* TODO: All sections need to switch to using an array of wires
     rather than (start,end) pairs.  The first element of this array
     should be the number of wires this symbol represents. */
  /*uint64_t * wires;*/
  uint64_t start_wire, end_wire;
  struct ast * body; /* used for function types */
  struct varlist * args;
  char * retvar;
  /* Width can be used to force constants to be a particular width */
  uint64_t isconst, width;
  int version;
  int type;
};

struct node
{
  struct symbol * data;
  struct node * next;
};

 struct node * symlist;

void free_symbols(struct node * nodes)
{
  if(nodes == 0) return;
  // We use "" as an identifier
  if(nodes->data->name != "")
    {
      free(nodes->data->name);
    }
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

struct symbol * constassign_(struct node * cur, const char * name, uint64_t start_wire, uint64_t end_wire, uint64_t width)
{
  assert(cur != 0);
  if(strcmp(cur->data->name, name) == 0)
    {
      /* if(cur->data->isconst != 0) */
      /*   { */
      /*     fprintf(stderr, "Attempt to assign to a constant variable.\n"); */
      /*     exit(-1); */
      /*   } */
      cur->data->start_wire = start_wire;
      cur->data->end_wire = end_wire;
      //cur->data->wires = wires;
      cur->data->isconst = -1;

      return cur->data;
    }
  else if(cur->next == 0)
    {
      struct node * newnode = malloc(sizeof(struct node));
      struct symbol * newdata = malloc(sizeof(struct symbol));
      newdata->name = malloc(strlen(name) + 1);
      newdata->type = SYM_VAR;
      strcpy(newdata->name, name);
      newdata->start_wire = start_wire;
      newdata->end_wire = end_wire;
      //cur->data->wires = wires;
      newnode->data = newdata;
      newnode->next = 0;
      newnode->data->isconst = -1;
      newnode->data->width = width;
      cur->next = newnode;
      return newdata;
    }
  else
    {
      return constassign_(cur->next, name, start_wire, end_wire, width);
    }
}

void constassign_scope(struct scope_stack * stack, const char * name, uint64_t start_wire, uint64_t end_wire, uint64_t width)
{
  if(stack==0) constassign_(symlist,name,start_wire,end_wire,width);
  else constassign_(stack->variables,name,start_wire,end_wire,width);
}

struct symbol * assign_(struct node * cur, const char * name, uint64_t start_wire, uint64_t end_wire)
{
  assert(cur != 0);
  if(strcmp(cur->data->name, name) == 0)
    {
      if(cur->data->isconst != 0)
        {
          fprintf(stderr, "Attempt to assign to a constant variable.\n");
          exit(-1);
        }
      cur->data->start_wire = start_wire;
      cur->data->end_wire = end_wire;

      return cur->data;
    }
  else if(cur->next == 0)
    {
      struct node * newnode = malloc(sizeof(struct node));
      struct symbol * newdata = malloc(sizeof(struct symbol));
      newdata->name = malloc(strlen(name) + 1);
      newdata->type = SYM_VAR;
      strcpy(newdata->name, name);
      newdata->start_wire = start_wire;
      newdata->end_wire = end_wire;
      newnode->data = newdata;
      newnode->next = 0;
      newnode->data->isconst = 0;
      cur->next = newnode;
      return newdata;
    }
  else
    {
      return assign_(cur->next, name, start_wire, end_wire);
    }
}

 void new(struct scope_stack * stack, char * name, int sw, int ew)
 {
   assign_(stack->variables, name, sw, ew);
 }

 void * push_scope(struct scope_stack * stack)
 {
   struct scope_stack * newstack = malloc(sizeof(struct scope_stack));
   newstack->variables = malloc(sizeof(struct node));
   newstack->variables->next = 0;
   newstack->variables->data = malloc(sizeof(struct symbol));
   newstack->variables->data->name = "";
   newstack->type = SCOPE_COND;
   newstack->next = stack;

   assert(newstack != 0);
   return newstack;
 }

void * push_scope_fun(struct scope_stack * stack)
{
   struct scope_stack * newstack = malloc(sizeof(struct scope_stack));
   newstack->variables = malloc(sizeof(struct node));
   newstack->variables->next = 0;
   newstack->variables->data = malloc(sizeof(struct symbol));
   newstack->variables->data->name = "";
   newstack->type = SCOPE_FUN;
   newstack->next = stack;
   return newstack;
}

struct symbol * get(struct scope_stack * stack, char * name)
{
  struct symbol * sym;
  if(stack == 0)
    {
      return lookup(name);
    }
  sym = lookup_(stack->variables, name, 0);
  if(sym == 0)
    {
      return get(stack->next, name);
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

struct symbol * assign(const char * name, uint64_t start_wire, uint64_t end_wire)
{
#ifdef DEBUG
  fprintf(stderr, "Assigning: %s\n", name);
#endif 
  return assign_(symlist, name, start_wire, end_wire);
}

/*
 * This is an assignment operation that is scope-aware; it changes the
 * value of a variable at the most recent enclosing scope or adds the
 * variable to the current scope.
 * 
 * TODO: This should be the assignment function everywhere.  We need
 * to modify the scoping for conditional assignments to be contained
 * with the the function call scope stack (which can then contain
 * further function call stacks):
 *
 * if->if->funcall->if->funcall->funcall->...
 * 
 * We should never go higher than an "if" block on the stack, because
 * those need to be mux'd or thrown away.
 *
 * We are going to allow functions to have side-effects.  This is
 * probably what people want, because they are used to languages where
 * this is the case.  Quirk: a variable declared in one function can
 * be modified by a function that is called from within that function.
 *
 * TODO: Is this an acceptable thing to do?  Should we have something
 * that is more C-like?
 * 
 * TODO: We will probably want something more C-like.  The problem is
 * that a global assignment may occur in a conditional statement, and
 * so we need to check all of those, but ignore conditionals that
 * refer to local assignments i.e. when we mask globals.
 *
 * Perhaps we will never mask globals?  An assignment is simply an
 * assignment, so we should never be able to mask a global (return an
 * error if an argument has the same name as a global).  All we should
 * do in a function is check the global variables, and if the variable
 * is not in the global scope, create a local.  If a variable is in
 * the global scope, then we need to find the most recent conditional
 * block and make the assignment there.
 *
 * Note: If a function creates a variable in its local scope, no other
 * functions will detect it!
 */
void * assign_scope(struct scope_stack * scope, const char * name, uint64_t start_wire, uint64_t end_wire)
{
  if(scope == 0)
    {
#ifdef DEBUG
      fprintf(stderr, "In global scope\n");
#endif
      return assign(name, start_wire, end_wire);
    }

  /* Go to the most recent if block, make assignment there. */
  if(scope->type == SCOPE_COND)
    {
      /* We are in a conditional scope*/
      return assign_(scope->variables, name, start_wire, end_wire);
    }

  /* TODO: Does this check need to be performed every time? */
  if(check_sym(name) == 0)
    {
#ifdef DEBUG
      fprintf(stderr, "Not in global scope\n");
#endif

      /* Variable does not exist in the global scope; it is safe to
         simply assign it a new value at the top level. */
      return assign_(scope->variables, name, start_wire, end_wire);
    }

  /* The variable exists in the global scope, but we might be in a
     conditional statement.  Descend the stack of scopes until we
     reach the global level or a conditional scope. */
  return assign_scope(scope->next,name,start_wire,end_wire);
}

uint64_t next_gate = -1;

struct ast;

struct visitor
{
  void * (*visit_assign)(struct visitor*, struct ast*);
  void * (*visit_other)(struct visitor*, struct ast *);
  void * (*visit_bit_assign)(struct visitor *, struct ast *);
  void * data;
};

struct ast
{
  struct ast * left, * right;
  void * (*action)(struct ast *);
  void * (*accept)(struct ast *, struct visitor *);
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

void * accept_standard(struct ast * this, struct visitor * v)
{
  return v->visit_other(v, this);
}

void * accept_assign(struct ast * this, struct visitor * v)
{
  return v->visit_assign(v, this);
}

void * accept_bit_assign(struct ast * this, struct visitor * v)
{
  return v->visit_bit_assign(v, this);
}

void * cond_visit_other(struct visitor * this, struct ast * obj)
{
  return obj->action(obj);
}

struct scope_stack * st = 0;

void * cond_visit_assign(struct visitor * this, struct ast * obj)
{
  /*
   * We need to create a mux here; if the condition is true, make the
   * assignment, otherwise do not make the assignment.
   *
   * Our "data" member should be the result wire from the conditional
   * statement and the current wires of the l-value.
   */
  uint64_t wire = *((uint64_t*)this->data);
  uint64_t wireinv;
  // The object's data will be the name of the variable being assigned.
  char * varname = obj->data;//data->varname;
  struct symbol * varsym = get(st,varname);
  uint64_t s_wire = varsym->start_wire;
  uint64_t e_wire = varsym->end_wire;
  uint64_t t1, t2;
  
  uint64_t * right_wires = obj->right->accept(obj->right, this);

  if(e_wire - s_wire != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in assignment of %s on line %d\n", varname, obj->line);
      exit(-1);
    }

  uint64_t i = 0;

  uint64_t * ret = right_wires;
  assign_(st->variables, varname, ret[0], ret[1]);

  return ret;
}

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
      
      constassign_scope(st, data->varname, i, i, 0);
      d = tree->left->action(tree->left);

      /* This was allocated dynamically */
      if(d != 0) free(d);
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
  uint64_t * wires = this->right->action(this->right);
  assign_scope(st, name, wires[0], wires[1]);
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
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  if(var->isconst == 0)
    {
      ret[0] = var->start_wire;
      ret[1] = var->end_wire;
    }
  else
    {
      /* Generate wires */
      uint64_t inputs[] = {1,1};
      ret[0] = next_gate+1;
      val = var->start_wire;
      for(i = 0; val >= 2; i++)
        {

          next_gate++;
          /* 11b = 3 */
          add_gate(next_gate, 2, (val%2 == 0) ? 0 : 0xF, inputs, 0,0);
          val /= 2;
        }
      next_gate++;
      add_gate(next_gate, 2, (val%2 == 0) ? 0 : 0xF, inputs, 0,0);

      if(var->width > 0)
        {
          uint64_t sz = next_gate - ret[0];
          for(i = 0; i < var->width - sz; i++)
            {
              next_gate++;
              add_gate(next_gate, 2, 0, inputs, 0,0);
            }
        }
      ret[1] = next_gate;
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
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * left_wires = this->left->action(this->left);
  uint64_t * right_wires = this->right->action(this->right);
  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in XOR %lu vs. %lu\n", left_wires[1] - left_wires[0], right_wires[1] - right_wires[0]);
      exit(-1);
    }
  ret[0] = next_gate+1;
  for(j = 0; j <=  left_wires[1] - left_wires[0]; j++)
    {
      uint64_t inputs[] = {left_wires[0]+j,right_wires[0]+j};
      next_gate++;

      add_gate(next_gate, 2, 0x6, inputs, 0,0);
    }
  free(left_wires);
  free(right_wires);
  ret[1] = next_gate;
  return ret;
}

void * and_action(struct ast * this)
{
  uint64_t i;
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * left_wires = this->left->action(this->left);
  uint64_t * right_wires = this->right->action(this->right);
  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in AND on line %d\n", this->line);
      exit(-1);
    }
  ret[0] = next_gate+1;
  for(i = 0; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i};
      next_gate++;     

      add_gate(next_gate, 2, 1, inputs, 0,0);
    }
  free(left_wires);
  free(right_wires);
  ret[1] = next_gate;
  //printf("returning %d %d\n", ret[0], ret[1]);
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
  for(i = 0; i <= s->end_wire - s->start_wire; i++)
    {
      uint64_t inputs[] = {s->start_wire+i};
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
      void * v = this->right->action(this->right);
      if(v != 0)
        free(v);
    }
  if(this->left != 0)
    {
      void * v = this->left->action(this->left);
      if(v != 0)
        free(v);
    }
  return 0;
}

void * command_accept(struct ast * this, struct visitor * vis)
{
  if(this->right != 0)
    {
      void * v = this->right->accept(this->right, vis);
      if(v != 0) free(v);
    }
  if(this->left != 0)
    {
      void * v = this->left->accept(this->left, vis);
      if(v != 0) free(v);
    }
  return 0;
}

void * integer_add_action(struct ast * this)
{
  uint64_t i, carry_s, carry_e, x, y, z;
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * left_wires = this->left->action(this->left);
  uint64_t * right_wires = this->right->action(this->right);
  uint64_t tmp[] = {left_wires[0], right_wires[0]};
  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in addition on line %d\n", this->line);
      exit(-1);
    }
  carry_s = next_gate+1;
  /* First adder has no carry in */
  next_gate++;


  add_gate(next_gate, 2, 1, tmp, 0, 0);

  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      uint64_t inputa[] = {left_wires[0]+i, right_wires[0]+i};
      uint64_t inputb[] = {left_wires[0]+i, next_gate};
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
      inputd[1] = left_wires[0]+i;
      add_gate(next_gate, 2, 6, inputd, 0, 0);
    }
  carry_e = next_gate+1;
  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }
  ret[0] = next_gate+1;
  next_gate++;

  add_gate(next_gate, 2, 6, tmp, 0, 0);


  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {carry_e + i - 1, carry_s + 4*(i - 1)};//{left_wires[0]+i, right_wires[0]+i, carry_s + i - 1};
      next_gate++;
      // Inputs:  a, b, carry_in
      // Outputs: sum, carry_out
      add_gate(next_gate, 2, 6, inputs, 0, 0);
      //      add_gate(next_gate, 3, "0 1 1 0 1 0 0 1", inputs, 0, 0);
    }
  free(left_wires);
  free(right_wires);
  ret[1] = next_gate;
  //printf("returning %d %d\n", ret[0], ret[1]);
  return ret;
}

void * integer_sub_action(struct ast * this)
{
  uint64_t i, carry_s, carry_e, x, y, z, a, b, one_wire, zero_wire;
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * left_wires = this->left->action(this->left);
  uint64_t * right_wires = this->right->action(this->right);
  uint64_t tmp[] = {left_wires[0], right_wires[0]};
  uint64_t dummy_inputs[] = {0,0};
  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in subtraction non line %d\n", this->line);
      exit(-1);
    }

  one_wire = next_gate+1;
  next_gate++;
  add_gate(next_gate, 2, 0xF, dummy_inputs, 0, 0);

  zero_wire = next_gate+1;
  next_gate++;
  add_gate(next_gate, 2, 0x0, dummy_inputs, 0, 0);

  /* Invert and add one */
  a = next_gate+1;
  for(i = 0; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {right_wires[0]+i, one_wire};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }

  carry_s = next_gate+1;
  /* First adder has no carry in */
  next_gate++;

  tmp[0] = a;
  tmp[1] = one_wire;
  add_gate(next_gate, 2, 1, tmp, 0, 0);

  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
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
  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
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


  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {carry_e + i - 1, carry_s + 4*(i - 1)};//{left_wires[0]+i, right_wires[0]+i, carry_s + i - 1};
      next_gate++;
      // Inputs:  a, b, carry_in
      // Outputs: sum, carry_out
      add_gate(next_gate, 2, 6, inputs, 0, 0);
      //      add_gate(next_gate, 3, "0 1 1 0 1 0 0 1", inputs, 0, 0);
    }


  carry_s = next_gate + 1;
  tmp[0] = left_wires[0];
  tmp[1] = b;
  next_gate++;
  add_gate(next_gate, 2, 1, tmp, 0, 0);

  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      uint64_t inputa[] = {left_wires[0]+i, b+i};
      uint64_t inputb[] = {left_wires[0]+i, next_gate};
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
      inputd[1] = left_wires[0]+i;
      add_gate(next_gate, 2, 6, inputd, 0, 0);
    }
  carry_e = next_gate+1;
  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {left_wires[0]+i, b+i};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }

  ret[0] = next_gate+1;
  next_gate++; 
  tmp[0] = left_wires[0];
  tmp[1] = b;
  add_gate(next_gate, 2, 6, tmp, 0, 0);

  /* next_gate++;  */
  /* tmp[0] = next_gate - 1; */
  /* tmp[1] = one_wire; */
  /* add_gate(next_gate, 2, 6, tmp, 0, 0); */

  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {carry_e + i - 1, carry_s + 4*(i - 1)};//{left_wires[0]+i, right_wires[0]+i, carry_s + i - 1};
      next_gate++;
      // Inputs:  a, b, carry_in
      // Outputs: sum, carry_out
      add_gate(next_gate, 2, 6, inputs, 0, 0);
      //      add_gate(next_gate, 3, "0 1 1 0 1 0 0 1", inputs, 0, 0);
    }
  free(left_wires);
  free(right_wires);
  ret[1] = next_gate;
  //printf("returning %d %d\n", ret[0], ret[1]);
  return ret;
}


void * constant_action(struct ast * tree)
{
  uint64_t i, bit;
  uint64_t num = *((uint64_t*)tree->data);
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t inputs[] = {1,1};
  ret[0] = next_gate+1;
  for(i = 0; num >= 2; i++)
    {

      next_gate++;

      add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);
      num /= 2;
    }
  next_gate++;

  add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);

  ret[1] = next_gate;
  return ret;
}

void * constant_action_2(struct ast * tree)
{
  uint64_t i, bit;
  uint64_t num = ((uint64_t*)tree->data)[0];
  uint64_t width = ((uint64_t*)tree->data)[1];
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t inputs[] = {1,1};

  ret[0] = next_gate+1;
  for(i = 0; num >= 2; i++)
    {
      next_gate++;

      add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);
      num /= 2;
    }
  next_gate++; 

  add_gate(next_gate, 2, ((num % 2) == 0) ? 0 : 0xF, inputs, 0, 0);

  bit = next_gate - ret[0]+1;

  for(i = 0; i < width - bit; i++)
    {
      next_gate++;

      add_gate(next_gate, 2, 0, inputs, 0, 0);
    }

  ret[1] = next_gate;
  return ret;
}

void constant_destroy(struct ast * tree)
{
  free(tree->data);
}

void * cond_visit_bit_assign(struct visitor *, struct ast *);

void mux(struct node * vars, struct scope_stack * s, uint64_t wire)
{
  if(vars == 0) return;
  else if(strcmp(vars->data->name,"") != 0)
    {
      //  fprintf(stderr, "muxing\n");
      struct symbol * sym = get(s, vars->data->name);
      uint64_t i,sw,ew,w,x,y,z;
      w = next_gate+1;
      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
        {
          uint64_t inputs[] = {sym->start_wire+i, vars->data->start_wire+i};
          next_gate++;
          add_gate(next_gate, 2, 6, inputs, 0,0);
        }
      x = next_gate;
      y = next_gate+1;
      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
        {
          uint64_t inputs[] = {w+i, wire};
          next_gate++;
          add_gate(next_gate, 2, 1, inputs, 0, 0);
        }
      z = next_gate;
      sw = next_gate+1;
      for(i = 0; i <= sym->end_wire - sym->start_wire; i++)
        {
          //      fprintf(stderr, "here %lld %lld\n", sym->start_wire, sym->end_wire);
          uint64_t inputs[] = {y+i,sym->start_wire+i}; //{sym->start_wire+i,vars->data->start_wire+i,wire};
          next_gate++;
          add_gate(next_gate, 2, 6, inputs, 0,0);
          //          add_gate(next_gate, 3, "0 1 0 1 0 0 1 1", inputs,0,0);
        }
      ew = next_gate;
      if(s != 0) assign_(s->variables, vars->data->name,sw,ew);
      else assign(vars->data->name,sw,ew);
      //    mux(vars->next, s, wire);
    }
  mux(vars->next,s,wire);
}


void * if_action(struct ast * tree)
{
  /*
   * The left subtree will be the conditional expression, the right
   * subtree will be the body.
   *
   * We must use the visitor to evaluate the body of the block.  What
   * will happen is that the command nodes will have an accept method
   * that applies the visitor to the left subtree, and then to the
   * right subtree.  Assignments in this section are rewritten as
   * multiplexers, and we can be assured that the assignments in the
   * else block will not be in conflict because the wire to the else
   * block is inverted.
   */
  struct scope_stack * tmp;
  void * right_ret;
  /* Possible bug -- conditional statements may not fetch the most
     recent version of variables?! */
  uint64_t * cond_wires = tree->left->action(tree->left);
  struct visitor * v = malloc(sizeof(struct visitor));
  v->data = malloc(sizeof(uint64_t));
  *((uint64_t*)v->data) = cond_wires[0]; /* The condition will only return one
                              wire */
  v->visit_assign = cond_visit_assign;
  v->visit_other = cond_visit_other;
  v->visit_bit_assign = cond_visit_bit_assign;

  st = push_scope(st);

  right_ret = tree->right->accept(tree->right, v);

  if(right_ret != 0) free(right_ret);

  //  fprintf(stderr, "exiting scope\n");

  tmp = st;
  st = st->next;

  mux(tmp->variables, st,cond_wires[0]);

  free_symbols(tmp->variables);
  free(tmp);

  free(v->data);
  free(v);

  free(cond_wires);
  return 0;
}

void * else_action(struct ast * tree)
{
  /*
   * The left subtree will be the conditional of the if block, the
   * right subtree will be the body of the else.
   */
  /* TODO: implement this */
  struct scope_stack * tmp;
  void * right_ret;
  uint64_t * cond_wires = ((struct ast *)tree->data)->action(tree->data);
  struct visitor * v = malloc(sizeof(struct visitor));
  uint64_t inputs[] = {1,1};
  v->data = malloc(sizeof(uint64_t));

  /* XOR with 1 to avoid 1-arity NOT gate */
  next_gate++;
  add_gate(next_gate, 2, 0xF, inputs, 0, 0);

  inputs[0] = cond_wires[0];
  inputs[1] = next_gate;

  next_gate++;
  //printf("%d gate arity 1 table [ 1 0 ] inputs [ %d ]\n", next_gate, cond_wires[0]);
  /*  putint(next_gate);
  fputs(" gate arity 1 table [ 1 0 ] inputs [ ",stdout);
  putint(cond_wires[0]);
  fputs(" ]\n",stdout);*/
  add_gate(next_gate, 2, 6, inputs, 0, 0);
  cond_wires[0] = next_gate;

  *((uint64_t*)v->data) = cond_wires[0]; /* The condition will only return one
                              wire */
  v->visit_assign = cond_visit_assign;
  v->visit_other = cond_visit_other;
  v->visit_bit_assign = cond_visit_bit_assign;

  st = push_scope(st);

  right_ret = tree->right->accept(tree->right, v);
  if(right_ret != 0) free(right_ret);

  tmp = st;
  st = st->next;

  mux(tmp->variables, st,cond_wires[0]);

  free_symbols(tmp->variables);
  free(tmp);

  free(v->data);
  free(v);

  free(cond_wires);
  return 0;
}

void * eq_action(struct ast * tree)
{
  uint64_t i;
  uint64_t * left_wires = tree->left->action(tree->left);
  uint64_t * right_wires = tree->right->action(tree->right);
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t tmp[] = {left_wires[0], right_wires[0]};

  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in comparison on line %d\n", tree->line);
      exit(-1);
    }

  next_gate++;

  add_gate(next_gate, 2, 9, tmp, 0, 0);

  /* Generate an AND tree */
  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      uint64_t inputs[] = {left_wires[0] + i, right_wires[0] + i};
      uint64_t inputs2[] = {next_gate+1, next_gate};
      next_gate++;

      add_gate(next_gate, 2, 9, inputs, 0, 0); 

      next_gate++;

      add_gate(next_gate, 2, 1, inputs2, 0, 0);
    } 
  ret[0] = ret[1] = next_gate;

  free(left_wires);
  free(right_wires);

  return ret;
}

void * l_action(struct ast* tree)
{
  uint64_t i;
  /*
   * Just to confuse every, I have reversed these...
   */
  uint64_t * right_wires = tree->left->action(tree->left);
  uint64_t * left_wires = tree->right->action(tree->right);
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t t1, t2,t3,t4;
  uint64_t tmp[] = {left_wires[0], right_wires[0]};

  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in comparison on line %d\n", tree->line);
      exit(-1);
    }


  t3 = next_gate+1;

  next_gate++;

  add_gate(next_gate, 2, 0xB /*"1011"*/, tmp, 0, 0);
  for(i = 1; i <= left_wires[1] - left_wires[0] - 1; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      uint64_t inputa[] = {left_wires[0]+i, right_wires[0]+i};
      uint64_t inputb[] = {next_gate, left_wires[0]+i};
      uint64_t inputc[] = {next_gate+1,next_gate+2};
      uint64_t inputd[] = {next_gate,next_gate+3};
      next_gate++;
      add_gate(next_gate, 2, 6, inputa, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputb, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 2 /*"0010"*/, inputc, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputd, 0, 0);
      //      add_gate(next_gate, 3, "0 0 1 0 1 0 1 1", inputs, 0, 0);
    }
  ret[0] = ret[1] =  next_gate;

  free(left_wires);
  free(right_wires);
  return ret;
}


void * g_action(struct ast* tree)
{
  uint64_t i;
  uint64_t * left_wires = tree->left->action(tree->left);
  uint64_t * right_wires = tree->right->action(tree->right);
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t t1, t2,t3,t4;
  uint64_t tmp[] = {left_wires[0], right_wires[0]};

  if(left_wires[1] - left_wires[0] != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in comparison on line %d\n", tree->line);
      exit(-1);
    }


  next_gate++;

  add_gate(next_gate, 2, 0xB /*"1011"*/, tmp, 0, 0);

  for(i = 1; i <= left_wires[1] - left_wires[0]; i++)
    {
      //      uint64_t inputs[] = {left_wires[0]+i, right_wires[0]+i, next_gate};
      uint64_t inputa[] = {left_wires[0]+i, right_wires[0]+i};
      uint64_t inputb[] = {next_gate, left_wires[0]+i};
      uint64_t inputc[] = {next_gate+1,next_gate+2};
      uint64_t inputd[] = {right_wires[0]+i, next_gate+3};//{next_gate,next_gate+3};
      next_gate++;
      add_gate(next_gate, 2, 6, inputa, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputb, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 2, inputc, 0, 0);
      next_gate++;
      add_gate(next_gate, 2, 6, inputd, 0, 0);

      //      add_gate(next_gate, 3, "0 0 1 0 1 0 1 1", inputs, 0, 0);
    }
  ret[0] = ret[1] =  next_gate;

  free(left_wires);
  free(right_wires);
  return ret;
}


void * not_action(struct ast * tree)
{
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * wires = tree->right->action(tree->right);
  uint64_t inputs[] = {1,1};

  next_gate++;
  add_gate(next_gate, 2, 0xF, inputs, 0, 0);

  inputs[0] = wires[0];
  inputs[1] = next_gate;


  next_gate++;

  add_gate(next_gate, 2, 6, wires, 0, 0);
  ret[0] = ret[1] = next_gate;
  free(wires);
  return ret;
}

struct array_fetch_data
{
  char * var, * index;
};

void * array_fetch_action(struct ast * tree)
{
  struct array_fetch_data * data = tree->data;
  char * name = malloc(strlen(data->var) + 11);
  struct symbol * index = get(st, data->index); //lookup(data->index);
  struct symbol * sym;
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  if(index->isconst == 0)
    {
      fprintf(stderr, "Attempt to use non-constant array index on line %d\n", tree->line);
      exit(-1);
    }
  sprintf(name, "$$$%s%d", data->var, (int)index->start_wire);
#ifdef DEBUG
  fprintf(stderr, "Fetching from scope: %lx\n", st);
#endif
  sym = get(st, name); //lookup(name);
  ret[0] = sym->start_wire;
  ret[1] = sym->end_wire;
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
  struct symbol * index = get(st, data->index); //lookup(data->index);
  uint64_t * wires = tree->right->action(tree->right);
  if(index->isconst == 0)
    {
      fprintf(stderr, "Attempt to use non-constant array index on line %d\n", tree->line);
      exit(-1);
    }
  sprintf(name, "$$$%s%d", data->var, (int)index->start_wire);
#ifdef DEBUG
  fprintf(stderr, "Assigning in scope: %lx\n", st);
#endif
  assign_scope(st,name, wires[0], wires[1]);
  free(wires);
  free(name);
  return 0;
}

void * array_assign_accept(struct ast * tree, struct visitor * v)
{
  struct array_fetch_data* data = tree->data;
  char * name = malloc(strlen(data->var) + 11);
  struct symbol * index = get(st, data->index); //lookup(data->index);
  /*  uint64_t * wires = tree->right->action(tree->right);*/
  uint64_t * ret;
  struct ast * newnode;

  if(index->isconst == 0)
    {
      fprintf(stderr, "Attempt to use non-constant array index on line %d\n", tree->line);
      exit(-1);
    }
  sprintf(name, "$$$%s%d", data->var, (int)index->start_wire);

  newnode = (struct ast *)malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = tree->right;
  newnode->destroy = variable_assignment_destroy;
  newnode->action = variable_assignment_action;
  newnode->accept = accept_assign;
  newnode->data = name;

  ret = v->visit_assign(v, newnode);
  newnode->destroy(newnode);
  free(newnode);
  return ret;
}

struct bit_fetch_data
{
  char * var;
  uint64_t start_wire, end_wire;
  void (*setup)(struct bit_fetch_data*);
  void * extra_data;
  uint64_t offset;
};

void setup_default(struct bit_fetch_data* this)
{
}

void setup_var(struct bit_fetch_data*this)
{
  /* Get var, check for constness, setup wires */
  struct symbol * index = get(st, this->extra_data); //lookup(this->extra_data);
  if(index->isconst == 0)
    {
      fprintf(stderr, "Attempt to use non-constant bit index.\n");
      exit(-1);
    }
  this->start_wire = this->end_wire = index->start_wire;
}

void * cond_visit_bit_assign(struct visitor * this, struct ast * obj)
{
  uint64_t wire = *((uint64_t*)this->data);
  uint64_t wireinv;
  // The object's data will be the name of the variable being assigned.
  struct bit_fetch_data * data = obj->data;//data->varname;
  struct symbol * varsym = get(st, data->var); //lookup(data->var);
  uint64_t inputs[] = {1,1};
  data->setup(data);

  uint64_t s_wire = data->start_wire;
  uint64_t e_wire = data->end_wire;
  uint64_t t1, t2, t3, t4;

  next_gate++;
  add_gate(next_gate, 2, 0xF, inputs, 0, 0);

  inputs[0] = wire;
  inputs[1] = next_gate;


  next_gate++;

  add_gate(next_gate, 2, 6, inputs, 0, 0);
  wireinv = next_gate;


  /*
   * Here we need to get the wires from the AST's right hand
   * expression, then add the mux gates.
   */
  uint64_t * right_wires = obj->right->accept(obj->right, this);

  if(e_wire - s_wire != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in bit assignment on line %d\n", obj->line);
      exit(-1);
    }

  uint64_t i = 0;

  uint64_t * ret = malloc(sizeof(uint64_t) * 2);

  t1 = next_gate+1;

  for(i = 0; i <= right_wires[1] - right_wires[0]; i++)
    {
      uint64_t inputs[] = {right_wires[0]+i, wire};
      next_gate++;
      // This needs to be fixed.

      add_gate(next_gate,2, 1, inputs, 0, 0);
    }

  t2 = next_gate+1;

  for(i = 0; i <= right_wires[1] - right_wires[0]; i++)
    {
      uint64_t inputs[] = {varsym->start_wire + s_wire +i, wireinv};
      next_gate++;
      // This needs to be fixed.

      add_gate(next_gate, 2, 1, inputs, 0, 0);
    }

  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;

  ret[0] = next_gate+1;

  for(i = varsym->start_wire; i < varsym->start_wire+s_wire; i++)
    {
      uint64_t inputs[] = {1,1};
      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }

  t3 = next_gate+1;

  for(i = 0; i <= right_wires[1] - right_wires[0]; i++)
    {
      uint64_t inputs[] = {t1+i, t2+i};
      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);
    }

  t4 = next_gate;

  free(right_wires);

  for(i = varsym->start_wire + e_wire + 1; i <= varsym->end_wire; i++)
    {
      uint64_t inputs[] = {1,1};
      
      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

      //      next_gate++;
      //      add_gate(next_gate, 1, 1, &i, 0, 0);
    }
  ret[1] = next_gate;

  assign_scope(st,data->var, ret[0], ret[1]);

  return ret;
}

void * bit_fetch_action(struct ast * tree)
{
  struct bit_fetch_data * data = tree->data;
  data->setup(data);

  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct symbol * sym = get(st, data->var); //lookup(data->var);
  data->setup(data);
  if(data->extra_data == 0)
    {
      ret[0] = sym->start_wire + data->start_wire;
      ret[1] = sym->start_wire + data->end_wire;
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
      if((sym->end_wire - sym->start_wire) < idx->start_wire)
        {
          fprintf(stderr, "Error: index %lu out of range\n", idx->start_wire);
          exit(-1);
        }
      ret[0] = sym->start_wire + idx->start_wire;
      /* To support x{i:5} syntax*/
      ret[1] = sym->start_wire + idx->start_wire + data->offset;
    }

  return ret;
}

void bit_fetch_destroy(struct ast * tree)
{
 struct bit_fetch_data * data = tree->data;
 free(data->var);
 if(data->extra_data != 0) free(data->extra_data);
 free(data);
}

void * bit_assign_action(struct ast * tree)
{
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  struct bit_fetch_data * data = tree->data;
  uint64_t * right_wires = tree->right->action(tree->right);
  struct symbol * var = get(st, data->var); //lookup(data->var);
  uint64_t i;

  data->setup(data);

  if(data->end_wire - data->start_wire != right_wires[1] - right_wires[0])
    {
      fprintf(stderr, "Operation size mismatch in bit assignment on line %d\n", tree->line);
      exit(-1);
    }
  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;

  ret[0] = next_gate+1;
  for(i = 0; i < data->start_wire; i++)
    {
      //      uint64_t inputs[] = {var->start_wire+i};
      //next_gate++;
      //add_gate(next_gate, 1, 1, inputs, 0, 0);
      uint64_t inputs[] = {1,1};
      inputs[0] = var->start_wire+i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }

  for(i = data->start_wire; i <= data->end_wire; i++)
    {
      //      uint64_t inputs[] = {right_wires[0]+i - data->start_wire};
      //next_gate++;
      //add_gate(next_gate, 1, 1, inputs, 0, 0);
      uint64_t inputs[] = {1,1};
      //      next_gate++;
      //add_gate(next_gate, 2, 0, inputs, 0, 0);

      inputs[0] = right_wires[0]+i - data->start_wire;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }

  for(i = var->start_wire + data->end_wire + 1; i <= var->end_wire; i++)
    {
      //      next_gate++;
      //      add_gate(next_gate, 1, 1, &i, 0, 0);
      uint64_t inputs[] = {1,1};
      //next_gate++;
      //add_gate(next_gate, 2, 0, inputs, 0, 0);

      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }
  ret[1] = next_gate;
  
  free(right_wires);
 
  assign_scope(st,data->var, ret[0], ret[1]);

  return ret;
}

void shift_destroy(struct ast * tree)
{
  free(tree->data);
}

void * shift_right_action(struct ast * tree)
{
  uint64_t i;
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * wires = tree->right->action(tree->right);
  uint64_t amount = *((uint64_t*)tree->data);

  if(amount > wires[1] - wires[0])
    {
      fprintf(stderr, "Attempt to shift more bits than width of term.\n");
      exit(-1);
    }

  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;

  ret[0] = next_gate+1;
  for(i = wires[0]+amount; i <= wires[1]; i++)
    {
      //      next_gate++;
      //add_gate(next_gate, 1, 1, &i, 0, 0);
      uint64_t inputs[] = {1,1};
      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }
  for(i = 0; i < amount; i++)
    {
      uint64_t inputs[] = {1,1};
      next_gate++;
      add_gate(next_gate, 2, 0, inputs, 0, 0);
    }
  ret[1] = next_gate;
  free(wires);
  return ret;
}

void * shift_left_action(struct ast * tree)
{
  uint64_t i;
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * wires = tree->right->action(tree->right);
  uint64_t amount = *((uint64_t*)tree->data);

  if(amount > wires[1] - wires[0])
    {
      fprintf(stderr, "Attempt to shift more bits than width of term: %d vs. %d.\n", amount, wires[1] - wires[0]);
      exit(-1);
    }
  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;

  ret[0] = next_gate+1;

  for(i = 0; i < amount; i++)
    {
      uint64_t input[] = {1,1};
      next_gate++;
      add_gate(next_gate, 2, 0, input, 0, 0);
    }

  for(i = wires[0]; i <= wires[1]-amount; i++)
    {
      //      next_gate++;
      //      add_gate(next_gate, 1, 1, &i, 0, 0);
      uint64_t inputs[] = {1,1};
      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }
  ret[1] = next_gate;
  free(wires);
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
  uint64_t * ret;

#ifdef DEBUG
  fprintf(stderr, "Calling function\n");
#endif

  st = push_scope_fun(st);
  while(v != 0 && a != 0)
    {
      struct symbol * sym = get(st,v->name);
      /* TODO: all assignments should be perform in-scope */
      /* More precisely, they should happen at the top-most scope
         where the variable is.  Need to create a new assignment
         operation that can deal with scoping, but NOT USE IT HERE --
         we want to mask variables with the same name in an enclosing
         scope. */
#ifdef DEBUG
      fprintf(stderr, "Binding var: %s\n",a->name);
#endif
      assign_(st->variables, a->name, sym->start_wire, sym->end_wire);
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
  if(ret != 0) free(ret);
  retsym = get(st,fsym->retvar);
  ret = malloc(sizeof(uint64_t)*2);
  ret[0] = retsym->start_wire;
  ret[1] = retsym->end_wire;
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
  uint64_t * ret = malloc(sizeof(uint64_t)*2);
  uint64_t * left_wires = tree->left->action(tree->left);
  uint64_t * right_wires = tree->right->action(tree->right);
  uint64_t i;
  uint64_t _inputs[] = {1,1};
  next_gate++;
  add_gate(next_gate, 2, 0, _inputs, 0, 0);
  uint64_t zed = next_gate;


  /*
   * Left side is more significant bits than right side, so we produce
   * the right side first.
   */
  ret[0] = next_gate+1;
  for(i = right_wires[0]; i <= right_wires[1]; i++)
    {
      //      next_gate++;
      //      add_gate(next_gate, 1, 1, &i, 0, 0);
      uint64_t inputs[] = {1,1};    
      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }
  for(i = left_wires[0]; i <= left_wires[1]; i++)
    {
      //      next_gate++;
      //      add_gate(next_gate, 1, 1, &i, 0, 0);
      uint64_t inputs[] = {1,1};
      
      inputs[0] = i;
      inputs[1] = zed;

      next_gate++;
      add_gate(next_gate, 2, 6, inputs, 0, 0);

    }
  ret[1] = next_gate;

  free(left_wires);
  free(right_wires);

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

#if 0
void karatsub(int a, uint64_t b)
{
  next_gate++;
  printf("%d gate artiy 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 1 1 1 0 0 1 1 1 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 1 1 0 1 1 0 0 1 0 0 1 0 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 0 1 0 1 0 1 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 1 0 1 0 1 0 1 0 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 0 1 0 0 1 0 0 1 1 0 1 1 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 0 1 1 1 0 0 1 1 1 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 0 0 0 1 1 1 0 0 0 0 0 0 1 1 1 0 0 1 1 1 0 0 0 1 1 0 0 0 1 1 0 0 1 1 1 0 0 1 1 0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 1 0 1 1 0 0 1 1 0 0 0 1 1 0 1 1 0 1 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 0 1 0 1 1 0 1 0 0 1 0 1 1 0 1 0 0 1 0 1 0 0 1 0 1 0 0 1 0 1 0 0 0 1 0 1 0 1 0 1 1 0 1 0 1 0 1 0 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 1 1 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 1 0 0 0 1 1 1 1 0 0 0 0 0 1 1 1 0 0 0 1 1 1 0 0 0 0 0 0 1 1 1 0 0 0 1 1 1 0 0 0 1 0 0 0 1 1 1 0 0 1 1 1 0 0 0 1 1 0 0 0 1 1 0 0 0 1 1 0 0 0 1 1 0 0 0 0 1 1 0 0 1 1 1 0 0 1 1 0 0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 1 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 1 0 0 0 0 0 1 1 1 1 1 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 0 1 1 1 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
  next_gate++;
  printf("%d gate arity 8 table [ 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 ] inputs [ %d %d %d %d %d %d %d %d ]\n", a, a+1, a+2, a+3, b, b+1, b+2, b+3);
}
#endif
int * generate_karatsuba(int * l, uint64_t * r)
{
  return 0;
}

/*
 * Perform integer multiplication using the Karatsuba method.
 */
void * integer_multiplication_action(struct ast * tree)
{
  return 0;
}

/*
 * Compute remainders modulo the right hand tree.
 */
void * integer_mod_reduction(struct ast * tree)
{
  return 0;
}

//struct ast * tree;

%}

%union {
  unsigned long long int val;
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
%token DEFVAR OCURL CCURL
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
  uint64_t s_wire = next_gate + 1;
  for(i = 0; i < $8; i++)
    {
      next_gate++; 
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
          fprintf(map_file, "%lu %lu\n", next_gate, next_gate);
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
  assign($2, s_wire, next_gate); 
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
  newnode->accept = command_accept;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| IF cond THEN calclist ELSE calclist END {
  struct ast * newnode1 = malloc(sizeof(struct ast));
  newnode1->left = $2;
  newnode1->right = $4;
  newnode1->action = if_action;
  newnode1->accept = accept_standard;
  newnode1->destroy = null_destroy;
  newnode1->line = yylineno;

  struct ast * newnode2 = malloc(sizeof(struct ast));
  newnode2->left = 0;
  newnode2->data = $2;
  newnode2->right = $6;
  newnode2->action = else_action;
  newnode2->accept = accept_standard;
  newnode2->destroy = null_destroy;
  newnode2->line = yylineno;

  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  newnode->data = 0;
  newnode->left = newnode2;
  newnode->right = newnode1;
  newnode->destroy = null_destroy;
  newnode->action = command_action;
  newnode->accept = command_accept;
  newnode->line = yylineno;

  $$ = newnode;
 }
;

cond: cat EQ cat {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = eq_action;
  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| cat L cat {
  struct ast* newnode = malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = l_action;
  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| cat G cat {
  struct ast* newnode = malloc(sizeof(struct ast));
  newnode->left = $1;
  newnode->right = $3;
  newnode->action = g_action;
  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
 }
| '!' cond {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = $2;
  newnode->action = not_action;
  newnode->accept = accept_standard;
  newnode->destroy = null_destroy;
  newnode->line = yylineno;
  $$ = newnode;
  }
| '!' '(' cond ')' {
  struct ast * newnode = malloc(sizeof(struct ast));
  newnode->left = 0;
  newnode->right = $3;
  newnode->action = not_action;
  newnode->accept = accept_standard;
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
  newnode->accept = accept_assign;
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
  sprintf(name, "$$$%s%d", $1, $3);
  newnode->left = 0;
  newnode->right = (struct ast *) $6;
  newnode->action = variable_assignment_action;
  newnode->destroy = variable_assignment_destroy;
  newnode->accept = accept_assign;
  newnode->data = name;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OB VAR CB EQUALS cat {
  struct ast * newnode = (struct ast*) malloc(sizeof(struct ast));
  struct array_fetch_data * data = (struct array_fetch_data*)malloc(sizeof(struct array_fetch_data));
  data->var = malloc(strlen($1) + 1);
  strcpy(data->var, $1);

  data->index = malloc(strlen($3) + 1);
  strcpy(data->index, $3);

  newnode->data = data;
  newnode->left = 0;
  newnode->right = $6;
  newnode->action = array_assign_action;
  newnode->destroy = array_fetch_destroy;
  newnode->accept = array_assign_accept;
  newnode->line = yylineno;

  $$ = newnode;

 }
| VAR OCURL VAR CCURL EQUALS cat {
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  $$ = newnode;

 }
| VAR OCURL NUMBER CCURL EQUALS cat {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_bit_assign;
  newnode->line = yylineno;

  $$ = newnode;
 }
| VAR OCURL NUMBER COMMA NUMBER CCURL EQUALS cat {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_bit_assign;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  sprintf(name, "$$$%s%d", $1, $3);
  newnode->left = newnode->right = 0;
  newnode->data = name;
  newnode->action = variable_fetch_action;
  newnode->destroy = variable_fetch_destroy;
  newnode->accept = accept_standard;
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

  data->index = malloc(strlen($3) + 1);
  strcpy(data->index, $3);

  newnode->data = data;
  newnode->left = newnode->right = 0;
  newnode->action = array_fetch_action;
  newnode->destroy = array_fetch_destroy;
  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OCURL NUMBER COMMA NUMBER CCURL {
  /* TODO: New node type here -- select individual wires */
  /* Need this to be assignable.  This will create some inefficient
     "copy" wires. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OCURL NUMBER ':' NUMBER CCURL {
  /* x{5:10} gets x{5}, x{6}, x{7}, etc. */
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;  
 }
| VAR OCURL NUMBER CCURL {
  struct ast * newnode = (struct ast *) malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data *) malloc(sizeof(struct bit_fetch_data));
  data->extra_data = 0;
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
  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
| VAR OCURL VAR ':' NUMBER CCURL {
  /* Similar to above */
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;

 }
| VAR OCURL VAR CCURL {
  /* similar to array syntax */
  struct ast * newnode = (struct ast *)malloc(sizeof(struct ast));
  struct bit_fetch_data * data = (struct bit_fetch_data*) malloc(sizeof(struct bit_fetch_data));
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
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
  newnode->accept = accept_standard;
  newnode->line = yylineno;
  $$ = newnode;
 }
;
%%

extern float hash_factor;
extern int optimize;

main(int argc, char ** argv)
{
  FILE * stats_file;
  symlist = (struct node *) malloc(sizeof(struct node));
  symlist->next = 0;
  symlist->data = (struct symbol *) malloc(sizeof(struct symbol));
  symlist->data->name = "";
  if(argc < 2)
    {
      fprintf(stderr, "Please supply map file name as first argument\n");
      return -1;
    }
  map_file = fopen(argv[1], "w");
  if(map_file == 0)
    {
      fprintf(stderr, "Cannot open map file for writing: %s\n", strerror(errno));
      return -1;
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
#if 0
  if(argc > 4)
    {
      uint64_t dummy = 0;
      fclose(stdout);
      stdout = fopen(argv[argc-1],"wb");

      use_binary = 1;
      fwrite(&dummy, sizeof(uint64_t),1,stdout);
      fflush(stdout);
      fwrite(&dummy, sizeof(uint32_t),1,stdout);
      fflush(stdout);
      fwrite(&dummy, sizeof(uint32_t),1,stdout);
      fflush(stdout);
      fwrite(&dummy, sizeof(uint32_t),1,stdout);
      fflush(stdout);
      fwrite(&dummy, sizeof(uint32_t),1,stdout);
      fflush(stdout);
      fwrite(&dummy, sizeof(uint8_t),1,stdout);
      fflush(stdout);
    }
  else
    {
      use_binary = 0;      
    }
#endif
  use_binary = 0;
  yyparse();
  fprintf(stderr, "Done parsing, producing output.\n");
  if(optimize != 0)  remove_redundant_gates(circuit);
  print_gate_list(circuit);

  fprintf(stats_file, "%lu\n", total_emitted_gates);
  return 0;
}

yyerror(char * s)
{
  fprintf(stderr, "Error: %s on line %d\n", s, yylineno);
}
