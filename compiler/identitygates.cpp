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
#include <sys/stat.h>

using namespace std;

uint64_t max_id = 0;

/* 
 * Use an AVL tree because we are spending a lot of time looking
 * things up in the tree, whereas insertion and removal are a one-time
 * cost.
 */
#include <db.h>

/* 
 * Use Berkeley DB for everything...
 */
#if 0
void map_file_print(DB* map_file, uint64_t old, uint64_t _new)
{
  DBT key, val;

  memset(&key, 0, sizeof(DBT));
  memset(&val, 0, sizeof(DBT));

  key.data = (char*)(&old);
  key.size = sizeof(uint64_t);

  val.data = (char*)(&_new);
  val.size = sizeof(uint64_t);

  map_file->put(map_file, NULL, &key, &val, 0);
}
#endif

void map_file_print(DB*, uint64_t old, uint64_t _new)
{
  //map_file = 
  if(store_type == 2) glob_map[old] = _new;
  map_file_print(old, _new);
}

int main_berkeley_db(int argc, char** argv)
{
 //  uint64_t _circuit_size;
  init_maps();
  if(argc < 3)
    {
      fprintf(stderr, "Please supply map file\n");
      return -1;
    }

  uint64_t circuit_size;
  FILE * stats_file = fopen(argv[2], "r");
  fscanf(stats_file, "%ld", &circuit_size);
  fclose(stats_file);

  stats_file = fopen(argv[2], "w");
  FILE * binary = fopen(argv[4], "wb+");


  fprintf(stderr, "Reported size for IdentityGates: %ld\n", circuit_size);

  //  FILE * map_file = fopen(argv[1], "r");
  //  sscanf(argv[2], "%lu", &_circuit_size);
  // Going to need to work on this....TODO
  //map<uint64_t,uint64_t> * __map = load_map(map_file);//, _circuit_size);
  //  map<uint64_t,uint64_t>& oldmap = *__map;
  //map<uint64_t,uint64_t> _map;

  //  GDBM_FILE oldmap = gdbm_open(argv[1], 512, GDBM_READER, S_IRWXU, fatal);

  //  int x= 1048786*2;
  //  gdbm_setopt(oldmap, GDBM_CACHESIZE, &x, sizeof(int));

  //  GDBM_FILE new_map_file = gdbm_open(argv[4], 512, GDBM_WRCREAT, S_IRWXU, fatal);


  sscanf(argv[6], "%hhd", &store_type);
  if(check_store_type(store_type) != 0)
    {
      fprintf(stderr, "Invalid store type\n");
      exit(-1);
    }

  sscanf(argv[7], "%hhd", &use_mmap_io);
  use_mmap_read = use_mmap_io;

  DB * oldmap = 0, * new_map_file = 0;
  FILE * old_map = 0, * new_map_file_ = 0;

  if(store_type == 0)
    {
      db_create(&oldmap, NULL ,0);
      db_create(&new_map_file, NULL, 0);

      oldmap->set_cachesize(oldmap, 2, 1024, 0);  
      new_map_file->set_cachesize(new_map_file, 2, 1024, 0);

      oldmap->open(oldmap, 0, argv[1], 0, DB_BTREE, 0, 0664);
      new_map_file->open(new_map_file, 0, argv[3], 0, DB_BTREE, DB_CREATE, 0664);

      map_file = new_map_file;
    }
  else //if((store_type == 1) || (store_type == 2) || (store_type == 3))
    {
      old_map = fopen(argv[1], "rb+");
      new_map_file_ = fopen(argv[3], "wb+");
      store_file = new_map_file_;
    }
  FILE * binary_in = fopen(argv[5], "rb");


  map<uint64_t,uint64_t> notmap;

  // fclose(map_file);
  // map_file = fopen(argv[1], "w"); //"a");
  uint64_t zero_gate = 0, one_gate = 0;

  uint64_t xxx = 0;

  while(xxx < circuit_size) //(!feof(stdin))
    {

      char line[256];
      line[0] = '\0';
      char output, party, tab, arity;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      
      read_binary_gate(binary_in,xxx, oldmap, old_map, id, inputs, tab, output, party, arity);
      xxx++;
      inputs[0] = map_lookup(new_map_file, new_map_file_, inputs[0]);
      inputs[1] = map_lookup(new_map_file, new_map_file_, inputs[1]);
      assert((id > inputs[0]) && (id > inputs[1]));
      if(max_id < id) max_id = id;

      if(output == 0)
        {
          /* 
           * Remove NOT gates here
           */
          if(notmap.find(inputs[0]) != notmap.end())
            {
              char t = ((tab & 0x08) >> 1) | ((tab & 0x04) << 1) | ((tab & 0x02) >> 1) | ((tab & 0x01) << 1);
              tab = t;
              inputs[0] = notmap.find(inputs[0])->second;
            }
            //   // Rewrite line
            //   char tmp[256];
            //   char x[5];
            //   x[3] = '0' + ((t >> 0) & 0x01);
            //   x[2] = '0' + ((t >> 1) & 0x01);
            //   x[1] = '0' + ((t >> 2) & 0x01);
            //   x[0] = '0' + ((t >> 3) & 0x01);
            //   x[4] = '\0';

            //   sprintf(tmp, "%lu,%lu,%s %lu %u %u\n", inputs[0], inputs[1], x, id, output, party);
            //   strcpy(line,tmp);
            // }
          if(notmap.find(inputs[1]) != notmap.end())
            {
              char t = ((tab & 0x08) >> 2) | ((tab & 0x04) >> 2) | ((tab & 0x02) << 2) | ((tab & 0x01) << 2);
              tab = t;
              inputs[1] = notmap.find(inputs[1])->second;
            }
          if(true)
            {
              // Rewrite line
              char tmp[256];
              // x[3] = '0' + ((tab >> 0) & 0x01);
              // x[2] = '0' + ((tab >> 1) & 0x01);
              // x[1] = '0' + ((tab >> 2) & 0x01);
              // x[0] = '0' + ((tab >> 3) & 0x01);
              // x[4] = '\0';

              //              sprintf(tmp, "%lu,%lu,%s %lu %u %u\n", inputs[0], inputs[1], x, id, output, party);
              //              fprintf(stderr, "%s", tmp);
              strcpy(line,tmp);
            }

          if(tab == 0) 
            {
                zero_gate = id;
                fprintf(stderr, "Zero: %ld\n", id);
                //                printf("%s", line);
                write_binary_gate(binary, id, inputs, tab, output, party, 2);
                total_emitted_gates++;
            }
          else if(tab == 0xF)
            {
              one_gate = id;
              fprintf(stderr, "One: %ld\n", id);
              //              printf("%s", line);
              write_binary_gate(binary, id, inputs, tab, output, party, 2);
              total_emitted_gates++;
            }
          else if(tab == 6)
            {
              // XOR gate, might be identity or zero gate
              if(inputs[0] == inputs[1])
                {
                  // XOR(tab,x) = 0
                  if(zero_gate == 0)
                    {
                      zero_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print(new_map_file, id, zero_gate);
                      //                      _map[id] = zero_gate;
                    }
                }
              else if((zero_gate != 0) || (one_gate != 0))
                {
                  if((inputs[0] == zero_gate) && (zero_gate != 0))
                    {
                      map_file_print(new_map_file, id, inputs[1]);
                      //_map[id] = inputs[1];
                      //    continue;
                    }
                  else if ((inputs[1] == zero_gate) && (zero_gate != 0))
                    {
                      map_file_print(new_map_file, id, inputs[0]);
                      //_map[id] = inputs[0];
                      //                      continue;
                    }
                  /* If we are XORing with a 1, then we are an inverter. */
                  else if((inputs[0] == one_gate) && (one_gate != 0))
                    {
                      // Need to write to a new map file for NOT gates
                      //                      map_file_print(new_map_file, id, inputs[1]);
                      notmap[id] = inputs[1];
                    }
                  else if((inputs[1] == one_gate) && (one_gate != 0))
                    {
                      //                      map_file_print(new_map_file, id, inputs[0]);
                      notmap[id] = inputs[0];
                    }
                  else
                    {
                      // Not an identity
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else if(tab == 1)
            {
              // AND gate, might be identity
              // Might also be a zero gate
              if(inputs[0] == inputs[1])
                {
                  map_file_print(new_map_file, id, inputs[0]);
                  //_map[id] = inputs[0];
                }
              else if((one_gate != 0) || (zero_gate != 0))
                {
                  if((inputs[0] == one_gate) && one_gate != 0)
                    {
                      map_file_print(new_map_file, id, inputs[1]);
                      //_map[id] = inputs[1];
                      //                      continue;
                    }
                  else if((inputs[1] == one_gate) && (one_gate != 0))
                    {
                      map_file_print(new_map_file, id, inputs[0]);
                      //_map[id] = inputs[0];
                      //                      continue;
                    }
                  else if(((inputs[0] == zero_gate) || (inputs[1] == zero_gate)) && (zero_gate != 0))
                    {
                      // AND(tab,0) = 0
                      map_file_print(new_map_file, id, zero_gate);
                      //_map[id] = zero_gate;
                    }
                  else
                    {
                      // Not an identity
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else if(tab == 9)
            {
              // XNOR gate, could become a constant
              if(inputs[0] == inputs[1])
                {
                  // Constant gate
                  if(one_gate == 0)
                    {
                      one_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print(new_map_file, id, one_gate);
                      //_map[id] = one_gate;
                    }

                }
              else if((zero_gate != 0) || (one_gate != 0))
                {
                  if((inputs[0] == one_gate) && (one_gate != 0))
                    {
                      map_file_print(new_map_file, id, inputs[1]);
                      //_map[id] = inputs[1];
                      //    continue;
                    }
                  else if ((inputs[1] == one_gate) && (one_gate != 0))
                    {
                      map_file_print(new_map_file, id, inputs[0]);
                      //_map[id] = inputs[0];
                      //                      continue;
                    }
                  /* If we are XORing with a 1, then we are an inverter. */
                  else if((inputs[0] == zero_gate) && (zero_gate != 0))
                    {
                      // Need to write to a new map file for NOT gates
                      //                      map_file_print(new_map_file, id, inputs[1]);
                      notmap[id] = inputs[1];
                    }
                  else if((inputs[1] == zero_gate) && (zero_gate != 0))
                    {
                      //                      map_file_print(new_map_file, id, inputs[0]);
                      notmap[id] = inputs[0];
                    }
                  else
                    {
                      // Not an identity
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else if(tab == 4)
            {
              if(inputs[0] == inputs[1])
                {
                  if(zero_gate == 0)
                    {
                      zero_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print(new_map_file, id, zero_gate);
                      //_map[id] = zero_gate;
                    }
                }
              else if((zero_gate != 0) && (inputs[0] == zero_gate))
                {
                  map_file_print(new_map_file, id, zero_gate);
                  //_map[id] = zero_gate;
                }
              else if((zero_gate != 0) && (inputs[1] == zero_gate))
                {
                  map_file_print(new_map_file, id, inputs[0]);
                  //_map[id] = inputs[0];
                }
              else if((one_gate != 0) && (inputs[0] == one_gate))
                {
                  // Inverter
                  notmap[id] = inputs[1];
                }
              else if((one_gate != 0) && (inputs[1] == one_gate))
                {
                  if(zero_gate == 0)
                    {
                      zero_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print(new_map_file, id, zero_gate);
                      //_map[id] = zero_gate;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else
            {
              //              printf("%s", line);
              write_binary_gate(binary, id, inputs, tab, output, party, 2);
              total_emitted_gates++;
            }
        }
      else
        {
          if(notmap.find(inputs[0]) != notmap.end())
            {
              char t = ((tab & 0x02) >> 1) | ((tab & 0x01) << 1);
              tab = t;
              inputs[0] = notmap.find(inputs[0])->second;
            }
          if(true)
            {
              //              if(inputs[0] == 64) fprintf(stderr, "%s\n", line);
              // Rewrite line
              char tmp[256];
              //              char x[5];
              /*x[3] = '0' + ((tab >> 0) & 0x01);*/
              /*x[2] = '0' + ((tab >> 1) & 0x01);*/
              // x[1] = '0' + ((tab >> 0) & 0x01);
              // x[0] = '0' + ((tab >> 1) & 0x01);
              // x[2] = '\0';

              //              sprintf(tmp, "%lu,%s %lu %u %u\n", inputs[0], tab, id, output, party);
              //              fprintf(stderr, "%s", tmp);
              strcpy(line,tmp);
            }
          //   printf("%s", line);
          write_binary_gate(binary, id, inputs, tab, output, party, 1);
          total_emitted_gates++;
        }
    }
  //  delete __map;
  fprintf(stats_file, "%lu\n", max_id);
  fprintf(stats_file, "%lu\n", total_emitted_gates);

  fprintf(stderr, "Emitted %lu gates\n", total_emitted_gates);

  //  gdbm_close(oldmap);
  //  gdbm_close(new_map_file);
  if(store_type == 0)
    {
      oldmap->close(oldmap,0);
      new_map_file->close(new_map_file, 0);
    }
  else if(store_type == 1)
    {
      fclose(old_map);
      fclose(new_map_file_);
    }
  return 0;
}

void map_file_print_old(FILE * map_file, uint64_t old, uint64_t _new)
{
  fprintf(map_file, "%lu %lu\n", old, _new);
}

int main_map(int argc, char ** argv)
{
  //  uint64_t _circuit_size;

  if(argc < 2)
    {
      fprintf(stderr, "Please supply map file\n");
      return -1;
    }

  FILE * stats_file = fopen(argv[3], "w");

  FILE * map_file = fopen(argv[1], "r");
  //  sscanf(argv[2], "%lu", &_circuit_size);
  // Going to need to work on this....TODO
  map<uint64_t,uint64_t> * __map = load_map(map_file);//, _circuit_size);
  map<uint64_t,uint64_t>& oldmap = *__map;
  map<uint64_t,uint64_t> _map;
  FILE * binary = fopen(argv[4], "wb");
#if 0
  /* 
   * This is the map of NOT gates.
   */
  uint64_t * notmap = (uint64_t*)malloc(sizeof(uint64_t)*_circuit_size);

  for(uint64_t j = 0; j < _circuit_size; j++)
    {
      notmap[j] = 0;
    }
#endif
  map<uint64_t,uint64_t> notmap;

  fclose(map_file);
  map_file = fopen(argv[1], "w"); //"a");
  uint64_t zero_gate = 0, one_gate = 0;

  while(!feof(stdin))
    {
      char line[256];
      // fgets(line, 255, stdin);

      // if(feof(stdin)) break;
      if(fgets(line, 255, stdin) == 0)
        {
          if(feof(stdin)) break;
          else
            {
              fprintf(stderr, "Error: %s\n", strerror(errno));
              exit(-1);
            }
        }

      char output, party, tab, arity;
      uint64_t inputs[2], id;
      inputs[0] = inputs[1] = 0;
      
      read_gate_old(line, oldmap, id, inputs, tab, output, party, arity);

      if(_map.find(inputs[0]) != _map.end()) inputs[0] = _map[inputs[0]];
      if(_map.find(inputs[1]) != _map.end()) inputs[1] = _map[inputs[1]];

      if(max_id < id) max_id = id;

      if(output == 0)
        {
          /* 
           * Remove NOT gates here
           */
          if(notmap.find(inputs[0]) != notmap.end())
            {
              char t = ((tab & 0x08) >> 1) | ((tab & 0x04) << 1) | ((tab & 0x02) >> 1) | ((tab & 0x01) << 1);
              tab = t;
              inputs[0] = notmap.find(inputs[0])->second;
            }
            //   // Rewrite line
            //   char tmp[256];
            //   char x[5];
            //   x[3] = '0' + ((t >> 0) & 0x01);
            //   x[2] = '0' + ((t >> 1) & 0x01);
            //   x[1] = '0' + ((t >> 2) & 0x01);
            //   x[0] = '0' + ((t >> 3) & 0x01);
            //   x[4] = '\0';

            //   sprintf(tmp, "%lu,%lu,%s %lu %u %u\n", inputs[0], inputs[1], x, id, output, party);
            //   strcpy(line,tmp);
            // }
          if(notmap.find(inputs[1]) != notmap.end())
            {
              char t = ((tab & 0x08) >> 2) | ((tab & 0x04) >> 2) | ((tab & 0x02) << 2) | ((tab & 0x01) << 2);
              tab = t;
              inputs[1] = notmap.find(inputs[1])->second;
            }
          if(true)
            {
              // Rewrite line
              char tmp[256];
              char x[5];
              x[3] = '0' + ((tab >> 0) & 0x01);
              x[2] = '0' + ((tab >> 1) & 0x01);
              x[1] = '0' + ((tab >> 2) & 0x01);
              x[0] = '0' + ((tab >> 3) & 0x01);
              x[4] = '\0';

              sprintf(tmp, "%lu,%lu,%s %lu %u %u\n", inputs[0], inputs[1], x, id, output, party);
              //              fprintf(stderr, "%s", tmp);
              strcpy(line,tmp);
            }

          if(tab == 0) 
            {
                zero_gate = id;
                //                printf("%s", line);
                write_binary_gate(binary, id, inputs, tab, output, party, 2);
                total_emitted_gates++;
            }
          else if(tab == 0xF)
            {
              one_gate = id;
              //              printf("%s", line);
              write_binary_gate(binary, id, inputs, tab, output, party, 2);
              total_emitted_gates++;
            }
          else if(tab == 6)
            {
              // XOR gate, might be identity or zero gate
              if(inputs[0] == inputs[1])
                {
                  // XOR(x,x) = 0
                  if(zero_gate == 0)
                    {
                      zero_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print_old(map_file, id, zero_gate);
                      _map[id] = zero_gate;
                    }
                }
              else if((zero_gate != 0) || (one_gate != 0))
                {
                  if((inputs[0] == zero_gate) && (zero_gate != 0))
                    {
                      map_file_print_old(map_file, id, inputs[1]);
                      _map[id] = inputs[1];
                      //    continue;
                    }
                  else if ((inputs[1] == zero_gate) && (zero_gate != 0))
                    {
                      map_file_print_old(map_file, id, inputs[0]);
                      _map[id] = inputs[0];
                      //                      continue;
                    }
                  /* If we are XORing with a 1, then we are an inverter. */
                  else if((inputs[0] == one_gate) && (one_gate != 0))
                    {
                      // Need to write to a new map file for NOT gates
                      //                      map_file_print_old(map_file, id, inputs[1]);
                      notmap[id] = inputs[1];
                    }
                  else if((inputs[1] == one_gate) && (one_gate != 0))
                    {
                      //                      map_file_print_old(map_file, id, inputs[0]);
                      notmap[id] = inputs[0];
                    }
                  else
                    {
                      // Not an identity
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else if(tab == 1)
            {
              // AND gate, might be identity
              // Might also be a zero gate
              if(inputs[0] == inputs[1])
                {
                  map_file_print_old(map_file, id, inputs[0]);
                  _map[id] = inputs[0];
                }
              else if((one_gate != 0) || (zero_gate != 0))
                {
                  if((inputs[0] == one_gate) && (one_gate != 0))
                    {
                      map_file_print_old(map_file, id, inputs[1]);
                      _map[id] = inputs[1];
                      //                      continue;
                    }
                  else if((inputs[1] == one_gate) && (one_gate != 0))
                    {
                      map_file_print_old(map_file, id, inputs[0]);
                      _map[id] = inputs[0];
                      //                      continue;
                    }
                  else if(((inputs[0] == zero_gate) || (inputs[1] == zero_gate)) && (zero_gate != 0))
                    {
                      // AND(x,0) = 0
                      map_file_print_old(map_file, id, zero_gate);
                      _map[id] = zero_gate;
                    }
                  else
                    {
                      // Not an identity
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else if(tab == 9)
            {
              // XNOR gate, could become a constant
              if(inputs[0] == inputs[1])
                {
                  // Constant gate
                  if(one_gate == 0)
                    {

                      one_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print_old(map_file, id, one_gate);
                      _map[id] = one_gate;
                    }

                }
              else if((zero_gate != 0) || (one_gate != 0))
                {
                  if((inputs[0] == one_gate) && (one_gate != 0))
                    {
                      map_file_print_old(map_file, id, inputs[1]);
                      _map[id] = inputs[1];
                      //    continue;
                    }
                  else if ((inputs[1] == one_gate) && (one_gate != 0))
                    {
                      map_file_print_old(map_file, id, inputs[0]);
                      _map[id] = inputs[0];
                      //                      continue;
                    }
                  /* If we are XORing with a 1, then we are an inverter. */
                  else if((inputs[0] == zero_gate) && (zero_gate != 0))
                    {
                      // Need to write to a new map file for NOT gates
                      //                      map_file_print_old(map_file, id, inputs[1]);
                      notmap[id] = inputs[1];
                    }
                  else if((inputs[1] == zero_gate) && (zero_gate != 0))
                    {
                      //                      map_file_print_old(map_file, id, inputs[0]);
                      notmap[id] = inputs[0];
                    }
                  else
                    {
                      // Not an identity
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else if(tab == 4)
            {
              if(inputs[0] == inputs[1])
                {
                  if(zero_gate == 0)
                    {
                      zero_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print_old(map_file, id, zero_gate);
                      _map[id] = zero_gate;
                    }
                }
              else if((zero_gate != 0) && (inputs[0] == zero_gate))
                {
                  map_file_print_old(map_file, id, zero_gate);
                  _map[id] = zero_gate;
                }
              else if((zero_gate != 0) && (inputs[1] == zero_gate))
                {
                  map_file_print_old(map_file, id, inputs[0]);
                  _map[id] = inputs[0];
                }
              else if((one_gate != 0) && (inputs[0] == one_gate))
                {
                  // Inverter
                  notmap[id] = inputs[1];
                }
              else if((one_gate != 0) && (inputs[1] == one_gate))
                {
                  if(zero_gate == 0)
                    {
                      zero_gate = id;
                      //                      printf("%s", line);
                      write_binary_gate(binary, id, inputs, tab, output, party, 2);
                      total_emitted_gates++;
                    }
                  else
                    {
                      map_file_print_old(map_file, id, zero_gate);
                      _map[id] = zero_gate;
                    }
                }
              else
                {
                  //                  printf("%s", line);
                  write_binary_gate(binary, id, inputs, tab, output, party, 2);
                  total_emitted_gates++;
                }
            }
          else
            {
              //              printf("%s", line);
              write_binary_gate(binary, id, inputs, tab, output, party, 2);
              total_emitted_gates++;
            }
        }
      else
        {
          if(notmap.find(inputs[0]) != notmap.end())
            {
              char t = ((tab & 0x02) >> 1) | ((tab & 0x01) << 1);
              tab = t;
              inputs[0] = notmap.find(inputs[0])->second;
            }
          if(true)
            {
              //              if(inputs[0] == 64) fprintf(stderr, "%s\n", line);
              // Rewrite line
              char tmp[256];
              char x[5];
              /*x[3] = '0' + ((tab >> 0) & 0x01);*/
              /*x[2] = '0' + ((tab >> 1) & 0x01);*/
              x[1] = '0' + ((tab >> 0) & 0x01);
              x[0] = '0' + ((tab >> 1) & 0x01);
              x[2] = '\0';

              sprintf(tmp, "%lu,%s %lu %u %u\n", inputs[0], x, id, output, party);
              //              fprintf(stderr, "%s", tmp);
              strcpy(line,tmp);
            }
          //          printf("%s", line);
          write_binary_gate(binary, id, inputs, tab, output, party, 1);
          total_emitted_gates++;
        }
    }
  delete __map;
  fprintf(stats_file, "%lu\n", max_id);
  fprintf(stats_file, "%lu\n", total_emitted_gates);
  return 0;
}

/* 
 * TODO: We need to deal with NOT gates here as well.  These require
 * more careful handling than identity gates, since any gate that
 * reads from a NOT gate must have its truth table changed.  We should
 * store a new table, which tracks NOT gates and maps their IDs to the
 * IDs they invert.
 *
 * Alternatively, we could ignore this issue, since NOT gates are
 * essentially free.  However, if the goal is to minimize the size of
 * the _circuit in general, we will probably want to do this....
 */
int main(int argc, char** argv)
{
  char * switch_ = getenv("USE_DB");
  if(switch_ != 0)
    {
      return main_berkeley_db(argc, argv);
    }
  else
    {
      return main_map(argc,argv);
    }

  return -1;
}
