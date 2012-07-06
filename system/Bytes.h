#ifndef BYTES_H_
#define BYTES_H_

#include <stdint.h>
#include <algorithm>
#include <cassert>
#include <string>
#include <vector>


typedef uint8_t byte;


class Bytes : public std::vector<byte>
{
	struct bitwise_xor{ byte operator() (byte l, byte r) const { return l ^ r;  }};

public:
	Bytes() {}
	Bytes(uint64_t n) : std::vector<byte>(n) {}
	Bytes(uint64_t n, byte b) : std::vector<byte>(n, b) {}
	Bytes(byte *begin, byte *end) : std::vector<byte>(begin, end) {}
	Bytes(const_iterator begin, const_iterator end) : std::vector<byte>(begin, end) {}
	Bytes(const std::vector<Bytes> &chunks) { merge(chunks); }

	const Bytes &operator =(const Bytes &rhs)
	{
		this->assign(rhs.begin(), rhs.end());
		return *this;
	}

	const Bytes &operator +=(const Bytes &rhs)
	{
		this->insert(this->end(), rhs.begin(), rhs.end());
		return *this;
	}

	const Bytes &operator ^=(const Bytes &rhs)
	{
		// TODO: see if this can be improved by a wider data type pointer
		assert(rhs.size() == size());
		Bytes::iterator dst = this->begin();
		Bytes::const_iterator src = rhs.begin();
		while (dst != this->end()) { *dst++ ^= *src++;}
		return *this;
	}

	byte get_ith_bit(size_t ix) const
	{
		assert(ix < size()*8);
		return ((*this)[ix/8] >> (ix%8)) & 0x01;
	}

	void set_ith_bit(size_t ix, byte bit)
	{
		assert(ix < size()*8);
		static const byte INVERSE_MASK[8] =
			{ 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

		(*this)[ix/8] &= INVERSE_MASK[ix%8];
		(*this)[ix/8] |= (bit&0x01) << (ix%8);
	}

	std::string to_hex() const;
	void from_hex(const std::string &s);

	Bytes hash(size_t bits) const;
	std::vector<Bytes> split(const size_t chunk_len) const;
	void merge(const std::vector<Bytes> &chunks);
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
