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
  FILE * binary = fopen(argv[2],"wb");
  FILE * input_file = fopen(argv[3], "r");
  FILE * stats_file = fopen(argv[4], "r");
  uint64_t circuit_size;
  //  sscanf(argv[4], "%lu", &circuit_size);
  fscanf(stats_file, "%lu", &circuit_size);
  uint64_t * usage_cnt = (uint64_t *)malloc(sizeof(uint64_t)*circuit_size);

  uint64_t gate_cnt = 0;
  uint32_t gen_in = 0,gen_out = 0,evl_in = 0,evl_out = 0;
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
      uint64_t id;
      char party;

      unsigned int z;
      for(z = 0; (z < strlen(line)) && (line[z] != ' '); z++)
        {
          assert((line[z] >= '0') && (line[z] <= '9'));
          id *= 10;
          id += line[z] - '0';
        }

      for(; (z < strlen(line)) && (line[z] != '.'); z++);
      z++;
      if(line[z] == 'a')
        {
          party = 0;
          gen_in++;
        }
      else
        {
          party = 1;
          evl_in++;
        }

      input_parties.push_back(party);
    }

  fseek(circuit, 0, SEEK_SET);
  fwrite(&gate_cnt, sizeof(uint64_t), 1, binary);
  fflush(binary);

  fwrite(&gen_in, sizeof(uint32_t), 1, binary);
  fwrite(&evl_in, sizeof(uint32_t), 1, binary);
  fwrite(&gen_out, sizeof(uint32_t), 1, binary);
  fwrite(&evl_out, sizeof(uint32_t), 1, binary);

  uint8_t _dummy = 8;
  fwrite(&_dummy, sizeof(uint8_t), 1, binary);

  for(unsigned int i = 0; i < gen_in + evl_in; i++)
    {
      uint8_t tab = 0;
      if(input_parties[i] == 1) tab |= 0x40;
      else tab |= 0x20;

      fwrite(&tab, sizeof(uint8_t), 1, binary);
      fflush(binary);
      fwrite(&(usage_cnt[i]), sizeof(uint64_t), 1, binary);
      fflush(binary);
    }

  uint8_t idx_size = 0;
  //  uint8_t tag = 0;
  //  int truth_table_size;
  //  uint64_t gate_cnt = 0;
  //  int ix;
  int i;

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

      if(id <= UINT16_MAX)
        {
          idx_size = sizeof(uint16_t);
        }
      else if(id <= UINT32_MAX)
        {
          idx_size = sizeof(uint32_t);
        }
      else
        {
          idx_size = sizeof(uint64_t);
        }

      if(output != 0)
        {
          //tab = 0x02;
          if(tab == 0x02) tab = 0x01;
          else if(tab == 0x01) tab = 0x02;
          if(party == 1)
            tab |= 0x80;
          else
            tab |= 0x60;
        }
      else
        {

          // Reverse the tab
          uint8_t q = tab & 0x0F;
          tab = ((q & 0x08) >> 3) | ((q & 0x04) >> 1) | ((q & 0x02) << 1) | ((q & 0x01) << 3);
          tab |= 0x10;
        }

      fwrite(&tab, sizeof(uint8_t), 1, binary);
      fflush(binary);
      for(i = 0; i < arity; i++)
        {           
          if(idx_size == sizeof(uint16_t))
            {
              uint16_t idx = inputs[i];
              fwrite(&idx,sizeof(uint16_t), 1, binary);
            }
          else if(idx_size == sizeof(uint32_t))
            {
              uint32_t idx = inputs[i];
              fwrite(&idx,sizeof(uint32_t), 1, binary);
            }
          else if(idx_size == sizeof(uint64_t))
            {
              uint64_t idx = inputs[i];
              fwrite(&idx,sizeof(uint64_t), 1, binary);
            }
          fflush(binary);
          
        }
      fwrite(&(usage_cnt[id]), sizeof(uint64_t), 1, binary);
      fflush(stdout);
    }

  return 0;
}
