#include "GarbledCct.h"

void GarbledCct::gen_init(const vector<Bytes> &ot_keys, const Bytes &gen_inp_mask, const Bytes &seed)
{
	m_ot_keys = &ot_keys;
	m_gen_inp_mask = gen_inp_mask;
	m_prng.srand(seed);

	// R is a random k-bit string whose left-most bit has to be 1
	Bytes tmp = m_prng.rand(Env::k());
	tmp.set_ith_bit(0, 1);
	tmp.resize(16, 0);
	m_128i_R = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&tmp[0]));

	m_gate_ix = 0;

	m_gen_inp_ix = 0;
	m_evl_inp_ix = 0;
	m_gen_out_ix = 0;
	m_evl_out_ix = 0;

	m_o_bufr.clear();

	if (m_w == 0)
	{
		m_w = new __m128i[Env::circuit().m_cnt];
	}

	tmp.assign(16, 0);
	for (size_t ix = 0; ix < Env::k(); ix++) tmp.set_ith_bit(ix, 1);
	m_out_mask = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

    m_m.resize(Env::circuit().gen_inp_cnt()*2);
	m_M.resize(Env::circuit().gen_inp_cnt()*2);

	Z m0, m1;

	// init group elements associated with the generator's input bits
	for (size_t ix = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
	{
		m0.random(m_prng);
		m1.random(m_prng);

		m_m[2*ix+0] = m0;
		m_m[2*ix+1] = m1;

		m_M[2*ix+0] = Env::clawfree().F(0, m0);
		m_M[2*ix+1] = Env::clawfree().F(1, m1);
	}
}


const int CIRCUIT_HASH_BUFFER_SIZE = 1048576;


void GarbledCct::com_init(const vector<Bytes> &ot_keys, const Bytes &gen_inp_mask, const Bytes &seed)
{
	gen_init(ot_keys, gen_inp_mask, seed);
	m_hash.reserve(CIRCUIT_HASH_BUFFER_SIZE+64);
	m_hash.clear();
}


void GarbledCct::evl_init(const vector<Bytes> &ot_keys, const Bytes &masked_gen_inp, const Bytes &evl_inp)
{
	m_ot_keys = &ot_keys;
	m_gen_inp_mask = masked_gen_inp;
	m_evl_inp = evl_inp;

	m_C.resize(Env::circuit().gen_out_cnt()*2);

	m_evl_out.resize((Env::circuit().evl_out_cnt()+7)/8);
	m_gen_out.resize((Env::circuit().gen_out_cnt()+7)/8);

	m_gate_ix = 0;

	m_gen_inp_ix = 0;
	m_evl_inp_ix = 0;
	m_gen_out_ix = 0;
	m_evl_out_ix = 0;

	m_i_bufr.clear();

	if (m_w == 0)
	{
		m_w = new __m128i[Env::circuit().m_cnt];
	}
	Bytes tmp(16, 0);
	for (size_t ix = 0; ix < Env::k(); ix++) tmp.set_ith_bit(ix, 1);
	m_out_mask = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

	m_hash.clear();
}


