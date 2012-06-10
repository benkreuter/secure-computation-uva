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
#include <sys/stat.h>
#include <sys/mman.h>

extern "C" {
#include "gate.h"
}

#include "util.h"

#include <map>
#include <string>
#include <vector>
#include <db.h>

using namespace std;

enum { ETC = 0, GEN_INP = 1, EVL_INP = 2, GEN_OUT = 3, EVL_OUT = 4};

void * counts[64];
int8_t count_mapped[64];
uint64_t count_size[64];

uint64_t get_count(FILE * cnt, uint64_t id)
{
  uint64_t ret = 0;
  if(store_type != 3)
    {
      if(fseek(cnt, sizeof(uint64_t)*id, SEEK_SET) != 0) fprintf(stderr, "Problem seeking\n");
      if(fread(&ret, sizeof(uint64_t), 1, cnt) != 1) fprintf(stderr, "Problem reading\n");
    }
  else
    {
      int fildes = fileno(cnt);
      if(count_mapped[fildes] == 0)
        {
          if(id == 0)
            {
              if(ftruncate(fildes,sizeof(uint64_t)) < 0) abort();
              count_size[fildes] = sizeof(uint64_t);
            }
          else
            {
              struct stat statbuf;
              if(fstat(fildes,&statbuf) != 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              count_size[fildes] = statbuf.st_size;
            }
          if(count_size[fildes] == 0) count_size[fildes] = 1;
          counts[fildes] = mmap(0, count_size[fildes], PROT_READ|PROT_WRITE, MAP_SHARED, fildes, 0);
          if(counts[fildes] == MAP_FAILED)
            {
              fprintf(stderr, "Map failed: %s\n", strerror(errno));
              store_type = 1;
              count_mapped[fildes] = 1;
            }
          else
            count_mapped[fildes] = -1;
        }
      if(count_mapped[fildes] == -1)
        {
          if(id*sizeof(uint64_t) >= count_size[fildes])
            {
              if(ftruncate(fildes, id*sizeof(uint64_t)*2) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              counts[fildes] = mremap(counts[fildes], count_size[fildes], id*sizeof(uint64_t)*2, MREMAP_MAYMOVE);
              count_size[fildes] = id*sizeof(uint64_t)*2;
            }
          ret = ((uint64_t*)counts[fildes])[id];
        }
      else
        {
          return get_count(cnt,id);
        }
    }
  //  if(id == 256) fprintf(stderr, "%ld\n", ret);
  return ret;
}

void inc_count(FILE * cnt, uint64_t id)
{
  uint64_t cur = get_count(cnt, id);
  cur++;
  if(store_type != 3)
    {
      fseek(cnt, sizeof(uint64_t)*id, SEEK_SET);
      fwrite(&cur, sizeof(uint64_t), 1, cnt);
      fflush(cnt);
    }
  else
    {
      int fildes = fileno(cnt);
      if(count_mapped[fildes] == 0)
        {
          if(id == 0)
            {
              if(ftruncate(fildes,sizeof(uint64_t)) < 0) abort();
              count_size[fildes] = sizeof(uint64_t);
            }
          else
            {
              struct stat statbuf;
              if(fstat(fildes,&statbuf) != 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              count_size[fildes] = statbuf.st_size;
            }
          if(count_size[fildes] == 0) count_size[fildes] = 1;
          counts[fildes] = mmap(0, count_size[fildes], PROT_READ|PROT_WRITE, MAP_SHARED, fildes, 0);
          if(counts[fildes] == MAP_FAILED)
            {
              fprintf(stderr, "Map failed: %s\n", strerror(errno));
              store_type = 1;
              count_mapped[fildes] = 1;
            }
          else
            count_mapped[fildes] = -1;
        }

      if(count_mapped[fildes] == -1)
        {
          if(id*sizeof(uint64_t) >= count_size[fildes])
            {
              if(ftruncate(fildes, id*sizeof(uint64_t)*2) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              counts[fildes] = mremap(counts[fildes], count_size[fildes], id*sizeof(uint64_t)*2, MREMAP_MAYMOVE);
              count_size[fildes] = id*sizeof(uint64_t)*2;
            }
          ((uint64_t*)counts[fildes])[id] = cur;
        }
      else
        {
          inc_count(cnt,id);
        }
    }
}

void set_count(FILE * cnt, uint64_t id, uint64_t cur)
{
  if(store_type != 3)
    {
      fseek(cnt, sizeof(uint64_t)*id, SEEK_SET);
      fwrite(&cur, sizeof(uint64_t), 1, cnt);
      fflush(cnt);
    }
  else
    {
      int fildes = fileno(cnt);
      if(count_mapped[fildes] == 0)
        {
          if(id == 0)
            {
              if(ftruncate(fildes,sizeof(uint64_t)) < 0) abort();
              count_size[fildes] = sizeof(uint64_t);
            }
          else
            {
              struct stat statbuf;
              if(fstat(fildes,&statbuf) != 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              count_size[fildes] = statbuf.st_size;
            }
          if(count_size[fildes] == 0) count_size[fildes] = 1;
          counts[fildes] = mmap(0, count_size[fildes], PROT_READ|PROT_WRITE, MAP_SHARED, fildes, 0);
          if(counts[fildes] == MAP_FAILED)
            {
              fprintf(stderr, "Map failed: %s\n", strerror(errno));
              store_type = 1;
              count_mapped[fildes] = 1;
            }
          else
            count_mapped[fildes] = -1;
        }
      if(count_mapped[fildes] == -1)
        {
          if(id*sizeof(uint64_t) >= count_size[fildes])
            {
              if(ftruncate(fildes, id*sizeof(uint64_t)*2) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              counts[fildes] = mremap(counts[fildes], count_size[fildes], id*sizeof(uint64_t)*2, MREMAP_MAYMOVE);
              count_size[fildes] = id*sizeof(uint64_t)*2;
            }
          ((uint64_t*)counts[fildes])[id] = cur;
        }
      else
        {
          set_count(cnt,id,cur);
        }
    }

}

int main(int argc, char ** argv)
{
  if(argc < 5)
    {
      fprintf(stderr, "Too few arguments\n");
      return -1;
    }
  //  map<uint64_t,uint64_t> dummy;
  DB* dummy = 0;
  FILE * dummy_ = 0;

  sscanf(argv[6], "%hhd", &store_type);

  sscanf(argv[7], "%hhd", &use_mmap_io);
  use_mmap_read = use_mmap_io;

  init_maps();
  FILE * circuit = fopen(argv[1],"rb+");
  FILE * binary = fopen(argv[2],"wb");
  FILE * input_file = fopen(argv[3], "r");
  // uint64_t circuit_size;
  // sscanf(argv[4], "%lu", &circuit_size);
  FILE * stats_file = fopen(argv[4], "r");
  uint64_t circuit_size;
  //  sscanf(argv[4], "%lu", &circuit_size);
  fscanf(stats_file, "%lu", &circuit_size);

  //  uint64_t * usage_cnt = (uint64_t *)malloc(sizeof(uint64_t)*circuit_size);

  // DB * usage_cnt;
  // db_create(&usage_cnt, NULL, 0);
  // usage_cnt->set_cachesize(usage_cnt, 2, 1024, 0);
  // usage_cnt->open(usage_cnt, 0, argv[5], 0, DB_BTREE, DB_CREATE, 0664);

  FILE * usage_cnt = fopen(argv[5], "wb+");
  fflush(usage_cnt);
  if(usage_cnt == 0)
    {
      fprintf(stderr, "%s\n", strerror(errno));
      return -1;
    }

  uint64_t gate_cnt = 0;
  uint32_t gen_in = 0,gen_out = 0,evl_in = 0,evl_out = 0;
  char arity;
  uint64_t xxx = 0;

  while(xxx < circuit_size) // (!feof(circuit))
    {
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      //      char line[128];
      /*
      if(fgets(line, 127, circuit) == 0)
        {
          if(feof(circuit)) break;
          else
            {
              fprintf(stderr, "Error: %s\n", strerror(errno));
              exit(-1);
            }
        }
      */
      //      read_gate(line, dummy, id, inputs, tab, output, party, arity);
      read_binary_gate(circuit, xxx, dummy, dummy_, id, inputs,tab,output,party,arity);
      xxx++;
      //usage_cnt[id] = 0;
      DBT key, val;
      memset(&key, 0, sizeof(DBT));
      memset(&val, 0, sizeof(DBT));
      uint64_t xx = 0;
      key.data = (char*)&id;
      key.size = sizeof(uint64_t);
      val.data = (char*)&xx;
      val.size = sizeof(uint64_t);
      //      usage_cnt->put(usage_cnt, 0, &key, &val, 0);
      set_count(usage_cnt, id, 0);

      //      usage_cnt[inputs[0]]++;
      memset(&key, 0, sizeof(DBT));
      memset(&val, 0, sizeof(DBT));
      //      uint64_t xx = 0;
      key.data = (char*)&(inputs[0]);
      key.size = sizeof(uint64_t);
      /*if(usage_cnt->get(usage_cnt, 0, &key, &val, 0) == 0)
        xx = *((uint64_t*)val.data) + 1;
      else
        xx = 1;
      memset(&key, 0, sizeof(DBT));
      memset(&val, 0, sizeof(DBT));
      //      uint64_t xx = 0;
      key.data = (char*)&(inputs[0]);
      key.size = sizeof(uint64_t);
      val.data = (char*)&xx;
      val.size = sizeof(uint64_t);*/
      //      usage_cnt->put(usage_cnt, 0, &key, &val, 0);
      inc_count(usage_cnt, inputs[0]);
      inc_count(usage_cnt, inputs[1]);

      // key.data = (char*)&(inputs[1]);
      // key.size = sizeof(uint64_t);
      // if(usage_cnt->get(usage_cnt, 0, &key, &val, 0) == 0)
      //   xx = *((uint64_t*)val.data) + 1;
      // else
      //   xx = 1;
      // memset(&key, 0, sizeof(DBT));
      // memset(&val, 0, sizeof(DBT));
      // //      uint64_t xx = 0;
      // key.data = (char*)&(inputs[1]);
      // key.size = sizeof(uint64_t);
      // val.data = (char*)&xx;
      // val.size = sizeof(uint64_t);
      // usage_cnt->put(usage_cnt, 0, &key, &val, 0);
      //      usage_cnt[inputs[1]]++;
      if(output != 0)
        {
          //usage_cnt[id]++;
          // xx = 1;
          // key.data = (char*)&id;
          // key.size = sizeof(uint64_t);
          // val.data = (char*)&xx;
          // val.size = sizeof(uint64_t);
          // usage_cnt->put(usage_cnt, 0, &key, &val, 0);
          set_count(usage_cnt, id, 1);
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
  fflush(binary);
  fwrite(&evl_in, sizeof(uint32_t), 1, binary);
  fflush(binary);
  fwrite(&gen_out, sizeof(uint32_t), 1, binary);
  fflush(binary);
  fwrite(&evl_out, sizeof(uint32_t), 1, binary);
  fflush(binary);

  uint8_t _dummy = 8;
  fwrite(&_dummy, sizeof(uint8_t), 1, binary);

  fflush(binary);

  for(unsigned int i = 0; i < gen_in + evl_in; i++)
    {
      uint8_t tab = 0;
      if(input_parties[i] == 1) tab |= 0x40;
      else tab |= 0x20;

      fwrite(&tab, sizeof(uint8_t), 1, binary);
      fflush(binary);
      //      fwrite(&(usage_cnt[i]), sizeof(uint64_t), 1, binary);
      // DBT key, val;
      // memset(&key, 0, sizeof(DBT));
      // memset(&val, 0, sizeof(DBT));
      // uint64_t xx = 0;

      // key.data = (char*)&(i);
      // key.size = sizeof(uint64_t);
      // if(usage_cnt->get(usage_cnt, 0, &key, &val, 0) == 0)
      //   xx = *((uint64_t*)val.data);
      // else
      //   xx = 0;
      uint64_t xx = get_count(usage_cnt, i);
      fwrite(&xx, sizeof(uint64_t), 1, binary);
      fflush(binary);
    }

  uint8_t idx_size = 0;
  //  uint8_t tag = 0;
  //  int truth_table_size;
  //  uint64_t gate_cnt = 0;
  //  int ix;
  int i;

  //  uint64_t xxx;
  xxx=0;

  while(xxx < circuit_size) //(!feof(circuit))
    {
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      //      char line[128];

      // fgets(line, 127, circuit);

      // if(feof(circuit)) break;

      // if(fgets(line, 127, circuit) == 0)
      //   {
      //     if(feof(circuit)) break;
      //     else
      //       {
      //         fprintf(stderr, "Error: %s\n", strerror(errno));
      //         exit(-1);
      //       }
      //   }


      //      read_gate(line, dummy, id, inputs, tab, output, party, arity);
      if(xxx == gate_cnt) break;
      read_binary_gate(circuit, xxx, dummy, dummy_, id, inputs,tab,output,party,arity);
      xxx++;

      if(id < inputs[0]) fprintf(stderr, "%ld %ld %ld\n", xxx, id, inputs[0]);
      assert(id > inputs[0]);
      if(id < inputs[1]) fprintf(stderr, "%ld %ld %ld\n", xxx, id, inputs[1]);
      assert(id > inputs[1]);

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
          tab = 0x02;
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
      //      fwrite(&(usage_cnt[id]), sizeof(uint64_t), 1, binary);
      DBT key, val;
      memset(&key, 0, sizeof(DBT));
      memset(&val, 0, sizeof(DBT));
      uint64_t xx = 0;

      key.data = (char*)&(i);
      key.size = sizeof(uint64_t);
      // if(usage_cnt->get(usage_cnt, 0, &key, &val, 0) == 0)
      //   xx = *((uint64_t*)val.data);
      // else
      //   xx = 0;
      xx = get_count(usage_cnt, id);
      if(id == 256) fprintf(stderr, "%ld\n", xx);
      if(id == 257) fprintf(stderr, "%ld\n", xx);
      //      xx = xx - 1; // ???
      fwrite(&xx, sizeof(uint64_t), 1, binary);
      fflush(binary);
      fflush(stdout);
    }

  return 0;
}
