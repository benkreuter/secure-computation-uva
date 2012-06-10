//#define _GNU_SOURCE
#include <unistd.h>
#include <search.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>

extern "C" {
#include "gate.h"
}

#include "util.h"

#include <map>
#include <string>
#include <gdbm.h>

using namespace std;

void * counts[64];
int8_t count_mapped[64];
uint64_t count_size[64];


uint8_t get_count(FILE * trk_f, uint64_t id)
{
  uint8_t ret = 0;
  if(store_type != 3)
    {
      fflush(trk_f);
      fseek(trk_f, sizeof(uint8_t)*id, SEEK_SET);
      fread(&ret, sizeof(uint8_t), 1, trk_f);
    }
  else
    {
      int fildes = fileno(trk_f);
      if(count_mapped[fildes] == 0)
        {
          if(id == 0)
            {
              if(ftruncate(fildes,sizeof(uint8_t)) < 0) abort();
              count_size[fildes] = sizeof(uint8_t);
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
              fprintf(stderr, "Map failed (get_count): %s\n", strerror(errno));
              store_type = 1;
              count_mapped[fildes] = 1;
            }
          else
            count_mapped[fildes] = -1;
        }
      if(count_mapped[fildes] == -1)
        {
          if(id*sizeof(uint8_t) >= count_size[fildes])
            {
              if(ftruncate(fildes, id*sizeof(uint8_t)*2) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              counts[fildes] = mremap(counts[fildes], count_size[fildes], id*sizeof(uint8_t)*2, MREMAP_MAYMOVE);
              count_size[fildes] = id*sizeof(uint8_t)*2;
            }
          ret = ((uint8_t*)counts[fildes])[id];
        }
      else
        {
          return get_count(trk_f,id);
        }
    }

  return ret;
}

void inc_count(FILE * trk_f, uint64_t id)
{
  uint8_t cur = get_count(trk_f, id);
  cur++;
  if(store_type != 3)
    {
      fseek(trk_f, sizeof(uint8_t)*id, SEEK_SET);
      fwrite(&cur, sizeof(uint8_t), 1, trk_f);
      fflush(trk_f);
    }
  else
    {
      int fildes = fileno(trk_f);
      if(count_mapped[fildes] == 0)
        {
          if(id == 0)
            {
              if(ftruncate(fildes,sizeof(uint8_t)) < 0) abort();
              count_size[fildes] = sizeof(uint8_t);
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
              fprintf(stderr, "Map failed (inc_count): %s\n", strerror(errno));
              store_type = 1;
              count_mapped[fildes] = 1;
            }
          else
            count_mapped[fildes] = -1;
        }

      if(count_mapped[fildes] == -1)
        {
          if(id*sizeof(uint8_t) >= count_size[fildes])
            {
              if(ftruncate(fildes, id*sizeof(uint8_t)*2) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              counts[fildes] = mremap(counts[fildes], count_size[fildes], id*sizeof(uint8_t)*2, MREMAP_MAYMOVE);
              count_size[fildes] = id*sizeof(uint8_t)*2;
            }
          ((uint8_t*)counts[fildes])[id] = cur;
        }
      else
        {
          inc_count(trk_f,id);
        }
    }

}

void set_count(FILE * trk_f, uint64_t id, uint8_t cur)
{
  if(store_type != 3)
    {
      fseek(trk_f, sizeof(uint8_t)*id, SEEK_SET);
      fwrite(&cur, sizeof(uint8_t), 1, trk_f);
      fflush(trk_f);
    }
  else
    {
      int fildes = fileno(trk_f);
      if(count_mapped[fildes] == 0)
        {
          if(id == 0)
            {
              if(ftruncate(fildes,sizeof(uint8_t)) < 0) abort();
              count_size[fildes] = sizeof(uint8_t);
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
          if(id*sizeof(uint8_t) >= count_size[fildes])
            {
              if(ftruncate(fildes, id*sizeof(uint8_t)*2) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              counts[fildes] = mremap(counts[fildes], count_size[fildes], id*sizeof(uint8_t)*2, MREMAP_MAYMOVE);
              count_size[fildes] = id*sizeof(uint8_t)*2;
            }
          ((uint8_t*)counts[fildes])[id] = cur;
        }
      else
        {
          set_count(trk_f,id,cur);
        }
    }

}

uint64_t get_count64(FILE * trk_f, uint64_t id)
{
  uint64_t ret;
  int8_t z;
  fflush(trk_f);
  fseek(trk_f, (sizeof(uint8_t) + sizeof(uint64_t))*id, SEEK_SET);
  if(fread(&z, sizeof(uint8_t), 1, trk_f) > 0)
    {
      if(z == -1)
        {
          fread(&ret, sizeof(uint64_t), 1, trk_f);
          return ret;
        }
      else 
        {
          return id;
        }
    }
  else
    {
      return id;
    }
}

void inc_count64(FILE * trk_f, uint64_t id)
{
  uint64_t cur = get_count64(trk_f, id);
  int8_t z = -1;
  cur++;
  fseek(trk_f, (sizeof(uint8_t) + sizeof(uint64_t))*id, SEEK_SET);
  fwrite(&z, sizeof(uint8_t), 1, trk_f);
  fwrite(&cur, sizeof(uint64_t), 1, trk_f);
  fflush(trk_f);
}

void set_count64(FILE * trk_f, uint64_t id, uint64_t cur)
{
  //  fseek(trk_f, sizeof(uint64_t)*id, SEEK_SET);
  int8_t z = -1;
  fseek(trk_f, (sizeof(uint8_t) + sizeof(uint64_t))*id, SEEK_SET);
  fwrite(&z, sizeof(uint8_t), 1, trk_f);
  fwrite(&cur, sizeof(uint64_t), 1, trk_f);
  fflush(trk_f);

}
void map_file_print(DB*, uint64_t old, uint64_t _new)
{
  //map_file = 
  if(store_type == 2) glob_map[old] = _new;
  map_file_print(old, _new);
}

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
  //  bool * live_gates;
  //  GDBM_FILE live_gates;
  //  DB * live_gates;
  init_maps();
  FILE * live_gates;
  uint64_t circuit_size, rep_size;
  //  uint64_t * _map, *newmap;
  //  map<uint64_t, uint64_t> _map, newmap;
  uint64_t input_cnt = 11;
  char arity;
  if(argc < 5)
    {
      fprintf(stderr, "Please specify the map file, circuit file and circuit size (./deadgates map_file size circuit_file)\n");
      return -1;
    }

  sscanf(argv[2], "%lu", &circuit_size);
  FILE * circuit_file = fopen(argv[1], "rb+");
  //  FILE * _map_file = fopen(argv[3], "r");
  //  GDBM_FILE _map = gdbm_open(argv[3], 512, GDBM_READER, S_IRWXU, 0);
  DB * _map = 0;
  FILE * map_ = 0;

  sscanf(argv[9], "%hhd", &store_type);

  sscanf(argv[10], "%hhd", &use_mmap_io);
  use_mmap_read = use_mmap_io;

  if(check_store_type(store_type) != 0)
    {
      fprintf(stderr, "Invalid store type\n");
      abort();
    }

  if(store_type == 0)
    {
      db_create(&_map,0,0);
      _map->set_cachesize(_map, 1, 1024, 0);
      _map->open(_map,0,argv[3],0,DB_BTREE,0,0664);
      map_file = _map;
    }
  else if((store_type == 1) || (store_type == 2) || (store_type == 3))
    {
      map_ = fopen(argv[3], "rb+");
    }
  else
    {
      // Invalid store_type
      abort();
    }
  sscanf(argv[4], "%lu", &input_cnt);

  //  GDBM_FILE newmap = gdbm_open(argv[6], 512, GDBM_WRCREAT, S_IRWXU, 0);
  // DB * newmap;
  // db_create(&newmap,0,0);
  // newmap->set_cachesize(newmap, 1, 1024, 0);
  // newmap->open(newmap, 0, argv[6], 0, DB_BTREE, DB_CREATE, 0664);
  FILE * newmap = 0;
  DB* newmap_ = 0;
  if(store_type != 0)
    {
      newmap = fopen(argv[6], "wb+");
    }
  else
    {
      db_create(&newmap_,0,0);
      ((DB*)newmap_)->set_cachesize(newmap_,1,1024,0);
      ((DB*)newmap_)->open(newmap_,0,argv[6],0,DB_BTREE,DB_CREATE,0664);
    }
  
  FILE * stats_file = fopen(argv[5], "r");
  fscanf(stats_file, "%lu", &circuit_size);
  circuit_size = 0;
  fscanf(stats_file, "%lu", &rep_size);

  fprintf(stderr, "Reported size: %ld\n", rep_size);
  //  rep_size--;
  //circuit_size*=2;
  circuit_size+=1;
  fclose(stats_file);
  stats_file = fopen(argv[5], "w");

  //  live_gates = (bool*)malloc(sizeof(bool) * circuit_size);
  //  _map = (uint64_t*)malloc(sizeof(uint64_t)* circuit_size);
  //  newmap = (uint64_t*)malloc(sizeof(uint64_t)* circuit_size);
  //  live_gates = gdbm_open(argv[7], 512, GDBM_WRCREAT, S_IRWXU, 0);
  // db_create(&live_gates,0,0);
  // live_gates->set_cachesize(live_gates, 1, 1024, 0);
  // live_gates->open(live_gates,0, argv[7], 0, DB_BTREE, DB_CREATE,0664);
  live_gates = fopen(argv[7], "wb+");
  uint64_t emit_cnt = input_cnt;
  FILE * circuit_out_file = fopen(argv[8], "wb+");


  fprintf(stderr, "Initialized\n");
  uint64_t cnt = 0;//input_cnt;
  int64_t next_id;
  fseek(circuit_file, 0, SEEK_SET);

  next_id = rep_size-1;
  circuit_size = 0;
  uint8_t one_out = 0;

  store_file = newmap;

  //  goto use_cnt_lbl;
  while(next_id >= 0)
    {
      /* Read a gate */
      char output, party, tab;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      // char line[128];

      // fgets(line, 127, stdin);
      // if(feof(stdin)) break;

      read_binary_gate(circuit_file, next_id, _map, map_, id, inputs, tab, output, party, arity);
      if(circuit_size == 0) circuit_size = id+1;
      next_id--;

      if((output != 0) || (get_count(live_gates,id) != 0))
        {
          set_count(live_gates, id, -1);
          set_count(live_gates, inputs[0], -1);
          set_count(live_gates, inputs[1], -1);
          if(output != 0) one_out = -1;
        }
      else
        {
          assert(one_out != 0);
          assert(output == 0);
        }
    }

  fprintf(stderr, "Built liveness table\n");
  // use_cnt_lbl:
  uint64_t ncnt = input_cnt;
  for(uint64_t qq = input_cnt+1; qq < circuit_size; qq++)
    {
      if(get_count(live_gates, qq) != 0)
        {
          ncnt++;

          //          set_count64(newmap, qq, ncnt);
          map_file_print(qq, ncnt);
        }
    }
  fseek(circuit_file, 0, SEEK_SET);
  fprintf(stderr, "Built usage count table\n");
  //  uint64_t cnt2 = ncnt-1;
  cnt = input_cnt;
  //  bool n = false;

  uint64_t xxx = 0, id;
  //  for(uint64_t i = 0; i <  circuit_size; i++)
  while(xxx < rep_size)//(!feof(circuit_file))
    {
      /* read a gate from the circuit file */
      /* Read a gate */
      char output, party, tab;
      uint64_t inputs[2];
      inputs[0] = inputs[1] = 0;
      //      char line[128];

      if(xxx == rep_size) break;
      assert(xxx < rep_size);
      read_binary_gate(circuit_file, xxx, _map, map_, id, inputs,tab,output,party,arity);
      xxx++;

      //      fprintf(stderr, "%llu\n", inputs[1]);
      //      if(live_gates[id])
      if(get_count(live_gates, id) != 0)
        {
#warning This is a dangerous design; if somehow a wire is remapped to 0, this will explode in your face.

          uint64_t kkk = map_lookup(newmap_, newmap, inputs[0]); //get_count64(newmap, inputs[0]);
          if(kkk != 0) inputs[0] = kkk;
          kkk = map_lookup(newmap_, newmap, inputs[1]);//get_count64(newmap, inputs[1]);
          if(kkk != 0) inputs[1] = kkk;

          cnt++;
          if(cnt <= inputs[1]) fprintf(stderr, "%ld %ld %ld\n", cnt, id, inputs[1]);
          assert(id > inputs[1]);
          write_binary_gate(circuit_out_file, cnt, inputs, tab, output, party, arity);
          emit_cnt++;
        }
      else
        {
          //          _map[id] = 0;
          assert(output == 0);
          //          fprintf(stderr, "Dead gate: %ld\n", id);
        }
      
    }


  fprintf(stderr, "Finally: %ld %ld %ld %ld %ld %ld\n", xxx, cnt, ncnt, emit_cnt, id, input_cnt);

  if(store_type == 0)
    {
      _map->close(_map,0);
    }
  else if(store_type == 1)
    {
      fclose(map_);
    }
  if(store_type == 0) newmap_->close(newmap_,0);
  else fclose(newmap);
  unlink(argv[6]);
  //  live_gates->close(live_gates,0);
  fclose(live_gates);

  // Clear out the file -- otherwise we will have some problems
  unlink(argv[7]);

  fprintf(stats_file, "%lu\n", emit_cnt - input_cnt);
  fclose(stats_file);

  fclose(circuit_out_file);

  fprintf(stderr, "Emitted %lu gates\n", emit_cnt - input_cnt);

  return 0;
}
