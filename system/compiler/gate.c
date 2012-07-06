#define _GNU_SOURCE
#include <search.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "gate.h"

int offset = 0;

uint64_t total_emitted_gates = 0;
uint64_t inputs = 0;

FILE * map_file;
int line = 0;

int use_binary = 0;
int optimize = 1;

void putint(uint64_t x)
{
  if(x < 10)
    {
      putchar(x+'0');
    }
  else
    {
      putint(x / 10);
      putchar((x%10) + '0');
    }
}

void emit_gate(char arity, uint64_t * inputs, char t, uint64_t id, char output, char party)
{
  int i;
  char tab;
  for(i = 0; i < arity; i++)
    {
      putint(inputs[i]);
      putchar(',');
    }
  tab = t;
  for(i = 0; i < ((arity == 2) ? 4 : 2); i++)
    {
      if((tab & ((arity == 2) ? 8 : 2)) != 0)
        printf("1");
      else
        printf("0");

      tab = tab << 1;
    }
  printf(" ");
  putint(id);
  if(output != 0) {
    printf(" 1");
  }
  else printf(" 0");
  printf(" %d\n", party);

  total_emitted_gates++;

}

void print_gate(struct gate * g)
{
#ifdef USE_OLD_PRINT
  int i;
  char tab;
  putint(g->id);
  if(g->output != 0)
    fputs(" output",stdout);
  fputs(" gate arity ",stdout);
  putint(g->arity);
  fputs(" table [", stdout);
  //fputc(g->truth_table, stdout);
  tab = g->truth_table;
  for(i = 0; i < ((g->arity == 2) ? 4 : 2); i++)
    {
      if((tab & ((g->arity == 2) ? 8 : 2)) != 0)
        printf(" 1");
      else
        printf(" 0");

      tab = tab << 1;
    }
  fputs(" ] inputs [ ", stdout);
  for(i = 0; i < g->arity; i++)
    {
      putint(g->inputs[i]);
      putchar(' ');
    }
  fputs("]",stdout);
  /*  if(g->comment != 0)
    {
      putchar(' ');
      fputs(g->comment, stdout);
      }*/
  fputs("\n",stdout);
  //  map_file_print(g->id, line + inputs);
  line++;
#endif
  int i;
  char tab;
  for(i = 0; i < g->arity; i++)
    {
      putint(g->inputs[i]);
      putchar(',');
    }
  tab = g->truth_table;
  for(i = 0; i < ((g->arity == 2) ? 4 : 2); i++)
    {
      if((tab & ((g->arity == 2) ? 8 : 2)) != 0)
        printf("1");
      else
        printf("0");

      tab = tab << 1;
    }
  printf(" ");
  putint(g->id);
  if(g->output != 0) printf(" 1");
  else printf(" 0");
  printf(" %d\n", g->party);

  // map_file_print(g->id, line + inputs);
  line++;
  total_emitted_gates++;
}

enum { ETC = 0, GEN_INP = 1, EVL_INP = 2, GEN_OUT = 3, EVL_OUT = 4};

