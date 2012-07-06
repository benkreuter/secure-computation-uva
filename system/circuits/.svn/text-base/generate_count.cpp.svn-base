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

#include <map>
#include <string>
#include <vector>

using namespace std;


enum { ETC = 0, GEN_INP = 1, EVL_INP = 2, GEN_OUT = 3, EVL_OUT = 4};

uint64_t get_count(FILE * cnt, uint64_t id)
{
  uint64_t ret = 0;
  if(fseek(cnt, sizeof(uint64_t)*id, SEEK_SET) != 0) fprintf(stderr, "Problem seeking\n");
  if(fread(&ret, sizeof(uint64_t), 1, cnt) != 1) fprintf(stderr, "Problem reading %ld\n\n", id);
  //  if(id == 256) fprintf(stderr, "%ld\n", ret);
  return ret;
}

void inc_count(FILE * cnt, uint64_t id)
{
  uint64_t cur = get_count(cnt, id);
  cur++;
  fseek(cnt, sizeof(uint64_t)*id, SEEK_SET);
  fwrite(&cur, sizeof(uint64_t), 1, cnt);
  fflush(cnt);
}

void set_count(FILE * cnt, uint64_t id, uint64_t cur)
{
  fseek(cnt, sizeof(uint64_t)*id, SEEK_SET);
  fwrite(&cur, sizeof(uint64_t), 1, cnt);
  fflush(cnt);

}

