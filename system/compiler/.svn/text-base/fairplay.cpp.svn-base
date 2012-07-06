//#define _GNU_SOURCE
#define __STDC_LIMIT_MACROS
#include <search.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>

extern "C" {
#include "gate.h"
}

#include "util.h"

#include <map>
#include <string>
#include <vector>

using namespace std;

enum { ETC = 0, GEN_INP = 1, EVL_INP = 2, GEN_OUT = 3, EVL_OUT = 4};

int main(int argc, char ** argv)
{
  if(argc < 5)
    {
      fprintf(stderr, "Too few arguments\n");
      return -1;
    }
  map<uint64_t,uint64_t> dummy;
  FILE * circuit = fopen(argv[1],"r");
  FILE * input_file = fopen(argv[3], "r");
  FILE * stats_file = fopen(argv[4], "r");
  uint64_t circuit_size;
  //  sscanf(argv[4], "%lu", &circuit_size);
  fscanf(stats_file, "%lu", &circuit_size);
  uint64_t * usage_cnt = (uint64_t *)malloc(sizeof(uint64_t)*circuit_size);

  uint64_t gate_cnt = 0;
  uint32_t gen_out = 0,evl_out = 0;
  char arity;

  while(!feof(circuit))
    {
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      char line[128];

      if(fgets(line, 127, circuit) == 0)
        {
          if(feof(circuit)) break;
          else
            {
              fprintf(stderr, "Error: %s\n", strerror(errno));
              exit(-1);
            }
        }

      read_gate(line, dummy, id, inputs, tab, output, party, arity);
      usage_cnt[id] = 0;

      usage_cnt[inputs[0]]++;
      usage_cnt[inputs[1]]++;
      if(output != 0)
        {
          usage_cnt[id]++;
          if(party == 0) gen_out++;
          else evl_out++;
        }
      gate_cnt++;
    }

  vector<char> input_parties;

  /* Read inputs */
  while(!feof(input_file))
    {
      char line[128];
      // fgets(line, 127, input_file);
      // if(feof(input_file)) break;
      if(fgets(line, 127, input_file) == 0)
        {
          if(feof(input_file)) break;
          else
            {
              fprintf(stderr, "Error: %s\n", strerror(errno));
              exit(-1);
            }
        }

      gate_cnt++;
      fprintf(stdout, "%s", line);
    }

  fseek(circuit, 0, SEEK_SET);


  //  uint8_t tag = 0;
  //  int truth_table_size;
  //  uint64_t gate_cnt = 0;
  //  int ix;

  uint64_t ao=0,bo=0;

  while(!feof(circuit))
    {
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      char line[128];

      // fgets(line, 127, circuit);

      // if(feof(circuit)) break;

      if(fgets(line, 127, circuit) == 0)
        {
          if(feof(circuit)) break;
          else
            {
              fprintf(stderr, "Error: %s\n", strerror(errno));
              exit(-1);
            }
        }


      read_gate(line, dummy, id, inputs, tab, output, party, arity);


      if(output != 0)
        {
          fprintf(stdout, "%lu output gate arity 1 table [ %c %c ] inputs [ %lu ] //output$output.%s[%lu]$0\n", id, ((tab>>1) % 2) + '0', (tab % 2) + '0', inputs[0], (party == 0) ? "alice" : "bob", (party == 0) ? ao : bo);
          if(party == 0) ao++;
          else bo++;
        }
      else
        {
          fprintf(stdout, "%lu gate arity 2 table [ %c %c %c %c ] inputs [ %lu %lu ]\n", id, ((tab >> 3) %2) + '0', ((tab >> 2) % 2) + '0', ((tab >> 1) % 2) + '0', (tab % 2) + '0', inputs[0], inputs[1]);
        }

    }

  return 0;
}