void binary_gate(struct gate * g)
{
#if 0
  uint8_t idx_size = 0;
  uint8_t tag = 0;
  int truth_table_size = 0;
  uint64_t gate_cnt = 0;
  int ix;
  if(g->arity == 3) truth_table_size = 8;
  else if(g->arity == 2) truth_table_size = 4;
  else truth_table_size = 2;
  int i;
  /* Size of IDs of other gates */
  if(g->id <= UINT16_MAX)
    {
      idx_size = sizeof(uint16_t);
    }
  else if(g->id <= UINT32_MAX)
    {
      idx_size = sizeof(uint32_t);
    }
  else
    {
      idx_size = sizeof(uint64_t);
    }
  ix = 0;/*
  for(i = 0; i <= strlen(g->truth_table); i++)
    {
      if((g->truth_table[i] == '0') || (g->truth_table[i] == '1'))
        {
          tag |= ((g->truth_table[i] == '0') ? 0x00 : 0x01) << ix;
          ix++;
        }
        }*/
  tag |= g->truth_table;
  /* Only the first four bits of the tag can be used -- only arity 1 or arity 2 gates are allowed */
  tag = tag & 0x0f;

  tag |= (g->output == 0) ? 0x00 : ((g->party == 1) ? 0x60 : 0x80);
  tag |= (g->arity == 1) ? 0x00 : 0x10;
  fwrite(&tag, sizeof(uint8_t), 1, stdout);
  fflush(stdout);
  for(i = 0; i < g->arity; i++)
    {
      if(g->id <= UINT16_MAX)
        {
          uint16_t idx = g->inputs[i];
          fwrite(&idx, sizeof(uint16_t), 1, stdout);
        }

      else if(g->id <= UINT32_MAX)
        {
          uint32_t idx = g->inputs[i];
          fwrite(&idx, sizeof(uint32_t), 1, stdout);
        }

      else if(g->id <= UINT64_MAX)
        {
          uint64_t idx = g->inputs[i];
          fwrite(&idx, sizeof(uint64_t), 1, stdout);
        }
      fflush(stdout);
    }
  fwrite(&gate_cnt, sizeof(uint8_t), 1, stdout);
  fflush(stdout);
#endif
}

void print_gate_list(struct gate_list * list)
{
  struct gate_list * cur = list;
  while(cur != 0)
    {
      if(use_binary==0) print_gate(cur->data);
      else binary_gate(cur->data);
      cur = cur->next;
    }
}

void free_gate_list(struct gate_list * list)
{
  struct gate_list * cur = list;
  while(cur != 0)
    {
      struct gate_list * tmp = cur;
      cur = cur->next;
      //      if(tmp->data->comment != 0) free(tmp->data->comment);
      free(tmp->data->inputs);
      //      free(tmp->data->truth_table);
      free(tmp->data);
      free(tmp);

    }
}

struct gate_list * circuit, * last;
uint64_t * idmap = 0;
uint64_t cursize = 0;
uint64_t window = 100;
uint64_t zero_gate = 0, one_gate = 0;

void map_file_print(uint64_t old, uint64_t new)
{
  fprintf(map_file, "%lu %lu\n", old, new);
}

/* TODO: If a gate has been detected before, do not add it to the list
   -- we are consuming far too much memory! */

void add_gate(uint64_t id, uint64_t arity, char truth_table, uint64_t * inputs, int output, char * comment)
{
  int i;
  struct gate * newgate = 0; 
  struct gate_list * newnode = 0; 
  assert(inputs[0] < id);

  if(arity > 1)  assert(inputs[1] < id);
  if((output == 0) && (optimize != 0))
    {
      if(truth_table == 0xF)
        {
          /* constant gate */
          if(one_gate != 0)
            {
              map_file_print(id, one_gate);
              return;
            }
          else
            {
              //fprintf(stderr, "One: %lu\n", id);
              one_gate = id;
            }
        }
      else if(truth_table == 0)
        {
          /* constant gate */
          if(zero_gate != 0)
            {
              map_file_print(id, zero_gate);
              return;
            }
          else
            {
              //              fprintf(stderr, "Zero: %llu\n", id);
              zero_gate = id;
            }
        }
    }

  newgate = malloc(sizeof(struct gate));
  newnode = malloc(sizeof(struct gate_list));
  newgate->truth_table = truth_table; //malloc(strlen(truth_table)+1);
  newgate->inputs = malloc(sizeof(uint64_t)*arity);
  newgate->party = 0;
  if((use_binary == 0) && (comment != 0) )
    {
      /*      newgate->comment = malloc(strlen(comment)+1);
              strcpy(newgate->comment, comment);*/
      newgate->party = (uint64_t)comment;
    }
  else
    {
      newgate->party = (uint64_t)comment;
      //      newgate->comment = 0;
    }
  newgate->id = id;
  newgate->arity = arity;
  newgate->truth_table = truth_table;//  strcpy(newgate->truth_table, truth_table);
  for(i = 0; i < arity; i++)
    {
      newgate->inputs[i] = inputs[i];
    }
  newgate->output = output;

  newnode->next = 0;
  newnode->data = newgate;
  if(last != 0)
    {
      last->next = newnode;
      last = newnode;
    }
  else
    {
      circuit = last = newnode;
    }
  cursize++;
  //  absize++;
  if(cursize >= window)
    {
      //fprintf(stderr, "Optimizing section\n");
      if(optimize != 0) remove_redundant_gates(circuit);
      //      remove_identity_gates(circuit);
      print_gate_list(circuit);
      free_gate_list(circuit);
      circuit = (void*)0;
      cursize = 0;
      last = (void*)0;
    }
}