int main(int argc, char ** argv)
{
  FILE *  input = fopen(argv[1], "rb");
  FILE * output = fopen(argv[2], "wb");
  FILE  * use_cnt = fopen(argv[3], "wb+");

  uint64_t gate_cnt;
  uint32_t gen_in, gen_out, evl_in, evl_out;
  uint64_t xxx;
  uint8_t dummy;

  // Read the header
  fread(&gate_cnt, sizeof(uint64_t), 1, input);
  fread(&gen_in, sizeof(uint32_t), 1, input);
  fread(&evl_in, sizeof(uint32_t), 1, input);
  fread(&gen_out, sizeof(uint32_t), 1, input);
  fread(&evl_out, sizeof(uint32_t), 1, input);
  fread(&dummy, sizeof(uint8_t), 1, input);

  // Read the gates.  For each gate, set the count in the count file.
  for(xxx = 0; xxx < gate_cnt; xxx++)
    {
      uint8_t tab;
      fread(&tab, sizeof(uint8_t), 1, input);
      uint8_t h;
      h = tab & 0xf0;

      set_count(use_cnt, xxx, 0); // Start with a zero count

      if((h == 0x40) || (h == 0x20)) // Input wire
        {
          uint64_t cnt;
          fread(&cnt, sizeof(uint64_t), 1, input);
          // Throwaway
        }
      else if(h == 0x10) // Middle gate
        {
          if(xxx <= UINT16_MAX)
            {
              uint16_t input0,input1;
              uint64_t cnt;
              fread(&input0, sizeof(uint16_t), 1, input);
              fread(&input1, sizeof(uint16_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);

	      assert((input0 <= gate_cnt) && (input1 <= gate_cnt));

              inc_count(use_cnt, input0);
              inc_count(use_cnt, input1);
            }
          else if(xxx <= UINT32_MAX)
            {
              uint32_t input0,input1;
              uint64_t cnt;
              fread(&input0, sizeof(uint32_t), 1, input);
              fread(&input1, sizeof(uint32_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);
	      assert((input0 <= gate_cnt) && (input1 <= gate_cnt));
              inc_count(use_cnt, input0);
              inc_count(use_cnt, input1);
            }
          else
            {
              uint64_t input0,input1;
              uint64_t cnt;
              fread(&input0, sizeof(uint64_t), 1, input);
              fread(&input1, sizeof(uint64_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);
	      assert((input0 <= gate_cnt) && (input1 <= gate_cnt));
              inc_count(use_cnt, input0);
              inc_count(use_cnt, input1);
            }
        }
      else
        {
          if(xxx <= UINT16_MAX)
            {
              uint16_t input0;
              uint64_t cnt;
              fread(&input0, sizeof(uint16_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);

              inc_count(use_cnt, input0);
            }
          else if(xxx <= UINT32_MAX)
            {
              uint32_t input0;
              uint64_t cnt;
              fread(&input0, sizeof(uint32_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);

              inc_count(use_cnt, input0);
            }
          else
            {
              uint64_t input0;
              uint64_t cnt;
              fread(&input0, sizeof(uint64_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);

              inc_count(use_cnt, input0);
            }
        }
    }

  // Now that the counts have been generated, rewind and rewrite the file
  fwrite(&gate_cnt, sizeof(uint64_t), 1, output);
  fwrite(&gen_in, sizeof(uint32_t), 1, output);
  fwrite(&evl_in, sizeof(uint32_t), 1, output);
  fwrite(&gen_out, sizeof(uint32_t), 1, output);
  fwrite(&evl_out, sizeof(uint32_t), 1, output);
  dummy = 8;
  fwrite(&dummy, sizeof(uint8_t), 1, output);
  fflush(output);

  fseek(input, 25, SEEK_SET);
  for(xxx = 0; xxx < gate_cnt; xxx++)
    {
      uint8_t tab;
      fread(&tab, sizeof(uint8_t), 1, input);
      uint8_t h;
      h = tab & 0xf0;

      fwrite(&tab, sizeof(uint8_t), 1, output);
      fflush(output);

      if((h == 0x40) || (h == 0x20)) // Input wire
        {
          uint64_t cnt;
          fread(&cnt, sizeof(uint64_t), 1, input);
          // Throwaway

          cnt = get_count(use_cnt, xxx);
          fwrite(&cnt, sizeof(uint64_t), 1, output);
          fflush(output);
        }
      else if(h == 0x10) // Middle gate
        {
          if(xxx <= UINT16_MAX)
            {
              uint16_t input0,input1;
              uint64_t cnt;
              fread(&input0, sizeof(uint16_t), 1, input);
              fread(&input1, sizeof(uint16_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);
              cnt = get_count(use_cnt, xxx);


              fwrite(&input0, sizeof(uint16_t), 1, output);
              fflush(output);
              fwrite(&input1, sizeof(uint16_t), 1, output);
              fflush(output);
              fwrite(&cnt, sizeof(uint64_t), 1, output);
              fflush(output);
            }
          else if(xxx <= UINT32_MAX)
            {
              uint32_t input0,input1;
              uint64_t cnt;
              fread(&input0, sizeof(uint32_t), 1, input);
              fread(&input1, sizeof(uint32_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);
              cnt = get_count(use_cnt, xxx);

              fwrite(&input0, sizeof(uint32_t), 1, output);
              fflush(output);
              fwrite(&input1, sizeof(uint32_t), 1, output);
              fflush(output);
              fwrite(&cnt, sizeof(uint64_t), 1, output);
              fflush(output);
            }
          else
            {
              uint64_t input0,input1;
              uint64_t cnt;
              fread(&input0, sizeof(uint64_t), 1, input);
              fread(&input1, sizeof(uint64_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);
              cnt = get_count(use_cnt, xxx);

              fwrite(&input0, sizeof(uint64_t), 1, output);
              fflush(output);
              fwrite(&input1, sizeof(uint64_t), 1, output);
              fflush(output);
              fwrite(&cnt, sizeof(uint64_t), 1, output);
              fflush(output);

            }
        }
      else
        {
          if(xxx <= UINT16_MAX)
            {
              uint16_t input0;
              uint64_t cnt;
              fread(&input0, sizeof(uint16_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);

              cnt = 1;
              fwrite(&input0, sizeof(uint16_t), 1, output);
              fflush(output);
              fwrite(&cnt, sizeof(uint64_t), 1, output);
              fflush(output);

            }
          else if(xxx <= UINT32_MAX)
            {
              uint32_t input0;
              uint64_t cnt;
              fread(&input0, sizeof(uint32_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);

              cnt = 1;
              fwrite(&input0, sizeof(uint32_t), 1, output);
              fflush(output);
              fwrite(&cnt, sizeof(uint64_t), 1, output);
              fflush(output);


            }
          else
            {
              uint64_t input0;
              uint64_t cnt;
              fread(&input0, sizeof(uint64_t), 1, input);
              fread(&cnt, sizeof(uint64_t), 1, input);
              cnt = 1;
              fwrite(&input0, sizeof(uint64_t), 1, output);
              fflush(output);
              fwrite(&cnt, sizeof(uint64_t), 1, output);
              fflush(output);

            }
        }
    }
  fclose(input);
  fclose(output);
  fclose(use_cnt);
  return 0;
}
