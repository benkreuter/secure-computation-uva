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


#include "util.h"

//using namespace boost::intrusive;

map<uint64_t,uint64_t> * load_map(FILE * map_file)//, uint64_t map_size)
//avl_set<pair<uint64_t,uint64_t> >* load_map(FILE * map_file, uint64_t map_size)
{
  //uint64_t * _map = (uint64_t*)malloc(sizeof(uint64_t)*map_size);
  // for(uint64_t i = 0; i < map_size; i++)
  //   {
  //     _map[i] = 0;
  //   }
  map<uint64_t,uint64_t> * _map = new map<uint64_t,uint64_t>;
  //avl_set<pair<uint64_t,uint64_t> > * _map = new avl_set<pair<uint64_t,uint64_t> >;
  while(!feof(map_file))
    {
      uint64_t _new, old;

      fscanf(map_file, "%lu %lu", &old, &_new);

      //     (*_map)[old] = _new;
      _map->insert(pair<uint64_t,uint64_t>(old,_new));
    }
  return _map;
}

int read_gate(const char* line, const std::map<uint64_t,uint64_t>& _map, uint64_t& id, uint64_t* inputs, char & tab, char & output, char & party, char & arity)
//int read_gate(const char* line, const avl_set<pair<uint64_t,uint64_t> >& _map, uint64_t& id, uint64_t* inputs, char & tab, char & output, char & party, char & arity)
{
  inputs[0] = inputs[1] = 0;
  tab = 0;
  char q = 0;
  int i,x,y;
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
  if(_map.find(inputs[0]) != _map.end())//(_map != 0) && (_map[inputs[0]] != 0))
    {
      inputs[0] = (_map.find(inputs[0]))->second;
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
      if(_map.find(inputs[1]) != _map.end())//(_map != 0) && (_map[inputs[1]] != 0))
        {
          inputs[1] = _map.find(inputs[1])->second;
        }
      arity = 2;
    }
  else
    {
      inputs[1] = 0;
      arity = 1;
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
  return 0;
}

#if 0
void emit_gate_in_string(char* line, char arity, uint64_t * inputs, char t, uint64_t id, char output, char party)
{
  // TODO: fix now
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

}
#endif