/*
 * Remove all identity gates
 * Running time: O(n^2)
 */
#if 0
void remove_identity_gates(struct gate_list * list)
{
  struct gate_list * cur = list;
  struct gate_list * lcur = cur;
  while(cur != 0)
    {
      if((cur != lcur) && (cur->data->arity == 1))
        {
          if((strcmp(cur->data->truth_table, "0 1") == 0) && (cur->data->output == 0))
            {
              /* 
               * This is an identity gate.
               */
              struct gate_list * tmp = cur->next;
              while(tmp != 0)
                {
                  /*
                   * Check to see if this identity gate is referenced;
                   * if it is, replace with this gate's input wire.
                   */
                  int i;
                  for(i = 0; i < tmp->data->arity; i++)
                    {
                      if(tmp->data->inputs[i] == cur->data->id)
                        {
                          tmp->data->inputs[i] = cur->data->inputs[0];
                        }
                    }

                  tmp = tmp->next;
                }
              map_file_print(cur->data->id, cur->data->inputs[0]);
              lcur->next = cur->next;
              //              if(cur->data->comment != 0) free(cur->data->comment);
              free(cur->data->inputs);
              free(cur->data->truth_table);
              free(cur->data);
              free(cur);
              cur = lcur;
            }
        }
      lcur = cur;
      cur = cur->next;
    }
}
#endif
float hash_factor = 2.5;
char **keys = 0, **mkeys;

/*
 * Remove redundant gates -- same truth table and inputs
 * Running time: O(n^2)
 *
 * We'll make this better right now, using a hash table.
 */
