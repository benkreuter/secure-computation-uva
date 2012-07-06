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

int main(int argc, char** argv)
{
  /* Build a table of gates that will lead to an output.  This uses
   * the following algorithm:
   *
   * 1. Read all gates, from the outputs backward.
   * 2. For each gate that is an output, mark its input gates as live.
   * 3. For each gate that is marked as live, mark its inputs as live.
   *
   * This should cover all the gates that actually lead to an output;
   * then we read the circuit file itself, and echo those gates which
   * are live.
   */
  bool * live_gates;
  uint64_t circuit_size;
  //  uint64_t * _map, *newmap;
  map<uint64_t, uint64_t> _map, newmap;
  uint64_t input_cnt = 11;
  char arity;
  if(argc < 6)
    {
      fprintf(stderr, "Please specify the map file, circuit file and circuit size (./deadgates map_file size circuit_file)\n");
      return -1;
    }

  sscanf(argv[2], "%lu", &circuit_size);
  FILE * circuit_file = fopen(argv[1], "r");
  FILE * _map_file = fopen(argv[3], "r");
  sscanf(argv[4], "%lu", &input_cnt);

  uint64_t emit_cnt = input_cnt;

  FILE * stats_file = fopen(argv[5], "r");
  fscanf(stats_file, "%lu", &circuit_size);
  circuit_size*=2;
  fclose(stats_file);
  stats_file = fopen(argv[5], "w");

  live_gates = (bool*)malloc(sizeof(bool) * circuit_size);
  //  _map = (uint64_t*)malloc(sizeof(uint64_t)* circuit_size);
  //  newmap = (uint64_t*)malloc(sizeof(uint64_t)* circuit_size);
  for(uint64_t j = 0; j < circuit_size; j++) 
    {
      //      _map[j] = newmap[j] = 0;
      live_gates[j] = false;
    }
  fprintf(stderr, "Initialized\n");
  /* Read the _map file, populate the _mapping hash table */
  while(!feof(_map_file))
    {
      //      char * old = malloc(sizeof(char) * 22);
      uint64_t _new, old;
      //      ENTRY ent, *ret;

      fscanf(_map_file, "%lu %lu", &old, &_new);
      if(feof(_map_file)) break;
      //      fprintf(stderr, "%llu %llu\n", old, _new);

      _map[old] = _new;
      /*      ent.key = old;
      ent.data = new;
      hsearch_r(ent, ENTER, &ret, &mtab);*/
    }
  fprintf(stderr, "Read map\n");

  uint64_t cnt = input_cnt;
  while(!feof(circuit_file))
    {
      // Count the lines
      char line[256];
      fgets(line, 255, circuit_file);
      cnt++;
    }
  fprintf(stderr, "Size: %lu\n", cnt);
  fseek(circuit_file, 0, SEEK_SET);
  while(!feof(stdin))
    {
      /* Read a gate */
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      char line[128];

      fgets(line, 127, stdin);
      if(feof(stdin)) break;

      read_gate(line, _map, id, inputs, tab, output, party, arity);
      
      //      _map[id] = id;
      //      if(newmap[inputs[0]] != 0) inputs[0] = newmap[inputs[0]];
      //      if(newmap[inputs[1]] != 0) inputs[1] = newmap[inputs[1]];
      // if(newmap.find(inputs[0]) != newmap.end())
      //   inputs[0] = newmap.find(inputs[0])->second;
      // if(newmap.find(inputs[1]) != newmap.end())
      //   inputs[1] = newmap.find(inputs[1])->second;

      if((output != 0) || (live_gates[id]))
        {
          /*          newmap[id] = cnt;
          cnt--;
          */
          //          assert((inputs[1] != 606567) && (inputs[0] != 606567) && (id != 606567));
          live_gates[id] = true;
          live_gates[inputs[0]] = true;
          live_gates[inputs[1]] = true;
        }
      else
        {
          assert(output == 0);
          //fprintf(stderr, "Will die: %lu\n", id);
        }
    }

  uint64_t ncnt = input_cnt;
  for(uint64_t qq = input_cnt+1; qq < circuit_size; qq++)
    {
      if(live_gates[qq])
        {
          ncnt++;
          newmap[qq] = ncnt;
        }
      /*if(qq == 606569)
        {
          fprintf(stderr, "%lu %lu\n", newmap.find(606569)->second, ncnt);
          }*/
    }
  cnt = input_cnt;
  bool n = false;
  //  for(uint64_t i = 0; i <  circuit_size; i++)
  while(!feof(circuit_file))
    {
      /* read a gate from the circuit file */
      /* Read a gate */
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      char line[128];

      fgets(line, 127, circuit_file);
      if(feof(circuit_file)) break;
      read_gate(line, _map, id, inputs, tab, output, party, arity);

      /* A gate might have be remapped again. */
      // if(newmap[inputs[0]] != 0) inputs[0] = newmap[inputs[0]];
      // if(newmap[inputs[1]] != 0) inputs[1] = newmap[inputs[1]];
      if(newmap.find(inputs[0]) != newmap.end())
        inputs[0] = newmap.find(inputs[0])->second;
      if(newmap.find(inputs[1]) != newmap.end())
        inputs[1] = newmap.find(inputs[1])->second;

      //      fprintf(stderr, "%llu\n", inputs[1]);
      if(live_gates[id])
        {
          cnt++;
          //          _map[id] = cnt;
          // Need to correct twice
          // fprintf(stderr, "%s %lu %lu\n", line, newmap[id], cnt);

          // if(n)
          //   {
          //     exit(-1);
          //   }
          if(newmap.find(id)->second != cnt)
            {
              fprintf(stderr, "%s %lu %lu\n%d %d\n", line, newmap.find(id)->second, cnt, live_gates[id-1], live_gates[id-2]);
              exit(-1);
            }
          //newmap[_map[id]] = cnt;
          //if(id == 29) fprintf(stderr, "%lu\n", _map[id]);
          //          fprintf(stderr, "%s", line);
          emit_gate(arity, inputs, tab, cnt, output, party);
          emit_cnt++;
        }
      else
        {
          //          _map[id] = 0;
          assert(output == 0);
          //fprintf(stderr, "Dead gate: %s", line);
        }
      
    }
  fprintf(stats_file, "%lu\n", emit_cnt);
  fclose(stats_file);

  return 0;
}
