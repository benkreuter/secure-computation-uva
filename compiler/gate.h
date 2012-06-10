#ifndef __GATE_H
#define __GATE_H

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <db.h>
#include <sys/types.h>

struct gate
{
  uint64_t id;
  char arity;
  char truth_table;
  uint64_t * inputs;
  char output, party;
  //  char * comment;
};

void putint(uint64_t);
void print_gate(struct gate *);

extern uint64_t total_emitted_gates;

struct gate_list
{
  struct gate * data;
  struct gate_list * next;
};

void print_gate_list(struct gate_list *);
void add_gate(uint64_t id, uint64_t arity, char truble_table, uint64_t * inputs, int output, char * comment);

void remove_redundant_gates(struct gate_list *);
void remove_identity_gates(struct gate_list * list);

void emit_gate(char arity, uint64_t * inputs, char t, uint64_t id, char output, char party);

void write_binary_gate(FILE * binary,uint64_t id, uint64_t *inputs, char tab, char output, char party, char arity);
void map_file_print(uint64_t old, uint64_t _new);

int check_store_type(uint8_t);

void   init_maps();

extern struct gate_list * circuit, * last;
extern uint64_t window, inputs;
//extern FILE * map_file;
//extern GDBM_FILE map_file;
extern DB * map_file;
extern FILE * store_file;
extern uint8_t store_type;
extern off_t max_map_items;
extern uint8_t use_mmap_io;
extern void * cir_buffer;
extern uint64_t cir_size_init;
extern int8_t cir_mapped;

extern void ** maps;
extern int8_t * mappeds;
extern uint64_t * map_size_inits;

#endif
