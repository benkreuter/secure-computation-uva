/*
 * Bytes.h
 *
 *  Created on: Feb 28, 2011
 *      Author: shench
 */

#ifndef BYTES_H_
#define BYTES_H_


#include <stdint.h>
#include <cassert>
#include <algorithm>
#include <string>
#include <vector>


typedef uint8_t byte;


class Bytes : public std::vector<byte>
{
	struct _Xor
	{
		byte operator() (byte lhs, byte rhs) const { return lhs ^ rhs; }
	};

public:
	Bytes() : std::vector<byte>(0) {}
	Bytes(size_t n) : std::vector<byte>(n) {}
	Bytes(size_t n, byte b) : std::vector<byte>(n, b) {}
	Bytes(byte *beg, byte *end) : std::vector<byte>(beg, end) {}
	Bytes(const_iterator beg, const_iterator end) : std::vector<byte>(beg, end) {}
	Bytes(const std::vector<Bytes> &chunks) { merge(chunks); }

	const Bytes &operator +=(const Bytes &rhs)
	{
		this->insert(this->end(), rhs.begin(), rhs.end());
		return *this;
	}

	const Bytes &operator ^=(const Bytes &rhs)
	{
		assert(rhs.size() == size());
		std::transform(this->begin(), this->end(), rhs.begin(), this->begin(), _Xor());
		return *this;
	}

	byte get_ith_bit(size_t ix) const
	{
		assert(ix < size()*8);
		return (this->at(ix/8) >> (ix%8)) & 0x01;
	}

	void set_ith_bit(size_t ix, byte bit)
	{
		assert(ix < size()*8);
		static const byte INVERSE_MASK[8] =
			{ 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

		this->at(ix/8) &= INVERSE_MASK[ix%8];
		this->at(ix/8) |= (bit&0x01) << (ix%8);
	}

	std::string to_hex() const;
	void from_hex(const std::string &s);

	Bytes hash(size_t bits) const;
	std::vector<Bytes> split(const size_t chunk_len) const;
	void merge(const std::vector<Bytes> &chunks);


	static int cnt;
};

// pre-condition: lhs.size() == rhs.size()
inline Bytes operator^ (const Bytes &lhs, const Bytes &rhs)
{
	assert(lhs.size() == rhs.size());
	Bytes ret(lhs);
	ret ^= rhs;
	return ret;
}

inline Bytes operator+ (const Bytes &lhs, const Bytes &rhs)
{
	Bytes ret(lhs);
	ret += rhs;
	return ret;
}

inline bool operator ==(const Bytes &lhs, const Bytes &rhs)
{
	return (lhs.size() != rhs.size())?
		false : std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

#endif /* BYTES_H_ */
