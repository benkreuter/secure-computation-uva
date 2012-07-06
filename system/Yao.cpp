#include "Yao.h"

#include <log4cxx/logger.h>
static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("Yao.cpp"));


Yao::Yao(EnvParams &params) : YaoBase(params)
{
	if (Env::s() != 1)
	{
		LOG4CXX_FATAL(logger, "s has to be 1 in the honest-but-curious setting");
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
	}

	// Init variables
	m_rnds.resize(Env::node_load());
	m_ccts.resize(Env::node_load());
	m_gen_inp_masks.resize(Env::node_load());
}


void Yao::start()
{
	oblivious_transfer();
	circuit_evaluate();
    final_report();
}


void Yao::oblivious_transfer()
{
	// TODO: implement efficient OT ext
	step_init();

	double start;
	uint64_t comm_sz = 0;

	Bytes send, recv, bufr(Env::elm_size_in_bytes()*4);
	std::vector<Bytes> bufr_chunks, recv_chunks;

	G  gr, hr, X[2], Y[2];
    Z r, y, a, s[2], t[2];

	// step 1: generating the CRS: g[0], h[0], g[1], h[1]
	if (Env::is_root())
	{
		EVL_BEGIN
			start = MPI_Wtime();
				y.random();
				a.random();

				m_ot_g[0].random();
				m_ot_g[1] = m_ot_g[0]^y;          // g[1] = g[0]^y

				m_ot_h[0] = m_ot_g[0]^a;          // h[0] = g[0]^a
				m_ot_h[1] = m_ot_g[1]^(a + Z(1)); // h[1] = g[1]^(a+1)

				bufr.clear();
				bufr += m_ot_g[0].to_bytes();
				bufr += m_ot_g[1].to_bytes();
				bufr += m_ot_h[0].to_bytes();
				bufr += m_ot_h[1].to_bytes();
			m_timer_evl += MPI_Wtime() - start;

			start = MPI_Wtime(); // send to Gen's root process
				EVL_SEND(bufr);
			m_timer_com += MPI_Wtime() - start;
		EVL_END

		GEN_BEGIN
			start = MPI_Wtime();
				bufr = GEN_RECV();
			m_timer_com += MPI_Wtime() - start;
		GEN_END

	    comm_sz += bufr.size();
	}

	// send g[0], g[1], h[0], h[1] to slave processes
	start = MPI_Wtime();
		MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm);
	m_timer_mpi += MPI_Wtime() - start;

	start = MPI_Wtime();
		bufr_chunks = bufr.split(Env::elm_size_in_bytes());

		m_ot_g[0].from_bytes(bufr_chunks[0]);
		m_ot_g[1].from_bytes(bufr_chunks[1]);
		m_ot_h[0].from_bytes(bufr_chunks[2]);
		m_ot_h[1].from_bytes(bufr_chunks[3]);

		// pre-processing
		m_ot_g[0].fast_exp();
		m_ot_g[1].fast_exp();
		m_ot_h[0].fast_exp();
		m_ot_h[1].fast_exp();

		// allocate memory for m_keys
		m_ot_keys.resize(Env::node_load());
		for (size_t ix = 0; ix < m_ot_keys.size(); ix++)
		{
			m_ot_keys[ix].reserve(Env::circuit().evl_inp_cnt()*2);
		}
	m_timer_evl += MPI_Wtime() - start;
	m_timer_gen += MPI_Wtime() - start;

	// Step 2: ZKPoK of (g[0], g[1], h[0], h[1])

	MPI_Barrier(m_mpi_comm);

	// Step 3: gr=g[b]^r, hr=h[b]^r, where b is the evaluator's bit

	EVL_BEGIN
		start = MPI_Wtime();
			send.resize(Env::exp_size_in_bytes()*Env::circuit().evl_inp_cnt());
			bufr.resize(Env::elm_size_in_bytes()*Env::circuit().evl_inp_cnt()*2);

			if (Env::is_root())
			{
				send.clear(); bufr.clear();
				for (size_t bix = 0; bix < Env::circuit().evl_inp_cnt(); bix++)
				{
					r.random();
					send += r.to_bytes();  // to be shared with slaves

					byte bit_value = m_evl_inp.get_ith_bit(bix);
					bufr += (m_ot_g[bit_value]^r).to_bytes(); // gr
					bufr += (m_ot_h[bit_value]^r).to_bytes(); // hr
				}
			}
		m_timer_evl += MPI_Wtime() - start;

		start = MPI_Wtime();
			MPI_Bcast(&send[0], send.size(), MPI_BYTE, 0, m_mpi_comm); // now every evaluator has r's
		m_timer_mpi += MPI_Wtime() - start;
	EVL_END

	if (Env::is_root())
	{
		EVL_BEGIN
			// send (gr, hr)'s
			start = MPI_Wtime();
				EVL_SEND(bufr);
			m_timer_com += MPI_Wtime() - start;
		EVL_END

		GEN_BEGIN
			// receive (gr, hr)'s
			start = MPI_Wtime();
				bufr = GEN_RECV();
			m_timer_com += MPI_Wtime() - start;
		GEN_END

		comm_sz += bufr.size();
	}

	// Step 4: the generator computes X[0], Y[0], X[1], Y[1]

	GEN_BEGIN
		// forward (gr, hr)'s to slaves
		start = MPI_Wtime();
			bufr.resize(Env::circuit().evl_inp_cnt()*2*Env::elm_size_in_bytes());
		m_timer_gen += MPI_Wtime() - start;

		start = MPI_Wtime();
			MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm); // now every Bob has bufr
		m_timer_mpi += MPI_Wtime() - start;

		start = MPI_Wtime();
			bufr_chunks = bufr.split(Env::elm_size_in_bytes());
		m_timer_gen += MPI_Wtime() - start;

		for (size_t bix = 0; bix < Env::circuit().evl_inp_cnt(); bix++)
		{
			start = MPI_Wtime();
				gr.from_bytes(bufr_chunks[2*bix+0]);
				hr.from_bytes(bufr_chunks[2*bix+1]);

				if (m_ot_keys.size() > 2)
				{
					gr.fast_exp();
					hr.fast_exp();
				}
			m_timer_gen += MPI_Wtime() - start;

			for (size_t cix = 0; cix < m_ot_keys.size(); cix++)
			{
				start = MPI_Wtime();
					Y[0].random(); // K[0]
					Y[1].random(); // K[1]

					m_ot_keys[cix].push_back(Y[0].to_bytes().hash(Env::k()));
					m_ot_keys[cix].push_back(Y[1].to_bytes().hash(Env::k()));

					s[0].random(); s[1].random();
					t[0].random(); t[1].random();

					// X[b] = ( g[b]^s[b] ) * ( h[b]^t[b] ), where b = 0, 1
					X[0] = m_ot_g[0]^s[0]; X[0] *= m_ot_h[0]^t[0];
					X[1] = m_ot_g[1]^s[1]; X[1] *= m_ot_h[1]^t[1];

					// Y[b] = ( gr^s[b] ) * ( hr^t[b] ) * K[b], where b = 0, 1
					Y[0] *= gr^s[0]; Y[0] *= hr^t[0];
					Y[1] *= gr^s[1]; Y[1] *= hr^t[1];

					send.clear();
					send += X[0].to_bytes(); send += X[1].to_bytes();
					send += Y[0].to_bytes(); send += Y[1].to_bytes();
				m_timer_gen += MPI_Wtime() - start;

				start = MPI_Wtime();
					GEN_SEND(send);
				m_timer_com += MPI_Wtime() - start;

				comm_sz += send.size();
			}
		}

		for (size_t ix = 0; ix < m_ot_keys.size(); ix++)
		{
			assert(m_ot_keys[ix].size() == Env::circuit().evl_inp_cnt()*2);
		}
	GEN_END

	// Step 5: the evaluator computes K = Y[b]/X[b]^r
	EVL_BEGIN
		start = MPI_Wtime(); // send has r's
			bufr_chunks = send.split(Env::exp_size_in_bytes());
		m_timer_evl += MPI_Wtime() - start;

		for (size_t bix = 0; bix < Env::circuit().evl_inp_cnt(); bix++)
		{
			start = MPI_Wtime();
				int bit_value = m_evl_inp.get_ith_bit(bix);
				r.from_bytes(bufr_chunks[bix]);
			m_timer_evl += MPI_Wtime() - start;

			for (size_t cix = 0; cix < m_ot_keys.size(); cix++)
			{
				start = MPI_Wtime();
					recv = EVL_RECV(); // receive X[0], X[1], Y[0], Y[1]
				m_timer_com += MPI_Wtime() - start;

				comm_sz += recv.size();

				start = MPI_Wtime();
					recv_chunks = recv.split(Env::elm_size_in_bytes());

					X[bit_value].from_bytes(recv_chunks[    bit_value]); // X[b]
					Y[bit_value].from_bytes(recv_chunks[2 + bit_value]); // Y[b]

					// K = Y[b]/(X[b]^r)
					Y[bit_value] /= X[bit_value]^r;
					m_ot_keys[cix].push_back(Y[bit_value].to_bytes().hash(Env::k()));
				m_timer_evl += MPI_Wtime() - start;
			}
		}

		for (size_t ix = 0; ix < m_ot_keys.size(); ix++)
		{
			assert(m_ot_keys[ix].size() == Env::circuit().evl_inp_cnt());
		}
	EVL_END

	step_report(comm_sz, "ob-transfer");
}