void GarbledCct::gen_next_gate(const Gate &current_gate)
{
	__m128i zero_key, a[2];
	static Bytes tmp;

	if (current_gate.m_tag == Circuit::GEN_INP)
	{
		// zero_key = m_prng.rand(Env::k());
		tmp = m_prng.rand(Env::k());
		tmp.resize(16, 0);
		zero_key = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		// a[0] = m_M[2*m_gen_inp_ix+0].to_bytes().hash(Env::k());
		tmp = m_M[2*m_gen_inp_ix+0].to_bytes().hash(Env::k());
		tmp.resize(16, 0);
		a[0] = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		// a[1] = m_M[2*m_gen_inp_ix+1].to_bytes().hash(Env::k());
		tmp = m_M[2*m_gen_inp_ix+1].to_bytes().hash(Env::k());
		tmp.resize(16, 0);
		a[1] = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		// a[0] ^= zero_key; a[1] ^= zero_key ^ R;
		a[0] = _mm_xor_si128(a[0], zero_key);
		a[1] = _mm_xor_si128(a[1], _mm_xor_si128(zero_key, m_128i_R));

		uint8_t bit = m_gen_inp_mask.get_ith_bit(m_gen_inp_ix);

		// m_o_bufr += a[bit];
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), a[bit]);
		m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());

		// m_o_bufr += a[1-bit];
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), a[1-bit]);
		m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());

		m_gen_inp_ix++;
	}
	else if (current_gate.m_tag == Circuit::EVL_INP)
	{
		// zero_key = m_prng.rand(Env::k());
		tmp = m_prng.rand(Env::k());
		tmp.resize(16, 0);
		zero_key = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		// a[0] = (*m_ot_keys)[2*m_evl_inp_ix+0];
		tmp = (*m_ot_keys)[2*m_evl_inp_ix+0];
		tmp.resize(16, 0);
		a[0] = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		// a[1] = (*m_ot_keys)[2*m_evl_inp_ix+1];
		tmp = (*m_ot_keys)[2*m_evl_inp_ix+1];
		tmp.resize(16, 0);
		a[1] = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		// a[0] ^= zero_key; a[1] ^= zero_key ^ R;
		a[0] = _mm_xor_si128(a[0], zero_key);
		a[1] = _mm_xor_si128(a[1], _mm_xor_si128(zero_key, m_128i_R));

		// m_o_bufr += a[0];
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), a[0]);
		m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());

		// m_o_bufr += a[1];
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), a[1]);
		m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());

		m_evl_inp_ix++;
	}
	else
	{
		const vector<uint64_t> &input = current_gate.m_input;
		assert(input.size() == 1 || input.size() == 2);

#ifdef FREE_XOR
		if (is_xor(current_gate))
		{
			zero_key = input.size() == 2?
				_mm_xor_si128(m_w[input[0]], m_w[input[1]]) : _mm_load_si128(m_w+input[0]);
		}
		else
#endif
		if (input.size() == 2) // 2-arity gates
		{
			uint8_t bit;
			__m128i key[2], in, out;
			__m128i X[2], Y[2], Z[2];

			in = _mm_set1_epi64x(m_gate_ix);

			X[0] = _mm_load_si128(m_w+input[0]);
			Y[0] = _mm_load_si128(m_w+input[1]);

			X[1] = _mm_xor_si128(X[0], m_128i_R);
			Y[1] = _mm_xor_si128(Y[0], m_128i_R);

			const uint8_t mask_bit_0 = _mm_extract_epi8(X[0], 0) & 0x01;
			const uint8_t mask_bit_1 = _mm_extract_epi8(Y[0], 0) & 0x01;
			const uint8_t mask_ix = (mask_bit_1<<1)|mask_bit_0;

			// encrypt the 0-th entry
			key[0] = _mm_load_si128(X+mask_bit_0);
			key[1] = _mm_load_si128(Y+mask_bit_1);
			KDF256((uint8_t*)&in, (uint8_t*)&out, (uint8_t*)key);
			out = _mm_and_si128(out, m_out_mask); // clear extra bits
			bit = current_gate.m_table[mask_ix];

#ifdef GRR
			// GRR technique: using zero entry's key as one of the output keys
			_mm_store_si128(Z+bit, out);
			Z[1-bit] = _mm_xor_si128(Z[bit], m_128i_R);
			zero_key = _mm_load_si128(Z);
#else
			tmp = m_prng.rand(Env::k());
			tmp.resize(16, 0);
			Z[0] = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));
			Z[1] = _mm_xor_si128(Z[0], m_128i_R);

			out = _mm_xor_si128(out, Z[bit]);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), out);
			m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());
#endif

			tmp.resize(16, 0);

			// encrypt the 1st entry
			key[0] = _mm_load_si128(X+(0x01^mask_bit_0));
			KDF256((uint8_t*)&in, (uint8_t*)&out, (uint8_t*)key);
			out = _mm_and_si128(out, m_out_mask);
			bit = current_gate.m_table[0x01^mask_ix];

			out = _mm_xor_si128(out, Z[bit]);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), out);
			m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());

			// encrypt the 2nd entry
			key[0] = _mm_load_si128(X+(0x00^mask_bit_0));
			key[1] = _mm_load_si128(Y+(0x01^mask_bit_1));
			KDF256((uint8_t*)&in, (uint8_t*)&out, (uint8_t*)key);
			out = _mm_and_si128(out, m_out_mask);
			bit = current_gate.m_table[0x02^mask_ix];

			out = _mm_xor_si128(out, Z[bit]);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), out);
			m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());

			// encrypt the 3rd entry
			key[0] = _mm_load_si128(X+(0x01^mask_bit_0));
			KDF256((uint8_t*)&in, (uint8_t*)&out, (uint8_t*)key);
			out = _mm_and_si128(out, m_out_mask);
			bit = current_gate.m_table[0x03^mask_ix];

			out = _mm_xor_si128(out, Z[bit]);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), out);
			m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());
		}
		else // 1-arity gates
		{
			__m128i key, idx, out;
			__m128i X[2], Z[2];

			uint8_t bit;

			idx = _mm_set1_epi64x(m_gate_ix);

			X[0] = _mm_load_si128(m_w+input[0]);
			X[1] = _mm_xor_si128(X[0], m_128i_R);

			const uint8_t mask_bit = _mm_extract_epi8(X[0], 0) & 0x01;

			// 0-th entry
			key = _mm_load_si128(X+mask_bit);
			KDF128((uint8_t*)&idx, (uint8_t*)&out, (uint8_t*)&key);
			out = _mm_and_si128(out, m_out_mask);
			bit = current_gate.m_table[mask_bit];

#ifdef GRR
			_mm_store_si128(Z+bit, out);
			Z[1-bit] = _mm_xor_si128(Z[bit], m_128i_R);
			zero_key = _mm_load_si128(Z);
#else
			tmp = m_prng.rand(Env::k());
			tmp.resize(16, 0);
			Z[0] = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));
			Z[1] = _mm_xor_si128(Z[0], m_128i_R);

			out = _mm_xor_si128(out, Z[bit]);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), out);
			m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());
