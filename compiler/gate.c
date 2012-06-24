#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <search.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gate.h"
#include <gdbm.h>

int offset = 0;
uint8_t store_type;

uint64_t total_emitted_gates = 0;
uint64_t inputs = 0;

uint64_t xor = 0, nxor = 0;

FILE * binary_file;
FILE * store_file;
DB* map_file;
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
  write_binary_gate(binary_file, g->id, g->inputs, g->truth_table, g->output, g->party, g->arity);

  line++;
  total_emitted_gates++;
}

enum { ETC = 0, GEN_INP = 1, EVL_INP = 2, GEN_OUT = 3, EVL_OUT = 4};


void * cir_buffer = 0;
int8_t cir_mapped = 0;
uint64_t cir_size_init = 0;
uint8_t use_mmap_io = 0;
uint64_t map_cnt = 0;

void write_binary_gate(FILE * binary,uint64_t id, uint64_t *inputs, char tab, char output, char party, char arity)
{
  //  uint8_t idx_size = 0;
  uint64_t i = 0;
  /* Ugly ugly ugly ugly ugly */
  //  idx_size = sizeof(uint64_t);
  if(id <= inputs[1]) fprintf(stderr, "%ld %ld %hhd\n", id, inputs[1], output);
  assert(id > inputs[1]);

  if(tab == 6) xor++;
  else nxor++;
  if(output != 0)
    {
      tab = 0x02;
      if(party == 1)
        tab |= 0x80;
      else
        tab |= 0x60;
      inputs[1] = 0;
    }
  else
    {

      // Reverse the tab
      uint8_t q = tab & 0x0F;
      tab = ((q & 0x08) >> 3) | ((q & 0x04) >> 1) | ((q & 0x02) << 1) | ((q & 0x01) << 3);
      tab |= 0x10;
    }

  if(use_mmap_io == 0)
    {
      fwrite(&tab, sizeof(uint8_t), 1, binary);
      fflush(binary);
      
      fwrite(&id, sizeof(uint64_t), 1, binary);
      fflush(binary);

      for(i = 0; i < 2; i++)
        {           
          uint64_t idx = inputs[i];
          fwrite(&idx,sizeof(uint64_t), 1, binary);
          fflush(binary);
          
        }
    }
  else
    {
      int filedes = fileno(binary);
      // Use memory-mapped IO
      /* It is safe to assume that we will not read and write the same
         file at the same time. */
//#pragma message "When using memory-mapped IO, it is critical to ensure that you are not reading and writing the same circuit file at the same time."
      if(cir_mapped == 0)
        {
          struct stat statbuf;
          if(ftruncate(filedes,(sizeof(uint8_t)+3*sizeof(uint64_t))*1) < 0)
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          if(fstat(filedes, &statbuf) != 0)
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          if(statbuf.st_size > cir_size_init)
            {
              cir_size_init = statbuf.st_size;
            }
          if(statbuf.st_size <= 0)
            {
              cir_size_init = sizeof(uint8_t)+3*sizeof(uint64_t);
            }

          //          fprintf(stderr, "Initializing map %ld\n", cir_size_init);
          cir_buffer = mmap(0, cir_size_init,PROT_READ|PROT_WRITE,MAP_SHARED,filedes,0);
          cir_mapped = 0;
          if(cir_buffer == MAP_FAILED)
            {
              fprintf(stderr, "Could not map (write_binary_gate): %s\n", strerror(errno));
              cir_mapped = 1;
              use_mmap_io = 0;
            }
          else
            {
              map_cnt = 0;
              cir_mapped = -1;
            }
        }

      if(cir_mapped == 1)
        {
          use_mmap_io = 0;
          fprintf(stderr, "Could not map??\n");
          write_binary_gate(binary,id, inputs, tab, output, party, arity);
        }
      else 
        {
          if(map_cnt*(sizeof(uint8_t)+3*sizeof(uint64_t)) >= cir_size_init)
            {
              // Need to grow the file
              //              fprintf(stderr, "Growing file %ld\n", cir_size_init*2);
              if(ftruncate(filedes,cir_size_init*2) < 0)
                {
                  fprintf(stderr, "Cannot grow file: %s\n", strerror(errno));
                  abort();
                }
              cir_buffer = mremap(cir_buffer, cir_size_init, cir_size_init*2, MREMAP_MAYMOVE);
              if(cir_buffer == MAP_FAILED)
                {
                  fprintf(stderr, "Unable to remap: %s\n", strerror(errno));
                  abort();
                }
              cir_size_init = cir_size_init*2; //(map_cnt+1)*(sizeof(uint8_t)+3*sizeof(uint64_t))*2;
            }
          ((uint8_t*)cir_buffer)[map_cnt*(sizeof(uint8_t)+3*sizeof(uint64_t))] = tab;
          *((uint64_t*)((cir_buffer + map_cnt*(sizeof(uint8_t)+3*sizeof(uint64_t)))+sizeof(uint8_t))) = id;
          *((uint64_t*)((cir_buffer + map_cnt*(sizeof(uint8_t)+3*sizeof(uint64_t)))+sizeof(uint8_t)+sizeof(uint64_t))) = inputs[0];
          *((uint64_t*)((cir_buffer + map_cnt*(sizeof(uint8_t)+3*sizeof(uint64_t)))+sizeof(uint8_t)+2*sizeof(uint64_t))) = inputs[1];
          map_cnt++;
        } 
    }
}


