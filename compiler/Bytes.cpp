/*
 * Bytes.cpp
 *
 *  Created on: Oct 6, 2011
 *      Author: shench
 */

//#include <log4cxx/logger.h>
#include <iostream>
#include <openssl/sha.h>

#include "Bytes.h"

//static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("Bytes.cpp"));

static byte MASK[8] =
{
	0xFF, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F
};

int Bytes::cnt = 0;

Bytes Bytes::hash(size_t bits) const
{
	assert((0 < bits) && (bits <= SHA256_DIGEST_LENGTH*8));

	byte buf[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, &((*this)[0]), this->size());
	SHA256_Final(buf, &sha256);

	Bytes hash(buf, buf + (bits+7)/8);
	hash.back() &= MASK[bits % 8]; // clear the extra bits

	cnt++;

	return hash;
}

static const char HEX_TABLE[16] =
{
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

std::string Bytes::to_hex() const
{
	std::string out;
	out.reserve(this->size()*2);

	for (const_iterator it = this->begin(); it != this->end(); it++)
	{
		out.push_back(HEX_TABLE[*it/16]);
		out.push_back(HEX_TABLE[*it%16]);
	}
	return out;
}

static const int REVERSE_HEX_TABLE[256] =
{
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static const int HEX_EXP[2] = { 16, 1 };

void Bytes::from_hex(const std::string &s)
{
	this->clear();
	this->resize((s.size()+1)/2, 0); // zero out this

	for (size_t ix = 0; ix < s.size(); ix++)
	{
		if (REVERSE_HEX_TABLE[s[ix]] == -1)
		{
            std::cerr << "Invalid hex format: " << s;
		}
		(*this)[ix/2] += REVERSE_HEX_TABLE[s[ix]]*HEX_EXP[ix%2];
	}
}

std::vector<Bytes> Bytes::split(const size_t chunk_len) const
{
	assert(this->size() % chunk_len == 0);
	std::vector<Bytes> ret(this->size()/chunk_len);

	const_iterator it = this->begin();
	for (int ix = 0; ix < ret.size(); ix++, it+= chunk_len)
		ret[ix].insert(ret[ix].end(), it, it+chunk_len);

	return ret;
}

void Bytes::merge(const std::vector<Bytes> &chunks)
{
	this->clear();
	this->reserve(chunks.size()*chunks[0].size());

	std::vector<Bytes>::const_iterator it;
	for (it = chunks.begin(); it != chunks.end(); it++) *this += *it;
}

