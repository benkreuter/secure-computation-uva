//#define _GNU_SOURCE
#include <search.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

extern "C" {
#include "gate.h"
}

#include "util.h"

#include <map>
#include <string>

using namespace std;


int main(int argc, char ** argv)
{
  struct hsearch_data htab;
  uint64_t circuit_size, *_map, j;
  FILE * _map_file;
  map<gate_key<uint32_t>,uint32_t,bool (*)(gate_key<uint32_t>,gate_key<uint32_t>)> gates(gate_key_comp<uint32_t>);

  if(argc < 3)
    {
      fprintf(stderr, "Please specify the _map file and circuit size\n");
      return -1;
    }

  sscanf(argv[2], "%llu", &circuit_size);
  _map_file = fopen(argv[1], "r");

  //  memset(&mtab, 0, sizeof(struct hsearch_data));
  memset(&htab, 0, sizeof(struct hsearch_data));
  if(hcreate_r(2.5*circuit_size, &htab) == 0)
    {
      fprintf(stderr, "Cannot allocate hash table: %s\n", strerror(errno));
      return -1;
    }
  //  hcreate_r(2.5*circuit_size, &htab);
  _map = (uint64_t*)malloc(sizeof(uint64_t)*circuit_size);
  for(j = 0; j < circuit_size; j++) _map[j] = 0;
  /* Read the _map file, populate the _mapping hash table */
  while(!feof(_map_file))
    {
      //      char * old = malloc(sizeof(char) * 22);
      uint64_t _new, old;
      //      ENTRY ent, *ret;

      fscanf(_map_file, "%llu %llu", &old, &_new);

      _map[old] = _new;
      /*      ent.key = old;
      ent.data = new;
      hsearch_r(ent, ENTER, &ret, &mtab);*/
    }

  /* Now we read each gate from stdin, and remove redundant gates as
     we go along -- immediately writing the gates to stdout.
   */
  uint64_t watch = 0;
  while(!feof(stdin))
    {
      watch++;
      assert(watch < circuit_size);
      char line[128], i, tmp[128], q, tab, x,y, output, party;
      uint64_t inputs[2], id;
      //      ENTRY ent, *ret;
      fgets(line, 127, stdin);
      inputs[0] = inputs[1] = 0;
      tab = 0;
      q = 0;
      /* Need to create a gate struct */
      for(i = 0; line[i] != ','; i++)
        {
          //          fprintf(stderr, "%c ", line[i]);
          assert((line[i] <= '9') && (line[i] >= '0'));
          inputs[q] *= 10;
          inputs[q] += line[i] - '0';
        }
      x = i;
      q++;
      i++;
      for(; (line[i] != ',')&&(line[i]!=' ');i++)
        {
          assert((line[i] <= '9') && (line[i] >= '0'));
          inputs[q] *= 10;
          inputs[q] += line[i] - '0';

          tab = tab << 1;
          tab += line[i] - '0';
        }
      y = i;
      if(line[i] == ',')
        {
          /* 2-arity gate */
          q++;
          i++;
          tab=0;
          for(; line[i] != ' '; i++)
            {
              tab = tab << 1;
              tab += line[i] - '0';
            }
        }
      /* Now we have the inputs and the table fully parsed; time to
         substitute the inputs with the values from the _map, and check
         for duplicate gates. */
      /* strncpy(tmp, line, x); */
      /* tmp[x+1] = '\0'; */
      /* ent.key = tmp; */
      /* if(hsearch_r(ent, FIND, &ret, &mtab) != 0) */
      /*   { */
      /*     /\* This index has been re_mapped *\/ */
      /*     inputs[0] = ret->data; */
      /*   } */
      if(_map[inputs[0]] != 0)
        {
          inputs[0] = _map[inputs[0]];
        }
      if(q > 1)
        {
          /* strncpy(tmp, line+x+1, y-x-1); */
          /* tmp[y-x] = '\0'; */
          /* ent.key = tmp; */
          /* if(hsearch_r(ent, FIND, &ret, &mtab) != 0) */
          /*   { */
          /*     /\* This index has been re_mapped *\/ */
          /*     inputs[1] = ret->data; */
          /*   }           */
          if(_map[inputs[1]] != 0)
            {
              inputs[1] = _map[inputs[1]];
            }
        }
      //key = malloc(sizeof(char)*(i+1));
      id = 0;
      /* Read the rest of the line:  id, output, party */
      i++;
      for(; line[i] != ' '; i++)
        {
          assert((line[i] <= '9') && (line[i] >= '0'));
          id *= 10;
          id += line[i] - '0';
        }
      i++;
      output = 0;
      for(; line[i] != ' '; i++)
        {
          assert((line[i] <= '9') && (line[i] >= '0'));
          output *= 10;
          output += line[i] - '0';
        }
      i++;
      party = 0;
      for(; (line[i] != '\n' && line[i] != ' '); i++)
        {
          //          assert((line[i] <= '9') && (line[i] >= '0'));
          if((line[i] <= '9') && (line[i] >= '0'))
            {
              party *= 10;
              party += line[i] - '0';
            }
          else
            {
              fprintf(stderr, "Bad line %s (%x %d)\n", line, line[i], i);
              return -1;
            }
        }

      /* TODO: We are still storing too much here.  We need to use the
         scheduling method that Chih-hao is using... */
      //      sprintf(tmp, "%llx,%llx,%x", inputs[0], inputs[1], tab);
      //      key = malloc(strlen(tmp)+1);
      //      strcpy(key, tmp);
      //      strncpy(key, &inputs[0], sizeof(uint64_t));
      //strncpy(key + sizeof(uint64_t), &inputs[1], sizeof(uint64_t));
      //strncpy(key + 2*sizeof(uint64_t), &tab, 1);
      //      key[2*sizeof(uint64_t)+2] = '\0';
      //      ent.key = key;
      //      if(hsearch_r(ent, FIND, &ret, &htab) == 0)
      gate_key<uint32_t> key;
      key.input0 = inputs[0];
      key.input1 = inputs[1];
      key.tab = tab;
      map<gate_key<uint32_t>,uint32_t,bool (*)(gate_key<uint32_t>,gate_key<uint32_t>)>::iterator _gate = gates.find(key);
      if(_gate == gates.end())
        {
          /* The gate has not been seen before */
          /*          ent.data = id;
                      hsearch_r(ent, ENTER, &ret, &htab);*/
          emit_gate(q,inputs,tab,id,output,party);
          gates[key] = id;
        }
      else
        {
          /* This gate has been seen before */
          //          free(key);
          /* sprintf(tmp, "%llu", id); */
          /* key = malloc(strlen(tmp)+1); */
          /* strcpy(key, tmp); */
          /* ent.key = key; */
          /* ent.data = ret->data; */
          /* hsearch_r(ent, ENTER, &ret, &mtab); */
          _map[id] = _gate->second;
        }
    }
  return 0;
}
