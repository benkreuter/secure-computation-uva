#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <string>

#include "Bytes.h"

inline void printHex(uint8_t m[16])
{
	for (int i = 0; i < 16; i++)
		std::cout << std::setw(4) << std::hex << static_cast<int>(m[i]);
	std::cout << std::endl;
}

inline void print(uint8_t m[16])
{
    printHex(m);
//    for (int i = 0; i < 16; i++)
//        std::cout << std::setw(4) << std::dec << static_cast<int>(m[i]);
//	std::cout << std::endl;
}

uint8_t min(uint8_t a, uint8_t b)
{
    return a < b? a : b;
}


uint8_t min(uint8_t a, uint8_t b, uint8_t c)
{
    return min(min(a, b), c);
}

int main(int argc, char *argv[])
{
    std::ifstream private_file(argv[1]);
	std::string input;

    Bytes s1, s2;

	if (!private_file.is_open())
	{
		std::cerr << "input file open failure: " << argv[1] << std::endl;
		exit(EXIT_FAILURE);
	}

    private_file >> input;
    s1.from_hex(input);

    private_file >> input;
    s2.from_hex(input);

    char *endptr;
    int bit_len = strtol(argv[2], &endptr, 10);

    s1.resize((bit_len+7)/8);
    s2.resize((bit_len+7)/8);

    std::cout << "s1: " << s1.to_hex() << std::endl;
    std::cout << "s2: " << s2.to_hex() << std::endl;

    std::vector<int> curr_row, prev_row;
    curr_row.resize(bit_len);

    int d, l, u;

    for (int j = 0; j < bit_len; j++) curr_row[j] = j+1;

    for (int i = 0; i < bit_len; i++)
    {
        prev_row.assign(bit_len, 0);
        curr_row.swap(prev_row);

        d = i;
        l = i + 1;
        u = prev_row[0];

        curr_row[0] = (s1.get_ith_bit(i) == s2.get_ith_bit(0))? d : (min(l, d, u) + 1);

        for (int j = 1; j < bit_len; j++)
        {
            d = u;
            u = prev_row[  j];
            l = curr_row[j-1];
            
            curr_row[j] = (s1.get_ith_bit(i) == s2.get_ith_bit(j))? d : (min(l, d, u) + 1);
        }
    }
    
    std::cout << curr_row.back() << std::endl;

    return 0;
}