#endif

			// 1-st entry
			key = _mm_load_si128(X+(0x01^mask_bit));
			KDF128((uint8_t*)&idx, (uint8_t*)&out, (uint8_t*)&key);
			out = _mm_and_si128(out, m_out_mask);
			bit = current_gate.m_table[0x01^mask_bit];

			out = _mm_xor_si128(out, Z[bit]);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(&tmp[0]), out);
			m_o_bufr.insert(m_o_bufr.end(), tmp.begin(), tmp.begin()+Env::key_size_in_bytes());
		}

		if (current_gate.m_tag == Circuit::EVL_OUT)
		{
			m_o_bufr.push_back(_mm_extract_epi8(zero_key, 0) & 0x01); // permutation bit
		}
		else if (current_gate.m_tag == Circuit::GEN_OUT)
		{
			m_o_bufr.push_back(_mm_extract_epi8(zero_key, 0) & 0x01); // permutation bit

//			// TODO: C[ix_0] = w[ix0] || randomness, C[ix_1] = w[ix1] || randomness
//			m_o_bufr += (key_pair[0] + m_prng.rand(Env::k())).hash(Env::k());
//			m_o_bufr += (key_pair[1] + m_prng.rand(Env::k())).hash(Env::k());
		}
	}

	_mm_store_si128(m_w+current_gate.m_ref_cnt, zero_key);

	m_gate_ix++;
}

void GarbledCct::update_hash(const Bytes &data)
{
	m_hash += data;

#ifdef RAND_SEED
	if (m_hash.size() > CIRCUIT_HASH_BUFFER_SIZE) // hash the circuit by 1GB chunks
	{
		Bytes temperary_hash = m_hash.hash(Env::k());
		m_hash.clear();
		m_hash += temperary_hash;
	}
#endif
}

void GarbledCct::com_next_gate(const Gate &current_gate)
{
	gen_next_gate(current_gate);
	update_hash(m_o_bufr);
	m_o_bufr.clear(); // flush the output buffer
}