void remove_redundant_gates(struct gate_list * list)
{
  uint64_t id = 0, count = 0, mcount = 0, j = 0;
  int i;
  struct gate_list * cur = list;

  struct hsearch_data htab, mtab;

  ENTRY ent, *ret;
  if(list == 0) return;
  memset(&htab, 0, sizeof(struct hsearch_data));
  memset(&mtab, 0, sizeof(struct hsearch_data));

  /* To function efficiently, hash tables should be twice the size of the circuit */
  hcreate_r(hash_factor*cursize, &htab);
  hcreate_r(hash_factor*cursize, &mtab);

  if(keys == 0)
    {
      /* We need to keep track of each of the hash key buffers that we allocate */
      keys = malloc(sizeof(char*)*window);
      mkeys = malloc(sizeof(char*)*window);
    }

  id = cur->data->id;
  while(cur->next != 0) //  while(cur != 0)
    {
      struct gate_list * tmp = cur->next;
      struct gate_list * ltmp = cur;
      char * gate_desc, * buf;
      uint64_t inputs[2];
      gate_desc = malloc(32*sizeof(char));
      buf = malloc(22 * sizeof(char));

      /* TODO: need to correct inputs here, since some gates may have
         been removed -- otherwise we may reference gates that no
         longer exist!  
         
         We should create a new hash table that stores the map, and
         get rid of the map file.
      */
      for(i = 0; i < tmp->data->arity; i++)
        {
          sprintf(buf, "%lu", tmp->data->inputs[i]);
          ent.key = buf;
          inputs[i] = tmp->data->inputs[i];
          if(hsearch_r(ent, FIND, &ret, &mtab) != 0)
            {
              /* The gate needs to be remapped */
              /* We only want to correct the mapping at the very
                 end!  */
              //              tmp->data->inputs[i] = ret->data;
              inputs[i] = (uint64_t)ret->data;
            }
        }
      free(buf);
      if(tmp->data->output != 0)
        {
          // Skip output gates
          cur = cur->next;
          id++;
          buf = malloc(22);
          sprintf(buf, "%lu",tmp->data->id);
          ent.key = buf;
          ent.data = (void*)id;
          hsearch_r(ent, ENTER, &ret, &mtab);
          mkeys[mcount] = ent.key;
          mcount++;
          //          tmp->data->id = id;
          continue;
        }
      assert((tmp->data->arity > 0) && (tmp->data->arity < 3));
      if(tmp->data->arity == 2)
        {
          sprintf(gate_desc, "%lu %lu %x", inputs[0], inputs[1], tmp->data->truth_table);
        }
      else if(tmp->data->arity == 1)
        {
          sprintf(gate_desc, "%lu %x", inputs[0], tmp->data->truth_table);
        }
      ent.key = gate_desc;
      if(hsearch_r(ent, FIND, &ret, &htab) == 0)
        {
#ifdef DEBUG_ENABLE
          fprintf(stderr, "Gate %s not found (%lu)\n",gate_desc, tmp->data->id);
#endif
          /* This gate is not already in the table */

          ent.key = gate_desc;
          ent.data = (void*)tmp->data->id;
          if(hsearch_r(ent, ENTER, &ret, &htab) == 0)
            {
              fprintf(stderr, "Could not insert element: %s\n", strerror(errno));
              exit(-1);
            }
          keys[count] = ent.key;
          count++;
          /* If the gate was found, we should not advance since we
             will be removing the gate and moving forward; since the
             gate was not found here, we add it to the hash table and
             proceed.*/
          cur = cur->next;

          /* TODO: this should be added to the id mapping hash */
          /* id++; */
          /* //          map_file_print(tmp->data->id, id); */
          /* buf = malloc(32); */
          /* sprintf(buf, "%lu",tmp->data->id); */
          /* ent.key = buf; */
          /* ent.data = (void*)id; */
          /* hsearch_r(ent, ENTER, &ret, &mtab); */
          /* //tmp->data->id = id; */
          /* mkeys[mcount] = ent.key; */
          /* mcount++; */
        }
      else
        {
          /* This is a redundant gate that can be removed */
#ifdef DEBUG_ENABLE
          fprintf(stderr, "Gate %s found: %lu\n",ent.key,ret->data);
#endif
          free(gate_desc); 
          // We store a map of ([table],[inputs]) -> id
          map_file_print(tmp->data->id, (uint64_t)ret->data);

          // This gate's id should be remapped
          buf = malloc(32);
          sprintf(buf, "%lu", tmp->data->id);
          ent.key = buf;
          ent.data = ret->data;
          hsearch_r(ent, ENTER, &ret, &mtab);

          // Fix memory leak?
          mkeys[mcount] = ent.key;
          mcount++;

          ltmp->next = tmp->next;
          //          if(tmp->data->comment != 0) free(tmp->data->comment);
          free(tmp->data->inputs);
          //          free(tmp->data->truth_table);
          free(tmp->data);
          free(tmp);
          tmp = ltmp;

          /* Don't advance to the next element, because we just
             removed it... */
        }
    }
  for(j = 0; j < count; j++)
    {
      free(keys[j]);
    }
  for(j = 0; j < mcount; j++)
    {
      free(mkeys[j]);
    }
  //  free(keys);
  //  free(mkeys);
  hdestroy_r(&htab);
  hdestroy_r(&mtab);
}