void Yao::circuit_evaluate()
{
	step_init();

	double start;
	uint64_t comm_sz = 0;

	Bytes bufr;
	vector<Bytes> bufr_chunks;

	G M;

	for (size_t ix = 0; ix < m_ccts.size(); ix++)
	{
		GEN_BEGIN
			start = MPI_Wtime();
				m_rnds[ix] = m_prng.rand(Env::k());
				m_gen_inp_masks[ix] = m_prng.rand(Env::circuit().gen_inp_cnt());
			m_timer_gen += MPI_Wtime() - start;

			start = MPI_Wtime();
				GEN_SEND(m_gen_inp_masks[ix] ^ m_gen_inp); // send the masked gen_inp
			m_timer_com += MPI_Wtime() - start;

			start = MPI_Wtime();
				m_ccts[ix].gen_init(m_ot_keys[ix], m_gen_inp_masks[ix], m_rnds[ix]);
			m_timer_gen += MPI_Wtime() - start;
		GEN_END

		EVL_BEGIN
			start = MPI_Wtime();
				m_gen_inp_masks[ix] = EVL_RECV(); // receive the masked gen_inp
			m_timer_com += MPI_Wtime() - start;

			start = MPI_Wtime();
				m_ccts[ix].evl_init(m_ot_keys[ix], m_gen_inp_masks[ix], m_evl_inp);
			m_timer_evl += MPI_Wtime() - start;
		EVL_END

		comm_sz += m_gen_inp_masks[ix].size();
	}

	GEN_BEGIN
		start = MPI_Wtime();
			bufr.clear();
			for (int ix = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
			{
				bool bit_value = m_gen_inp.get_ith_bit(ix);
				bufr += m_ccts[0].m_M[2*ix+bit_value].to_bytes();
			}
		m_timer_gen += MPI_Wtime() - start;

		start = MPI_Wtime();
			GEN_SEND(bufr);
		m_timer_com += MPI_Wtime() - start;
	GEN_END

	EVL_BEGIN
		start = MPI_Wtime();
			bufr = EVL_RECV();
		m_timer_com += MPI_Wtime() - start;

		start = MPI_Wtime();
			bufr_chunks = bufr.split(Env::elm_size_in_bytes());
			for (size_t ix = 0, kx = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
			{
				M.from_bytes(bufr_chunks[ix]);

				for (size_t jx = 0; jx < m_ccts.size(); jx++)
				{
					m_ccts[jx].m_M.push_back(M);
				}
			}
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	comm_sz += bufr.size();

	GEN_BEGIN // generate and send the circuit gate-by-gate
		start = MPI_Wtime();
			while (Env::circuit().more_gate_binary())
			{
				const Gate &g = Env::circuit().next_gate_binary();

				for (size_t ix = 0; ix < m_ccts.size(); ix++)
				{
						m_ccts[ix].gen_next_gate(g);
						bufr = m_ccts[ix].send();
					m_timer_gen += MPI_Wtime() - start;

					start = MPI_Wtime();
						GEN_SEND(bufr);
					m_timer_com += MPI_Wtime() - start;

					comm_sz += bufr.size();

					start = MPI_Wtime(); // start m_timer_gen
				}
			}
		m_timer_gen += MPI_Wtime() - start;
	GEN_END

	EVL_BEGIN // receive and evaluate the circuit gate-by-gate
		start = MPI_Wtime();
			while (Env::circuit().more_gate_binary())
			{
				const Gate &g = Env::circuit().next_gate_binary();

				for (size_t ix = 0; ix < m_ccts.size(); ix++)
				{
					m_timer_evl += MPI_Wtime() - start;

					start = MPI_Wtime();
						bufr = EVL_RECV();
					m_timer_com += MPI_Wtime() - start;

					comm_sz += bufr.size();

					start = MPI_Wtime();
						m_ccts[ix].recv(bufr);
						m_ccts[ix].evl_next_gate(g);
				}
			}
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	step_report(comm_sz, "circuit-evl");

	if (Env::circuit().evl_out_cnt() != 0)
		proc_evl_out();

    if (Env::circuit().gen_out_cnt() != 0)
        proc_gen_out();
}


void Yao::proc_evl_out()
{
	step_init();

	double start;
	uint64_t comm_sz = 0;

	EVL_BEGIN
		start = MPI_Wtime();
			m_evl_out = m_ccts[0].m_evl_out;
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	step_report(comm_sz, "chk-evl-out");
}

void Yao::proc_gen_out()
{
	step_init();

	double start;
	uint64_t comm_sz = 0;

	EVL_BEGIN
		start = MPI_Wtime();
			m_gen_out = m_ccts[0].m_gen_out;
		m_timer_evl += MPI_Wtime() - start;

		start = MPI_Wtime();
			EVL_SEND(m_gen_out);
		m_timer_com += MPI_Wtime() - start;
	EVL_END

	GEN_BEGIN
		start = MPI_Wtime();
			m_gen_out = GEN_RECV();
		m_timer_com += MPI_Wtime() - start;
	GEN_END

	comm_sz += m_gen_out.size();

	step_report(comm_sz, "chk-gen-out");
}