void GarbledCct::evl_next_gate(const Gate &current_gate)
{
	__m128i current_key, a;
	Bytes::const_iterator it;
	static Bytes tmp;

	if (current_gate.m_tag == Circuit::GEN_INP)
	{
		uint8_t bit = m_gen_inp_mask.get_ith_bit(m_gen_inp_ix);
		Bytes::iterator it = m_i_bufr_ix + bit*Env::key_size_in_bytes();

		tmp = m_M[m_gen_inp_ix].to_bytes().hash(Env::k());
		tmp.resize(16, 0);
		current_key = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		tmp.assign(it, it+Env::key_size_in_bytes());
		tmp.resize(16, 0);
		a = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		m_i_bufr_ix += Env::key_size_in_bytes()*2;

		current_key = _mm_xor_si128(current_key, a);

		m_gen_inp_ix++;
	}
	else if (current_gate.m_tag == Circuit::EVL_INP)
	{
		uint8_t bit = m_evl_inp.get_ith_bit(m_evl_inp_ix);
		Bytes::iterator it = m_i_bufr_ix + bit*Env::key_size_in_bytes();

		tmp = (*m_ot_keys)[m_evl_inp_ix];
		tmp.resize(16, 0);
		current_key = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		tmp.assign(it, it+Env::key_size_in_bytes());
		tmp.resize(16, 0);
		a = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));

		m_i_bufr_ix += Env::key_size_in_bytes()*2;

		current_key = _mm_xor_si128(current_key, a);

		m_evl_inp_ix++;
	}
	else
	{
        const vector<uint64_t> &input = current_gate.m_input;

#ifdef FREE_XOR
		if (is_xor(current_gate))
		{
			current_key = _mm_load_si128(m_w+input[0]);
			for (size_t bit_ix = 1; bit_ix < input.size(); bit_ix++)
			{
				current_key = _mm_xor_si128(current_key, m_w[input[bit_ix]]);
			}
		}
		else
#endif
        if (input.size() == 2) // 2-arity gates
		{
        	__m128i key[2], msg, out, X, Y;

			msg = _mm_set1_epi64x(m_gate_ix);

			key[0] = _mm_load_si128(m_w+input[0]);
			key[1] = _mm_load_si128(m_w+input[1]);

			const uint8_t mask_bit_0 = _mm_extract_epi8(key[0], 0) & 0x01;
			const uint8_t mask_bit_1 = _mm_extract_epi8(key[1], 0) & 0x01;

			KDF256((uint8_t*)&msg, (uint8_t*)&out, (uint8_t*)key);
			out = _mm_and_si128(out, m_out_mask);

			uint8_t garbled_ix = (mask_bit_1<<1) | (mask_bit_0<<0);

#ifdef GRR
			it = m_i_bufr_ix+(garbled_ix-1)*Env::key_size_in_bytes();
			if (garbled_ix == 0)
			{
				current_key = _mm_load_si128(&out);
			}
			else
			{
				tmp.assign(it, it+Env::key_size_in_bytes());
				tmp.resize(16, 0);
				a = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));
				current_key = _mm_xor_si128(out, a);
			}
			m_i_bufr_ix += 3*Env::key_size_in_bytes();
#else
			it = m_i_bufr_ix + garbled_ix*Env::key_size_in_bytes();
			tmp.assign(it, it+Env::key_size_in_bytes());
			tmp.resize(16, 0);
			new_current_key = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));
			new_current_key = _mm_xor_si128(new_current_key, out);

			m_i_bufr_ix += 4*Env::key_size_in_bytes();
#endif
		}
		else // 1-arity gates
		{
        	__m128i key, msg, out;

			msg = _mm_set1_epi64x(m_gate_ix);
			key = _mm_load_si128(m_w+input[0]);
			KDF128((uint8_t*)&msg, (uint8_t*)&out, (uint8_t*)&key);
			out = _mm_and_si128(out, m_out_mask);

			uint8_t garbled_ix = _mm_extract_epi8(key, 0) & 0x01;

#ifdef GRR
			it = m_i_bufr_ix;
			if (garbled_ix == 0)
			{
				current_key = _mm_load_si128(&out);
			}
			else
			{
				tmp.assign(it, it+Env::key_size_in_bytes());
				tmp.resize(16, 0);
				a = _mm_loadu_si128(reinterpret_cast<__m128i*>(&tmp[0]));
				current_key = _mm_xor_si128(out, a);
			}
			m_i_bufr_ix += Env::key_size_in_bytes();
#else
//			it = m_i_bufr_ix + garbled_ix*Env::key_size_in_bytes();
//			current_key = out ^ Bytes(it, it+Env::key_size_in_bytes());
//			m_i_bufr_ix += 2*Env::key_size_in_bytes();
#endif
		}

		if (current_gate.m_tag == Circuit::EVL_OUT)
		{
			uint8_t out_bit = _mm_extract_epi8(current_key, 0) & 0x01;
			out_bit ^= *m_i_bufr_ix;
			m_evl_out.set_ith_bit(m_evl_out_ix, out_bit);
			m_i_bufr_ix++;

			m_evl_out_ix++;
		}
		else if (current_gate.m_tag == Circuit::GEN_OUT)
		{
			// TODO: Ki08 implementation
			uint8_t out_bit = _mm_extract_epi8(current_key, 0) & 0x01;
			out_bit ^= *m_i_bufr_ix;
			m_gen_out.set_ith_bit(m_gen_out_ix, out_bit);
			m_i_bufr_ix++;

//			m_C[2*m_gen_out_ix+0] = Bytes(m_i_bufr_ix, m_i_bufr_ix+Env::key_size_in_bytes());
//			m_i_bufr_ix += Env::key_size_in_bytes();
//
//			m_C[2*m_gen_out_ix+1] = Bytes(m_i_bufr_ix, m_i_bufr_ix+Env::key_size_in_bytes());
//			m_i_bufr_ix += Env::key_size_in_bytes();

			m_gen_out_ix++;
		}
	}

	_mm_store_si128(m_w+current_gate.m_ref_cnt, current_key);

	update_hash(m_i_bufr);
	m_gate_ix++;
}
