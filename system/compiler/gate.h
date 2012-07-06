#ifndef __GATE_H
#define __GATE_H

#include <stdint.h>
#include <limits.h>
#include <stdio.h>

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

extern struct gate_list * circuit, * last;
extern uint64_t window, inputs;
extern FILE * map_file;

#endif