void print_gate_list(struct gate_list * list)
{
  struct gate_list * cur = list;
  while(cur != 0)
    {
      print_gate(cur->data);
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

void ** maps;
int8_t* mappeds;
uint64_t* map_size_inits;

// This is essentially the maximum size of our cache.  Thus if we can
// use STL maps to compile big circuits on systems with less RAM.
off_t max_map_items = 10000000;

void init_maps()
{
  int i;
  mappeds = (int8_t*)malloc(128*sizeof(uint8_t));
  if(mappeds == 0) abort();
  map_size_inits = (uint64_t*)malloc(128*sizeof(uint64_t));
  if(map_size_inits == 0) abort();
  maps = (void**)malloc(128*sizeof(void*));
  if(maps == 0) abort();
  for(i = 0; i < 128; i++)
    {
      mappeds[i] = 0;
      map_size_inits[i] = 0;
      maps[i] = MAP_FAILED;
    }
}

void map_file_print(uint64_t old, uint64_t new)
{
  //  fprintf(map_file, "%lu %lu\n", old, new);
  //  datum key, val;
  if(store_type == 0)
    {
      DBT key, val;

      memset(&key, 0, sizeof(DBT));
      memset(&val, 0, sizeof(DBT));

      key.data = (char*)(&old);
      key.size = sizeof(uint64_t);

      val.data = (char*)(&new);
      val.size = sizeof(uint64_t);

      //  gdbm_store(map_file, key, val, GDBM_REPLACE);
      map_file->put(map_file, NULL, &key, &val, 0);
    }
  else if((store_type == 1) || (store_type == 2))
    {
      fseek(store_file, (sizeof(uint8_t) + sizeof(uint64_t))*old, SEEK_SET);
      uint8_t written = -1;
      fwrite(&written, sizeof(uint8_t), 1, store_file);
      //      fwrite(&old, sizeof(uint64_t), 1, store_file);
      fwrite(&new, sizeof(uint64_t), 1, store_file);
      fflush(store_file);
    }
  else if(store_type == 3)
    {
      // Initialize memory-mapped IO if needed
      int fildes = fileno(store_file);
      if(fildes < 0)
        abort();
      // Use memory mapped IO if possible.
      if(mappeds[fildes] == 0)
        {
          //          fprintf(stderr, "Initializing map %d for write\n", fildes);
          // We need to make sure the file is long enough for us to
          // always use.

          struct stat statbuf;
          if(fstat(fildes,&statbuf) != 0)
            {
              fprintf(stderr, "%s\n", strerror(errno));
              abort();
            }
          fprintf(stderr, "Size: %lu\n", statbuf.st_size);
          maps[fildes] = 0;
          if(statbuf.st_size > max_map_items*(sizeof(uint8_t) + sizeof(uint64_t)))
            {
              maps[fildes] = mmap(0,statbuf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,fildes,0);
              map_size_inits[fildes] = statbuf.st_size;
            }
          else
            {
              if(ftruncate(fildes, max_map_items * (sizeof(uint8_t) + sizeof(uint64_t))) < 0)
                {
                  fprintf(stderr, "%s\n", strerror(errno));
                  abort();
                }
              map_size_inits[fildes] = max_map_items*(sizeof(uint8_t)+sizeof(uint64_t));
              maps[fildes] = mmap(0, map_size_inits[fildes], PROT_READ|PROT_WRITE,MAP_SHARED,fildes,0);
              //              fprintf(stderr, "%lx\n", (uint64_t)maps[fildes]);
            }
          mappeds[fildes] = 0;
          if(maps[fildes] == MAP_FAILED)
            {
              mappeds[fildes] = 1;
              store_type = 1;
              fprintf(stderr, "Could not map (map_file_print): %s\n", strerror(errno));
            }
          else
            {
              mappeds[fildes] = -1;   
              fprintf(stderr, "Map created!\n");
            }
        }

      if(mappeds[fildes] == -1)
        {
          uint64_t new_pos = old*(sizeof(uint8_t)+sizeof(uint64_t));
          if(new_pos >= map_size_inits[fildes])
            {
              if(ftruncate(fildes,new_pos * 2) == -1)
                {
                  fprintf(stderr, "Cannot grow file: %s\n", strerror(errno));
                  abort();
                }
              maps[fildes] = mremap(maps[fildes], map_size_inits[fildes], new_pos*2, MREMAP_MAYMOVE);
              if(maps[fildes] == MAP_FAILED)
                {
                  fprintf(stderr, "Cannot remap: %s\n", strerror(errno));
                  abort();
                }
              map_size_inits[fildes] = new_pos*2;//map_size_inits[fildes]*10;//old*(sizeof(uint8_t)+sizeof(uint64_t))*10;
            }
          ((uint8_t*)(maps[fildes]))[old*(sizeof(uint8_t)+sizeof(uint64_t))] = -1;
          *((uint64_t*)((maps[fildes])+old*(sizeof(uint8_t)+sizeof(uint64_t)) + sizeof(uint8_t))) = new;
        }
      else if(mappeds[fildes] == 1)
        {
          // Cannot use memory map for some reason!
          fprintf(stderr, "Unable to map???\n");
          if(mappeds[fildes] != 1) abort();
          //store_type = 1;
          map_file_print(old, new);
        }
      else
        abort();

    }
  else
    {
      abort();
    }
}

int check_store_type(uint8_t s)
{
  if((s != 0) && (s != 1) && (s != 2) && (s != 3))
    return -1;
  else
    return 0;
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
  if(output != 0) newgate->inputs[1] = 0;
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
          //          fprintf(stderr, "Gate %s not found (%lu)\n",gate_desc, tmp->data->id);
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
          //          fprintf(stderr, "Gate %s found: %lu\n",ent.key,ret->data);
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
