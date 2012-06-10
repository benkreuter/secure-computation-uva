#define _GNU_SOURCE
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
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

extern "C" {
#include "gate.h"
}

#include "util.h"

#include <map>
#include <string>

using namespace std;


#include "util.h"


map<uint64_t,uint64_t> glob_map;


uint64_t map_lookup(DB * db_map, FILE * file_map, uint64_t id)
{
  uint64_t ret;
  if(store_type == 0)
    {
      DBT key, val;
      memset(&key, 0, sizeof(DBT));
      memset(&val, 0, sizeof(DBT));
      key.data = (char*)&(id);
      key.size = sizeof(uint64_t);
      if(db_map->get(db_map, 0, &key, &val, 0) != DB_NOTFOUND)
        {
          ret = *((uint64_t*)val.data);
        }
      else
        ret = id;
    }
  else if(store_type == 1)
    {
      fflush(file_map);
      if(fseek(file_map, (sizeof(uint8_t) + sizeof(uint64_t))*id, SEEK_SET) < 0)
        {
          fprintf(stderr, "%s\n", strerror(errno));
          abort();
        }
      uint8_t written;
      if(fread(&written, sizeof(uint8_t), 1, file_map) < 1)
        {
          //fprintf(stderr, "%s\n", strerror(errno));
          //          abort();
          if(ferror(file_map) != 0)
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          /* 
           * If ferror returns 0, there is no error; we are simply
           * trying to read beyond EOF.  This means that we never
           * wrote anything out here i.e. that this wire has not yet
           * been remapped.
           *
           * A similar test is not performed below, because if we
           * wrote one byte but not the remaining 8, there is
           * definitely a problem.
           */
          ret = id;
        }
      else
        {
          if(written != 0)
            {
              if(fread(&ret, sizeof(uint64_t), 1, file_map) < 1)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }              
            }

          else
            ret = id;
        }
    }
  else if(store_type == 2)
    {
      // STL map backed by flat file
      map<uint64_t,uint64_t>::iterator val = glob_map.find(id);
      if(val != glob_map.end()) // The value was alread in the map
        ret = val->second;
      else // Check file
        {
          fflush(file_map);
          if(fseek(file_map, (sizeof(uint8_t) + sizeof(uint64_t))*id, SEEK_SET) < 0)
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          uint8_t written;
          if(fread(&written, sizeof(uint8_t), 1, file_map) < 1)
            {
              //fprintf(stderr, "%s\n", strerror(errno));
              //          abort();
              if(ferror(file_map) != 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              /* 
               * If ferror returns 0, there is no error; we are simply
               * trying to read beyond EOF.  This means that we never
               * wrote anything out here i.e. that this wire has not yet
               * been remapped.
               *
               * A similar test is not performed below, because if we
               * wrote one byte but not the remaining 8, there is
               * definitely a problem.
               */
              ret = id;
            }
          else
            {
              if(written != 0)
                {
                  if(fread(&ret, sizeof(uint64_t), 1, file_map) < 1)
                    {
                      fprintf(stderr, "%s\n", strerror(errno));
                      abort();
                    }
                  assert(ret <= id);

                  // If the number of elements will exceed the max
                  // size, flush the map.  There is definitely a
                  // better way to do this....
                  if(glob_map.size() == max_map_items) glob_map.clear();

                  glob_map[id] = ret; // add this to the map
                }

              else
                ret = id;
            }
        }
    }
  else if(store_type == 3)
    {
      int fildes = fileno(file_map);
      // Use memory mapped IO if possible.
      if(mappeds[fildes] == 0)
        {
          //          fprintf(stderr, "Initializing map %d for read\n", fildes);
          // We need to make sure the file is long enough for us to
          // always use.
#warning We will rely on ftruncate to increase the size of memory mapped files as needed.  This might not be portable.
          struct stat statbuf;
          if(fstat(fildes,&statbuf))
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          if(statbuf.st_size > max_map_items*(sizeof(uint8_t) + sizeof(uint64_t)))
            {
              maps[fildes] = mmap(0,statbuf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,fildes,0);
              map_size_inits[fildes] = statbuf.st_size;
            }
          else
            {
              // Write a zero at the appropriate length, so that we avoid SIGBUS
              if(ftruncate(fildes, max_map_items * (sizeof(uint8_t) + sizeof(uint64_t))) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              maps[fildes] = mmap(0, max_map_items*(sizeof(uint8_t)+sizeof(uint64_t)), PROT_READ|PROT_WRITE,MAP_SHARED,fildes,0);
              map_size_inits[fildes] = max_map_items*(sizeof(uint8_t)+sizeof(uint64_t));
            }
          if(maps[fildes] == MAP_FAILED)
            {
              mappeds[fildes] = 1;
              store_type = 1;
              fprintf(stderr, "Could not map (map_lookup): %s\n", strerror(errno));
            }
          else
            {
              mappeds[fildes] = -1;
            }
        }
      if(mappeds[fildes] == -1)
        {
          uint64_t new_pos = id*(sizeof(uint8_t)+sizeof(uint64_t));
          if(new_pos >= map_size_inits[fildes])
            {
              //              if(ftruncate(fildes,map_size_inits[fildes]*10) == -1)
              if(ftruncate(fildes, new_pos*2) == -1)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              maps[fildes] = mremap(maps[fildes], map_size_inits[fildes], new_pos*2, MREMAP_MAYMOVE);
              if(maps[fildes] == MAP_FAILED)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              map_size_inits[fildes] = new_pos*2; //map_size_inits[fildes]*10;
            }
          uint8_t written = ((uint8_t*)maps[fildes])[id*(sizeof(uint8_t)+sizeof(uint64_t))];
          if(written != 0)
            {
              ret = *((uint64_t*)(((uint8_t*)maps[fildes])+id*(sizeof(uint8_t)+sizeof(uint64_t))+sizeof(uint8_t)));
            }
          else
            {
              ret = id;
            }
        }
      else if(mappeds[fildes] == 1)
        {
          // Cannot use memory map for some reason!
          if(mappeds[fildes] == -1) 
            {
              fprintf(stderr, "Something went wrong with map %d!\n", fildes);
              abort();
            }
          //store_type = 1;
          return map_lookup(db_map, file_map, id);
        }
      else
        abort();
    }
  else
    {
      abort();
    }
  assert(ret <= id);
  return ret;
}

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

//int read_gate(const char* line, const std::map<uint64_t,uint64_t>& _map, uint64_t& id, uint64_t* inputs, char & tab, char & output, char & party, char & arity)
//int read_gate(const char* line, const avl_set<pair<uint64_t,uint64_t> >& _map, uint64_t& id, uint64_t* inputs, char & tab, char & output, char & party, char & arity)
int read_gate(const char* line, DB* _map, FILE * map_, uint64_t& id, uint64_t* inputs, char & tab, char & output, char & party, char & arity)
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
#if 0
  DBT key,val;
  memset(&key,0,sizeof(DBT));
  memset(&val,0,sizeof(DBT));
  if(_map != 0)
    {
      key.data = (char*)&(inputs[0]);
      key.size = sizeof(uint64_t);
      //      val = gdbm_fetch(_map, key);

      //  if(_map.find(inputs[0]) != _map.end())//(_map != 0) && (_map[inputs[0]] != 0))
      //      if(val.dptr != 0)
      if(_map->get(_map,0,&key,&val,0) != DB_NOTFOUND)
        {
          inputs[0] = *((uint64_t*)val.data);//(_map.find(inputs[0]))->second;
          //          free(val.dptr);
        }
    }
#endif
  if((_map != 0) || (map_ != 0))
    inputs[0] = map_lookup(_map, map_, inputs[0]);
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
      //      if(_map.find(inputs[1]) != _map.end())//(_map != 0) && (_map[inputs[1]] != 0))
      if((_map != 0) || (map_ != 0))
        inputs[1] = map_lookup(_map, map_, inputs[1]);
#if 0
        {
          key.data = (char*)&(inputs[1]);
          key.size = sizeof(uint64_t);
          //          val = gdbm_fetch(_map, key);
          //  if(_map.find(inputs[0]) != _map.end())//(_map != 0) && (_map[inputs[0]] != 0))
          //          if(val.dptr != 0)
          if(_map->get(_map,0,&key,&val,0) != DB_NOTFOUND)
            {
              inputs[1] = *((uint64_t*)val.data);//(_map.find(inputs[0]))->second;_map.find(inputs[1])->second;
              //              free(val.dptr);
            }
        }
#endif
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

void * read_buffer;
int8_t read_mapped = 0;
uint64_t read_size_init = 0;
uint64_t read_map_cnt = 0;
uint8_t use_mmap_read=0;

void read_binary_gate(FILE * binary, 
                      uint64_t pos, 
                      DB* _map, 
                      FILE * map_,
                      uint64_t& id, 
                      uint64_t* inputs, 
                      char & tab, 
                      char & output, 
                      char & party, 
                      char & arity)
{
  // Read a gate from a binary file.
  inputs[0] = inputs[1] = 0;
  tab = 0;
  char q = 0;
  int i,x,y;
  char idx_size = 0;

  idx_size = sizeof(uint64_t);
  arity = 0;

  if(use_mmap_read == 0)
    {
      fseek(binary, (sizeof(uint8_t) + sizeof(uint64_t)*3)*pos, SEEK_SET);
      //    }


      fread(&tab, sizeof(uint8_t), 1, binary);
    }
  else
    {
      int fildes = fileno(binary);
      if(read_mapped == 0)
        {
          struct stat statbuf;
          if(fstat(fildes, &statbuf) != 0)
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          read_buffer = mmap(0, statbuf.st_size,PROT_READ,MAP_PRIVATE,fildes,0);
          read_size_init = statbuf.st_size;
          if(read_buffer == MAP_FAILED)
            {
              fprintf(stderr, "Could not map (read_binary_gate): %s\n", strerror(errno));
              read_mapped = 1;
              use_mmap_read = 0;
            }
          else
            {
              read_mapped = -1;
            }
        }

      if(read_mapped == -1)
        {
          tab = *((uint8_t*)((uint8_t*)read_buffer + pos*(sizeof(uint8_t) + 3 * sizeof(uint64_t))));
        }
      else
        {
          use_mmap_read = 0;
          read_binary_gate(binary, 
                           pos, 
                           _map, 
                           map_,
                           id, 
                           inputs, 
                           tab, 
                           output, 
                           party, 
                           arity);
        }
    }

  if((tab & 0xF0) == 0x80)
    {
      output = 1;
      party = 1;
      arity = 1;
    }
  else if((tab & 0xF0) == 0x60)
    {
      output = 1;
      party = 0;
      arity = 1;
    }
  else
    {
      output = 0;
      uint8_t q = tab & 0x0F;
      tab = ((q & 0x08) >> 3) | ((q & 0x04) >> 1) | ((q & 0x02) << 1) | ((q & 0x01) << 3);
      arity = 2;
    }

  if(use_mmap_read == 0)
    fread(&id, sizeof(uint64_t), 1, binary);
  else
    id = *((uint64_t*)((uint8_t*)read_buffer + pos*(sizeof(uint8_t)+3*sizeof(uint64_t)) + sizeof(uint8_t)));

  for(i = 0; i < arity; i++)
    {
      if(idx_size == sizeof(uint16_t))
        {
          uint16_t idx;
          fread(&idx, sizeof(uint16_t), 1, binary);
          inputs[i] = idx;
        }
      else if(idx_size == sizeof(uint32_t))
        {
          uint32_t idx;
          fread(&idx, sizeof(uint32_t), 1, binary);
          inputs[i] = idx;
        }
      else
        {
          if(use_mmap_read == 0)
            {
              uint64_t idx;
              fread(&idx, sizeof(uint64_t), 1, binary);
              inputs[i] = idx;
            }
          else
            inputs[i] = *((uint64_t*)((uint8_t*)read_buffer + (pos*(sizeof(uint8_t)+3*sizeof(uint64_t))) + sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint64_t)*i));
        }
    }
  if((_map != 0) || (map_ != 0))
    inputs[0] = map_lookup(_map, map_, inputs[0]);
  assert((id > inputs[0]) && (id > inputs[1]));
  if(arity > 1)
    {
      if((_map != 0) || (map_ != 0))
        inputs[1] = map_lookup(_map, map_, inputs[1]);
      assert((id > inputs[0]) && (id > inputs[1]));
      arity = 2;
    }
  else
    {
      inputs[1] = 0;
      arity = 1;
    }
  
}

int read_gate_old(const char* line, const std::map<uint64_t,uint64_t>& _map, uint64_t& id, uint64_t* inputs, char & tab, char & output, char & party, char & arity)
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
