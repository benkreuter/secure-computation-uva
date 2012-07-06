#include "BetterYao.h"

#include <log4cxx/logger.h>
static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("BetterYao.cpp"));


BetterYao::BetterYao(EnvParams &params) : YaoBase(params)
{
	// Init variables
	m_coms.resize(Env::node_load());
	m_rnds.resize(Env::node_load());
	m_ccts.resize(Env::node_load());
	m_gen_inp_masks.resize(Env::node_load());
}


void BetterYao::start()
{
	// Random Combination Technique from PSSW09
//	ot_ext();

	// Committing OT from SS11
	oblivious_transfer();

	circuit_commit();
	cut_and_choose();
	consistency_check();
	circuit_evaluate();

    final_report();
}


void BetterYao::oblivious_transfer()
{
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


void BetterYao::circuit_commit()
{
	step_init();

	double start;
	uint64_t comm_sz = 0;

	Bytes bufr;

	// construct garbled circuits and generate its hash on the fly
	GEN_BEGIN
		start = MPI_Wtime();
			for (size_t ix = 0; ix < m_ccts.size(); ix++)
			{
				m_rnds[ix] = m_prng.rand(Env::k());
				m_gen_inp_masks[ix] = m_prng.rand(Env::circuit().gen_inp_cnt());
				m_ccts[ix].com_init(m_ot_keys[ix], m_gen_inp_masks[ix], m_rnds[ix]);
			}

			while (Env::circuit().more_gate_binary())
			{
				const Gate & g = Env::circuit().next_gate_binary();
				for (size_t ix = 0; ix < m_ccts.size(); ix++)
				{
					m_ccts[ix].com_next_gate(g);
				}
			}

			for (size_t ix = 0; ix < m_coms.size(); ix++)
			{
				m_coms[ix] = m_ccts[ix].hash();
			}
		m_timer_gen += MPI_Wtime() - start;
	GEN_END

	// commit to a garbled circuit by giving away its hash
	GEN_BEGIN
		start = MPI_Wtime();
			bufr = Bytes(m_coms);
		m_timer_gen += MPI_Wtime() - start;

		start = MPI_Wtime();
			GEN_SEND(bufr);
		m_timer_com += MPI_Wtime() - start;
	GEN_END

	// receive hashes of the garbled circuits
	EVL_BEGIN
		start = MPI_Wtime();
			bufr = EVL_RECV();
		m_timer_com += MPI_Wtime() - start;

		start = MPI_Wtime();
			m_coms = bufr.split(bufr.size()/m_ccts.size());
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	comm_sz += bufr.size();

    step_report(comm_sz, "circuit-gen");
}


void BetterYao::cut_and_choose()
{
	step_init();

	double start;
	uint64_t comm_sz = 0;

	if (Env::is_root())
	{
		Bytes coins = m_prng.rand(Env::k());         // Step0: flip coins
		Bytes remote_coins, comm, open;

		EVL_BEGIN
			start = MPI_Wtime();
				comm = EVL_RECV();                   // Step1: receive bob's commitment
				EVL_SEND(coins);                     // Step2: send coins to bob
				open = EVL_RECV();
			m_timer_com += MPI_Wtime() - start;

			start = MPI_Wtime();
				if (!(open.hash(Env::s()) == comm))         // Step3: check bob's decommitment
				{
					LOG4CXX_FATAL(logger, "commitment to coins can't be properly opened");
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				}
				remote_coins = Bytes(open.begin()+Env::k()/8, open.end());
			m_timer_evl += MPI_Wtime() - start;
		EVL_END

		GEN_BEGIN
			start = MPI_Wtime();
				open = m_prng.rand(Env::k()) + coins;       // Step1: commit to coins
				comm = open.hash(Env::s());
			m_timer_gen += MPI_Wtime() - start;

			start = MPI_Wtime();
				GEN_SEND(comm);
				remote_coins = GEN_RECV();           // Step2: receive alice's coins
				GEN_SEND(open);                      // Step3: decommit to the coins
			m_timer_com += MPI_Wtime() - start;
		GEN_END

		comm_sz = comm.size() + remote_coins.size() + open.size();

		start = MPI_Wtime();
			coins ^= remote_coins;
			Prng prng;
			prng.srand(coins); // use the coins to generate more randomness

			// make 60-40 check-vs-evaluateion circuit ratio
			m_all_chks.assign(Env::s(), 1);

			// FisherÐYates shuffle
			std::vector<int> indices(m_all_chks.size());
			for (size_t ix = 0; ix < indices.size(); ix++) { indices[ix] = ix; }

			// starting from 1 since the 0-th circuit is always evaluation-circuit
			for (size_t ix = 1; ix < indices.size(); ix++)
			{
				int rand_ix = prng.rand_range(indices.size()-ix);
				std::swap(indices[ix], indices[ix+rand_ix]);
			}

			int num_of_evls;
			switch(m_all_chks.size())
			{
			case 0: case 1:
				LOG4CXX_FATAL(logger, "there isn't enough circuits for cut-and-choose");
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				break;

			case 2: case 3:
				num_of_evls = 1;
				break;

			case 4:
				num_of_evls = 2;
				break;

			default:
				num_of_evls = m_all_chks.size()*2/5;
				break;
			}

			for (size_t ix = 0; ix < num_of_evls; ix++) { m_all_chks[indices[ix]] = 0; }
		m_timer_evl += MPI_Wtime() - start;
		m_timer_gen += MPI_Wtime() - start;
	}

	start = MPI_Wtime();
		m_chks.resize(Env::node_load());
	m_timer_evl += MPI_Wtime() - start;
	m_timer_gen += MPI_Wtime() - start;

	start = MPI_Wtime();
		MPI_Scatter(&m_all_chks[0], m_chks.size(), MPI_BYTE, &m_chks[0], m_chks.size(), MPI_BYTE, 0, m_mpi_comm);
	m_timer_mpi += MPI_Wtime() - start;

	step_report(comm_sz, "cut-&-check");
}


void BetterYao::consistency_check()
{
	step_init();

	Bytes send, recv, bufr;
	std::vector<Bytes> recv_chunks, bufr_chunks;

	double start = 0;
	uint64_t comm_sz = 0;

	Z m;
	G M;

	EVL_BEGIN
		if (Env::is_root())
		{
			start = MPI_Wtime();
				bufr = EVL_RECV();
			m_timer_com += MPI_Wtime() - start;

			comm_sz += bufr.size();
		}

		start = MPI_Wtime();
			bufr.resize(Env::elm_size_in_bytes()*Env::circuit().gen_inp_cnt());
		m_timer_evl += MPI_Wtime() - start;

		start = MPI_Wtime();
			MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm);
		m_timer_mpi += MPI_Wtime() - start;

		start = MPI_Wtime();
			bufr_chunks = bufr.split(Env::elm_size_in_bytes());
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	GEN_BEGIN
		if (Env::is_root())
		{
			start = MPI_Wtime();
				bufr.clear();
				for (int ix = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
				{
					bool bit_value = (m_gen_inp[ix/8]>>(ix%8)) & 0x01;
					M = Env::clawfree().F(bit_value,  m_ccts[0].m_m[2*ix+bit_value]);
					bufr += M.to_bytes(); // M[0]
				}
			m_timer_gen += MPI_Wtime() - start;

			start = MPI_Wtime();
				GEN_SEND(bufr);
			m_timer_com += MPI_Wtime() - start;

			comm_sz += bufr.size();
		}
	GEN_END

	EVL_BEGIN
		start = MPI_Wtime();
			recv = EVL_RECV();
		m_timer_com += MPI_Wtime() - start;

		comm_sz += recv.size();

		start = MPI_Wtime();
			recv_chunks = recv.split(Env::exp_size_in_bytes());
			for (size_t ix = 0, kx = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
			{
				M.from_bytes(bufr_chunks[ix]);

				for (size_t jx = 0; jx < m_ccts.size(); jx++)
				{
					if (m_chks[jx])
						continue;

					// m = m[i] - m[0]
					m.from_bytes(recv_chunks[kx++]);
					m_ccts[jx].m_M.push_back(Env::clawfree().R(m)); // h^m
					m_ccts[jx].m_M.back() *= M;                     // M[0] * h^m = M[i]
				}
			}
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	GEN_BEGIN
		start = MPI_Wtime();
			if (Env::is_root())
			{
				bufr.clear();
				for (size_t ix = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
				{
					bool bit_value = (m_gen_inp[ix/8]>>(ix%8)) & 0x01;
					bufr += m_ccts[0].m_m[2*ix+bit_value].to_bytes();
				}
			}
			bufr.resize(Env::exp_size_in_bytes()*Env::circuit().gen_inp_cnt());
		m_timer_gen += MPI_Wtime() - start;

		start = MPI_Wtime();
			MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm);
		m_timer_mpi += MPI_Wtime() - start;

		start = MPI_Wtime();
			bufr_chunks = bufr.split(Env::exp_size_in_bytes());

			send.clear();
			for (size_t ix = 0; ix < Env::circuit().gen_inp_cnt(); ix++)
			{
				bool bit_value = (m_gen_inp[ix/8]>>(ix%8)) & 0x01;
				m.from_bytes(bufr_chunks[ix]);

				for (size_t jx = 0; jx < m_ccts.size(); jx++)
				{
					if (m_chks[jx]) // no data for check-circuits
						continue;
					send += (m_ccts[jx].m_m[2*ix+bit_value]-m).to_bytes();
				}
			}
		m_timer_gen += MPI_Wtime() - start;

		start = MPI_Wtime();
			GEN_SEND(send);
		m_timer_com += MPI_Wtime() - start;

		comm_sz += send.size();
	GEN_END

	step_report(comm_sz, "const-check");
}


void BetterYao::circuit_evaluate()
{
	step_init();

	Env::circuit().reload_binary();

	int verify = 1;
	double start;
	uint64_t comm_sz = 0;
	Bytes bufr;

	for (size_t ix = 0; ix < m_ccts.size(); ix++)
	{
		if (m_chks[ix]) // check-circuits
		{
			GEN_BEGIN // reveal randomness
				start = MPI_Wtime();
					bufr = Bytes(m_ot_keys[ix]);
				m_timer_gen += MPI_Wtime() - start;

				start = MPI_Wtime();
					GEN_SEND(m_gen_inp_masks[ix]);
					GEN_SEND(m_rnds[ix]);
					GEN_SEND(bufr);
				m_timer_com += MPI_Wtime() - start;
			GEN_END

			EVL_BEGIN // receive randomness
				start = MPI_Wtime();
					m_gen_inp_masks[ix] = EVL_RECV();
					m_rnds[ix] = EVL_RECV();
					bufr = EVL_RECV();
				m_timer_com += MPI_Wtime() - start;

				start = MPI_Wtime();
					m_ot_keys[ix] = bufr.split(Env::key_size_in_bytes());
					m_ccts[ix].com_init(m_ot_keys[ix], m_gen_inp_masks[ix], m_rnds[ix]);
				m_timer_evl += MPI_Wtime() - start;
			EVL_END

			comm_sz += m_gen_inp_masks[ix].size() + m_rnds[ix].size() + bufr.size();
		}
		else // evaluation-circuits
		{
			EVL_BEGIN
				start = MPI_Wtime();
					m_gen_inp_masks[ix] = EVL_RECV();
				m_timer_com += MPI_Wtime() - start;

				start = MPI_Wtime();
					m_ccts[ix].evl_init(m_ot_keys[ix], m_gen_inp_masks[ix], m_evl_inp);
				m_timer_evl += MPI_Wtime() - start;
			EVL_END

			GEN_BEGIN
				start = MPI_Wtime();
					GEN_SEND(m_gen_inp_masks[ix] ^ m_gen_inp); // send the masked gen_inp
				m_timer_com += MPI_Wtime() - start;

				start = MPI_Wtime();
					m_ccts[ix].gen_init(m_ot_keys[ix], m_gen_inp_masks[ix], m_rnds[ix]);
				m_timer_gen += MPI_Wtime() - start;
			GEN_END

			comm_sz += m_gen_inp_masks[ix].size();
		}
	}

	EVL_BEGIN
		start = MPI_Wtime();
			while (Env::circuit().more_gate_binary())
			{
				const Gate &g = Env::circuit().next_gate_binary();

				for (size_t ix = 0; ix < m_ccts.size(); ix++)
				{
					if (m_chks[ix]) { m_ccts[ix].com_next_gate(g); continue; }

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

	GEN_BEGIN // re-generate the evaluation-circuits
		start = MPI_Wtime();
			while (Env::circuit().more_gate_binary())
			{
				const Gate &g = Env::circuit().next_gate_binary();

				for (size_t ix = 0; ix < m_ccts.size(); ix++)
				{
					if (m_chks[ix]) { continue; }

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

	EVL_BEGIN // check the hash of all the garbled circuits
		start = MPI_Wtime();
			for (size_t ix = 0; ix < m_ccts.size(); ix++)
			{
				verify &= (m_ccts[ix].hash() == m_coms[ix]);

				if (m_ccts[ix].hash() != m_coms[ix] &&  m_chks[ix]) // check-circuit
				{
					std::cout << "chk: " << m_ccts[ix].hash().to_hex() << " vs " << m_coms[ix].to_hex() << std::endl;
				}
				if (m_ccts[ix].hash() != m_coms[ix] && !m_chks[ix]) // evaluation-circuit
				{
					std::cout << "evl: " << m_ccts[ix].hash().to_hex() << " vs " << m_coms[ix].to_hex() << std::endl;
				}
			}

			int all_verify;
		m_timer_evl += MPI_Wtime() - start;

		start = MPI_Wtime();
			MPI_Reduce(&verify, &all_verify, 1, MPI_INT, MPI_LAND, 0, m_mpi_comm);
		m_timer_mpi += MPI_Wtime() - start;

		start = MPI_Wtime();
			if (Env::is_root() && !all_verify)
			{
				LOG4CXX_FATAL(logger, "Verification failed");
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			}
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	step_report(comm_sz, "circuit-evl");

	if (Env::circuit().evl_out_cnt() != 0)
		proc_evl_out();

    if (Env::circuit().gen_out_cnt() != 0)
        proc_gen_out();
}


void BetterYao::proc_evl_out()
{
	step_init();

	double start;
	uint64_t comm_sz = 0;
	Bytes send, recv;

	EVL_BEGIN
		start = MPI_Wtime();
			const Bytes ZEROS((Env::circuit().evl_out_cnt()+7)/8, 0);

			for (size_t ix = 0; ix < m_ccts.size(); ix++) // fill zeros for uniformity (convenient to MPIs)
			{
				send += (m_chks[ix])? ZEROS : m_ccts[ix].m_evl_out;
			}

			if (Env::group_rank() == 0)
			{
				recv.resize(send.size()*Env::node_amnt());
			}
		m_timer_evl += MPI_Wtime() - start;

		start = MPI_Wtime();
			MPI_Gather(&send[0], send.size(), MPI_BYTE, &recv[0], send.size(), MPI_BYTE, 0, m_mpi_comm);
		m_timer_mpi += MPI_Wtime() - start;

		start = MPI_Wtime();
			if (Env::is_root())
			{
				size_t chks_total = 0;
				for (size_t ix = 0; ix < m_all_chks.size(); ix++)
					chks_total += m_all_chks[ix];

				// find majority by locating the median of output from evaluation-circuits
				std::vector<Bytes> vec = recv.split((Env::circuit().evl_out_cnt()+7)/8);
				size_t median_ix = (chks_total+vec.size())/2;
				std::nth_element(vec.begin(), vec.begin()+median_ix, vec.end());

				m_evl_out = *(vec.begin()+median_ix);
			}
		m_timer_evl += MPI_Wtime() - start;
	EVL_END

	step_report(comm_sz, "chk-evl-out");
}

void BetterYao::proc_gen_out()
{
	double start;
	uint64_t comm_sz = 0;

	step_init();
	m_gen_out = m_ccts[0].m_gen_out;

	EVL_BEGIN
		send_data(Env::world_rank()-1, m_gen_out);
	EVL_END

	GEN_BEGIN
		m_gen_out = recv_data(Env::world_rank()+1);
	GEN_END

	step_report(comm_sz, "chk-gen-out");

//	Bytes send, bufr, recv;
//	std::vector<Bytes> bufr_chunks;
//	step_report(comm_sz, "generator's output processing");
//
//	G g, h;
//
//	start = MPI_Wtime();
//		if (Env::is_root())
//		{
//			if (Env::is_evl()) // evaluator
//			{
//				g.random();
//
//				send = g.to_bytes();
//				send_data(Env::world_rank()-1, send);
//				recv = recv_data(Env::world_rank()-1);
//
//				bufr = send + recv;
//			}
//			else // generator
//			{
//				h.random();
//
//				recv = recv_data(Env::world_rank()+1);
//				send = h.to_bytes();
//				send_data(Env::world_rank()+1, send);
//
//				bufr = recv + send;
//			}
//		}
//
//		bufr.resize(Env::elm_size_in_bytes()*2);
//		MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm);
//		bufr_chunks = bufr.split(Env::elm_size_in_bytes());
//
//		g.from_bytes(bufr_chunks[0]);
//		h.from_bytes(bufr_chunks[1]);
//
//		g.fast_exp();
//		h.fast_exp();
//	m_timer_evl += MPI_Wtime() - start;
//	m_timer_gen += MPI_Wtime() - start;
//
// Step 1. Alice commits to Bob's output keys: g^M[ix] * h^r[ix]
//
//    for (size_t ix = 0; ix < C_array.size(); ix++)
//    {
//#ifdef ALICE
//        start = clock();
//            // M[ix] = hash(C[ix,0] || C[ix,1] || ... || C[ix,e-1])
//            temp.clear();
//            for (size_t jx = 0; jx < m_evls[ix]->C.size(); jx++)
//            {
//                temp.insert(temp.end(), m_evls[ix]->C[jx].begin(), m_evls[ix]->C[jx].end());
//            }
//            temp = Env::H(temp, Env::EXP_LENGTH()*8);
//            my_element_from_bytes(&*M_array[ix], temp);
//
//            // C[ix] = h^r[ix]
//            element_random(&*r_array[ix]);
//            element_pp_pow_zn(G1_elem, &*M_array[ix], pp_g);     // elem = g^M[ix]
//            element_pp_pow_zn(&*C_array[ix], r_array[ix], pp_h); //    C = h^r[ix]
//            element_mul(G1_elem, G1_elem, &*C_array[ix]);        // elem = g^M[ix] h^r[ix]
//        timer_e += clock() - start;
//
//        LOG4CXX_DEBUG
//        (
//            logger,
//            "\nC[" << ix <<  "]: " << Env::Hex(my_element_to_bytes(G1_elem))
//        );
//
//        m_socket.write_Bytes(my_element_to_bytes(G1_elem));
//        sz += element_length_in_bytes(G1_elem);
//#elif defined BOB
//		my_element_from_bytes(&*C_array[ix], m_socket.read_Bytes());
//		sz += element_length_in_bytes(&*C_array[ix]);
//#endif
//    }
//
//		clock_t start;
//	size_t sz = 0;
//	Bytes temp;
//
//	element_t g, h, G1_elem, Zr_elem;
//
//	element_init_G1(g, Env::pairing);
//	element_init_G1(h, Env::pairing);
//	element_init_G1(G1_elem, Env::pairing);
//	element_init_Zr(Zr_elem, Env::pairing);
//
//#ifdef ALICE
//	std::vector<element_s *> a_array(Env::s()*1/2); // for Or proof
//	for (size_t ix = 0; ix < Env::s()*1/2; ix++)
//    {
//		a_array[ix] = new element_s();
//		element_init_Zr(&*a_array[ix], Env::pairing);
//    }
//#elif defined BOB
//	std::vector<element_s *> A_array(Env::s()*1/2); // for Or proof
//	for (size_t ix = 0; ix < Env::s()*1/2; ix++)
//    {
//		A_array[ix] = new element_s();
//		element_init_G1(&*A_array[ix], Env::pairing);
//    }
//#endif
//
//	std::vector<element_s *> M_array(Env::s()*1/2);
//	std::vector<element_s *> r_array(Env::s()*1/2);
//	std::vector<element_s *> C_array(Env::s()*1/2);
//
//	for (size_t ix = 0; ix < Env::s()*1/2; ix++)
//	{
//		M_array[ix] = new element_s();
//		r_array[ix] = new element_s();
//		C_array[ix] = new element_s();
//
//		element_init_Zr(&*M_array[ix], Env::pairing);
//		element_init_Zr(&*r_array[ix], Env::pairing);
//		element_init_G1(&*C_array[ix], Env::pairing);
//	}
//
//	element_init_G1(g, Env::pairing);
//	element_init_G1(h, Env::pairing);
//
//	// Alice picks g, and Bob picks h.
//#ifdef ALICE
//    start = clock();
//        element_random(g);
//    timer_e += clock() - start;
//	m_socket.write_Bytes(my_element_to_bytes(g));
//#elif defined BOB
//	my_element_from_bytes(g, m_socket.read_Bytes());
//#endif
//
//#ifdef ALICE
//	my_element_from_bytes(h, m_socket.read_Bytes());
//#elif defined BOB
//    start = clock();
//        element_random(h);
//    timer_g += clock() - start;
//	m_socket.write_Bytes(my_element_to_bytes(h));
//#endif
//
//	sz += element_length_in_bytes(g);
//	sz += element_length_in_bytes(h);
//
//	element_pp_t pp_g, pp_h;
//
//    start = clock();
//        element_pp_init(pp_g, g);
//        element_pp_init(pp_h, h);
//#ifdef ALICE
//    timer_e += clock() - start;
//#elif defined BOB
//    timer_g += clock() - start;
//#endif
//
//	// Step 1. Alice commits to Bob's output keys: g^M[ix] * h^r[ix]
//    for (size_t ix = 0; ix < C_array.size(); ix++)
//    {
//#ifdef ALICE
//        start = clock();
//            // M[ix] = hash(C[ix,0] || C[ix,1] || ... || C[ix,e-1])
//            temp.clear();
//            for (size_t jx = 0; jx < m_evls[ix]->C.size(); jx++)
//            {
//                temp.insert(temp.end(), m_evls[ix]->C[jx].begin(), m_evls[ix]->C[jx].end());
//            }
//            temp = Env::H(temp, Env::EXP_LENGTH()*8);
//            my_element_from_bytes(&*M_array[ix], temp);
//
//            // C[ix] = h^r[ix]
//            element_random(&*r_array[ix]);
//            element_pp_pow_zn(G1_elem, &*M_array[ix], pp_g);     // elem = g^M[ix]
//            element_pp_pow_zn(&*C_array[ix], r_array[ix], pp_h); //    C = h^r[ix]
//            element_mul(G1_elem, G1_elem, &*C_array[ix]);        // elem = g^M[ix] h^r[ix]
//        timer_e += clock() - start;
//
//        LOG4CXX_DEBUG
//        (
//            logger,
//            "\nC[" << ix <<  "]: " << Env::Hex(my_element_to_bytes(G1_elem))
//        );
//
//        m_socket.write_Bytes(my_element_to_bytes(G1_elem));
//        sz += element_length_in_bytes(G1_elem);
//#elif defined BOB
//		my_element_from_bytes(&*C_array[ix], m_socket.read_Bytes());
//		sz += element_length_in_bytes(&*C_array[ix]);
//#endif
//    }
//
//	// Step 2. Bob opens the commitments
//#ifdef ALICE
//	std::vector<std::vector<Bytes> > gen_out_keys(Env::s()*1/2);
//
//    for (size_t ix = 0; ix < Env::s()*1/2; ix++)
//    {
//        gen_out_keys[ix].resize(2*Env::circuit().gen_out.size());
//        for (size_t jx = 0; jx < 2*Env::circuit().gen_out.size(); jx++)
//        {
//            temp = m_socket.read_Bytes();
//            start = clock();
//                if (!(m_ccts[ix]->gen_out_commit[jx] == Env::H(temp, Env::k()+1)))
//                {
//                    LOG4CXX_FATAL (logger, "Decommitment fails!");
//                }
//            timer_e += clock() - start;
//
//            LOG4CXX_DEBUG
//            (
//                logger,
//                "\ngen_out_commit[" << ix << ", " << jx << "]: " <<
//                Env::Hex(m_ccts[ix]->gen_out_commit[jx]) <<
//                "\nreceived commitment: " <<
//                Env::Hex(Env::H(temp, Env::k()+1))
//            );
//
//            start = clock();
//                gen_out_keys[ix][jx] = Bytes(temp.begin(), temp.begin()+Env::KEY_LENGTH());
//            timer_e += clock() - start;
//            sz += temp.size();
//        }
//    }
//#elif defined BOB
//	for (size_t ix = 0; ix < Env::s()*1/2; ix++)
//	{
//		for (size_t jx = 0; jx < m_gens[ix]->C.size(); jx++)
//		{
//			m_socket.write_Bytes(m_gens[ix]->C[jx]);
//			LOG4CXX_TRACE
//			(
//				logger,
//				"\nDecomit[" << ix << ", " << jx << "]: " << Env::Hex(m_gens[ix]->C[jx])
//			);
//			sz += m_gens[ix]->C[jx].size();
//		}
//	}
//#endif
//
//	// Step 3. Alice sends Bob's output value
//#ifdef ALICE
//	m_socket.write_Bytes(m_evls[0]->gen_out_bytes);
//	sz += m_evls[0]->gen_out_bytes.size();
//#elif defined BOB
//	Bytes gen_out_bytes = m_socket.read_Bytes();
//	sz += gen_out_bytes.size();
//
//	assert(gen_out_bytes.size() == (Env::circuit().gen_out.size()+7)/8);
//
//    // Bob computes the proof statement from his output.
//	start = clock();
//	{
//		for (size_t ix = 0; ix < Env::s()*1/2; ix++)
//		{
//			// M[ix] = hash(C[ix, 2*0+b0] || C[ix, 2*1+b1] || ... || C[ix, 2*n+bn])
//			temp.clear();
//			for (size_t jx = 0; jx < Env::circuit().gen_out.size(); jx++)
//			{
//				size_t bit = (gen_out_bytes[jx/8] >> (jx%8)) & 0x01;
//				size_t bit_ix = 2*jx + bit;
//				// 1st half of C is the message, and 2nd half is the randomness.
//				temp.insert
//				(
//					temp.end(),
//					m_gens[ix]->C[bit_ix].begin(),
//					m_gens[ix]->C[bit_ix].begin()+Env::KEY_LENGTH()
//				);
//			}
//			my_element_from_bytes
//			(
//				&*M_array[ix],
//				Env::H(temp, Env::EXP_LENGTH()*8)
//			);
//
//			element_pp_pow_zn(G1_elem, &*M_array[ix], pp_g);    // G1_elem = g^M[ix]
//			element_div(&*C_array[ix], &*C_array[ix], G1_elem); // C[ix] = h^r[ix]
//			LOG4CXX_DEBUG
//			(
//				logger,
// 				"\nC[" << ix << "]: " << Env::Hex(my_element_to_bytes(C_array[ix]))
//			);
//		}
//	}
//	timer_g += clock() - start;
//#endif
//
//	// Step 4. Or proof: OR_ix C[ix] == h^{r[ix]}.
//#ifdef ALICE
//	for (size_t ix = 0; ix < a_array.size(); ix++) // sending commitment h^a[ix]
//	{
//        start = clock();
//            element_random(&*a_array[ix]);
//            element_pp_pow_zn(G1_elem, &*a_array[ix], pp_h);
//        timer_e += clock() - start;
//		m_socket.write_Bytes(my_element_to_bytes(G1_elem));
//		sz += element_length_in_bytes(G1_elem);
//	}
//#elif defined BOB
//	for (size_t ix = 0; ix < C_array.size(); ix++) // getting commitment A[ix] = h^a[ix]
//	{
//		my_element_from_bytes(&*A_array[ix], m_socket.read_Bytes());
//		sz += element_length_in_bytes(&*A_array[ix]);
//	}
//#endif
//
//	element_t c; // challenge c
//	element_init_Zr(c, Env::pairing);
//#ifdef ALICE
//	my_element_from_bytes(c, m_socket.read_Bytes());
//#elif defined BOB
//    start = clock();
//        element_random(c);
//    timer_g += clock() - start;
//	m_socket.write_Bytes(my_element_to_bytes(c));
//#endif
//	sz += element_length_in_bytes(c);
//
//#ifdef ALICE
//	for (size_t ix = 0; ix < a_array.size(); ix++) // sending response a[ix] - c*r[ix]
//	{
//        start = clock();
//            element_mul(Zr_elem, c, &*r_array[ix]);
//            element_sub(Zr_elem, &*a_array[ix], Zr_elem);
//        timer_e += clock() - start;
//		m_socket.write_Bytes(my_element_to_bytes(Zr_elem));
//		sz += element_length_in_bytes(Zr_elem);
//	}
//#elif defined BOB
//	bool ret = true;
//    for (size_t ix = 0; ix < C_array.size(); ix++) // getting response a[ix]-c*r[ix]
//    {
//        // check if A[ix] = h^{a[ix]-c*r[ix]} * C[ix]^c
//        my_element_from_bytes(Zr_elem, m_socket.read_Bytes());
//        start = clock();
//            element_pp_pow_zn(G1_elem, Zr_elem, pp_h);          // G1_elem = h^{a[ix]-c*r[ix]}
//            element_pow_zn(&*C_array[ix], &*C_array[ix], c);    // C[ix] = C[ix]^c
//            element_mul(&*C_array[ix], &*C_array[ix], G1_elem); // C[ix] = G1_elem * C[ix]
//
//            ret &= (element_cmp(&*A_array[ix], &*C_array[ix]) == 0);
//        timer_g += clock() - start;
//        sz += element_length_in_bytes(Zr_elem);
//    }
//
//    if (ret)
//    {
//        LOG4CXX_INFO(logger, "Two-output succeeded");
//    }
//    else
//    {
//        LOG4CXX_INFO(logger, "Two-output failed.");
//    }
//#endif
//
//	// clear up
//	for (size_t ix = 0; ix < M_array.size(); ix++)
//	{
//#ifdef ALICE
//		element_clear(&*a_array[ix]);
//		delete a_array[ix];
//#elif defined BOB
//		element_clear(&*A_array[ix]);
//		delete A_array[ix];
//#endif
//		element_clear(&*M_array[ix]);
//		element_clear(&*r_array[ix]);
//		element_clear(&*C_array[ix]);
//
//		delete M_array[ix];
//		delete r_array[ix];
//		delete C_array[ix];
//	}
//
//	element_clear(G1_elem);
//	element_clear(Zr_elem);
//	element_clear(g);
//	element_clear(h);
//	element_clear(c);
//
//	return sz;
//}
}

////
//// Implementation of "Two-Output Secure Computation with Malicious Adversaries"
//// by abhi shelat and Chih-hao Shen from EUROCRYPT'11 (Protocol 2)
////
//// The evaluator (sender) generates m_ot_bit_cnt pairs of k-bit random strings, and
//// the generator (receiver) has input m_ot_bits and will receive output m_ot_out.
////
//uint64_t BetterYao::ot_init()
//{
//	double start;
//	uint64_t comm_sz = 0;
//
//	start = MPI_Wtime();
//		std::vector<Bytes> bufr_chunks;
//		Bytes bufr(Env::elm_size_in_bytes()*4);
//
//		Z y, a;
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	// step 1: ZKPoK of the CRS: g[0], h[0], g[1], h[1]
//	if (Env::is_root())
//	{
//		EVL_BEGIN // evaluator (OT sender)
//			start = MPI_Wtime();
//				bufr = EVL_RECV();
//			m_timer_com += MPI_Wtime() - start;
//		EVL_END
//
//		GEN_BEGIN // generator (OT receiver)
//			start = MPI_Wtime();
//				y.random();
//
//				a.random();
//
//				m_ot_g[0].random();
//				m_ot_g[1] = m_ot_g[0]^y;          // g[1] = g[0]^y
//
//				m_ot_h[0] = m_ot_g[0]^a;          // h[0] = g[0]^a
//				m_ot_h[1] = m_ot_g[1]^(a + Z(1)); // h[1] = g[1]^(a+1)
//
//				bufr.clear();
//				bufr += m_ot_g[0].to_bytes();
//				bufr += m_ot_g[1].to_bytes();
//				bufr += m_ot_h[0].to_bytes();
//				bufr += m_ot_h[1].to_bytes();
//			m_timer_gen += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				GEN_SEND(bufr);
//			m_timer_com += MPI_Wtime() - start;
//		GEN_END
//
//	    comm_sz += bufr.size();
//	}
//
//	// send g[0], g[1], h[0], h[1] to slave processes
//	start = MPI_Wtime();
//		MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm);
//	m_timer_mpi += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		bufr_chunks = bufr.split(Env::elm_size_in_bytes());
//
//		m_ot_g[0].from_bytes(bufr_chunks[0]);
//		m_ot_g[1].from_bytes(bufr_chunks[1]);
//		m_ot_h[0].from_bytes(bufr_chunks[2]);
//		m_ot_h[1].from_bytes(bufr_chunks[3]);
//
//		// group element pre-processing
//		m_ot_g[0].fast_exp();
//		m_ot_g[1].fast_exp();
//		m_ot_h[0].fast_exp();
//		m_ot_h[1].fast_exp();
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	return comm_sz;
//}
//
//
//uint64_t BetterYao::ot_random()
//{
//	double start;
//	uint64_t comm_sz = 0;
//
//	start = MPI_Wtime();
//		Bytes send, recv;
//		std::vector<Bytes> recv_chunks;
//
//		Z r, s[2], t[2];
//		G gr, hr, X[2], Y[2];
//
//		m_ot_out.clear();
//		m_ot_out.reserve(2*m_ot_bit_cnt); // the receiver only uses half of it
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	EVL_BEGIN // evaluator (OT sender)
//		for (size_t bix = 0; bix < m_ot_bit_cnt; bix++)
//		{
//			// Step 1: gr=g[b]^r, hr=h[b]^r, where b is the receiver's bit
//			start = MPI_Wtime();
//				recv.clear();
//				recv += EVL_RECV(); // receive gr, hr
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += recv.size();
//
//			// Step 2: the evaluator computes X[0], Y[0], X[1], Y[1]
//			start = MPI_Wtime();
//				recv_chunks = recv.split(Env::elm_size_in_bytes());
//
//				gr.from_bytes(recv_chunks[0]);
//				hr.from_bytes(recv_chunks[1]);
//
//				Y[0].random(); Y[1].random(); // K[0], K[1] sampled at random
//
//				m_ot_out.push_back(Y[0].to_bytes().hash(Env::k()));
//				m_ot_out.push_back(Y[1].to_bytes().hash(Env::k()));
//
//				s[0].random(); s[1].random();
//				t[0].random(); t[1].random();
//
//				// X[b] = ( g[b]^s[b] ) * ( h[b]^t[b] ) for b = 0, 1
//				X[0] = m_ot_g[0]^s[0]; X[0] *= m_ot_h[0]^t[0];
//				X[1] = m_ot_g[1]^s[1]; X[1] *= m_ot_h[1]^t[1];
//
//				// Y[b] = ( gr^s[b] ) * ( hr^t[b] ) * K[b] for b = 0, 1
//				Y[0] *= gr^s[0]; Y[0] *= hr^t[0];
//				Y[1] *= gr^s[1]; Y[1] *= hr^t[1];
//
//				send.clear();
//				send += X[0].to_bytes();
//				send += X[1].to_bytes();
//				send += Y[0].to_bytes();
//				send += Y[1].to_bytes();
//			m_timer_evl += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				EVL_SEND(send);
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += send.size();
//		}
//
//		assert(m_ot_out.size() == 2*m_ot_bit_cnt);
//	EVL_END
//
//	GEN_BEGIN // generator (OT receiver)
//		assert(m_ot_recv_bits.size() >= ((m_ot_bit_cnt+7)/8));
//
//		for (size_t bix = 0; bix < m_ot_bit_cnt; bix++)
//		{
//			// Step 1: gr=g[b]^r, hr=h[b]^r, where b is the receiver's bit
//			start = MPI_Wtime();
//				int bit_value = m_ot_recv_bits.get_ith_bit(bix);
//
//				r.random();
//
//				gr = m_ot_g[bit_value]^r;
//				hr = m_ot_h[bit_value]^r;
//
//				send.clear();
//				send += gr.to_bytes();
//				send += hr.to_bytes();
//			m_timer_gen += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				GEN_SEND(send);
//
//				// Step 2: the evaluator computes X[0], Y[0], X[1], Y[1]
//				recv.clear();
//				recv += GEN_RECV(); // receive X[0], Y[0], X[1], Y[1]
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += send.size() + recv.size();
//
//			// Step 3: the evaluator computes K = Y[b]/X[b]^r
//			start = MPI_Wtime();
//				recv_chunks = recv.split(Env::elm_size_in_bytes());
//
//				X[bit_value].from_bytes(recv_chunks[    bit_value]); // X[b]
//				Y[bit_value].from_bytes(recv_chunks[2 + bit_value]); // Y[b]
//
//				// K = Y[b]/(X[b]^r)
//				Y[bit_value] /= X[bit_value]^r;
//				m_ot_out.push_back(Y[bit_value].to_bytes().hash(Env::k()));
//			m_timer_gen += MPI_Wtime() - start;
//		}
//
//		assert(m_ot_out.size() == m_ot_bit_cnt);
//	GEN_END
//
//	return comm_sz;
//}
//
//
////
//// The evaluator (sender) has input m_ot_send_pairs (m_ot_bit_cnt pairs of l-bit strings), and
//// the generator (receiver) has input m_ot_recv_bits (m_ot_bit_cnt bits) and will receive output m_ot_out.
////
//uint64_t BetterYao::ot(uint32_t l)
//{
//	double start;
//	uint64_t comm_sz = 0;
//
//	Bytes send, recv;
//	vector<Bytes> recv_chunks;
//	Prng prng;
//
//	comm_sz += ot_random();
//
//	EVL_BEGIN // evaluator (OT sender)
//		assert(m_ot_send_pairs.size() == m_ot_out.size());
//
//		vector<Bytes>::iterator ot_out_it = m_ot_out.begin();
//		vector<Bytes>::const_iterator send_pairs_it = m_ot_send_pairs.begin();
//
//		while (ot_out_it != m_ot_out.end())
//		{
//			start = MPI_Wtime();
//				send.clear();
//
//				// send m_ot_send_pairs[2*bix+b] masked with PRNG(m_ot_out[2*bix+b]) and
//				// let m_ot_out[2*bix+b] = m_ot_send_pairs[2*bix+b] for b = 0, 1
//				prng.srand(*ot_out_it);
//				*ot_out_it = *send_pairs_it;
//				send += *send_pairs_it ^ prng.rand(l);
//				ot_out_it++; send_pairs_it++;
//
//				prng.srand(*ot_out_it);
//				*ot_out_it = *send_pairs_it;
//				send += *send_pairs_it ^ prng.rand(l);
//				ot_out_it++; send_pairs_it++;
//			m_timer_evl += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				EVL_SEND(send);
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += send.size();
//		}
//	EVL_END
//
//	GEN_BEGIN // generator (OT receiver)
//		vector<Bytes>::iterator ot_out_it = m_ot_out.begin();
//
//		for (size_t bix = 0; bix < m_ot_bit_cnt; bix++, ot_out_it++)
//		{
//			start = MPI_Wtime();
//				recv = GEN_RECV();
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += recv.size();
//
//			start = MPI_Wtime();
//				// receive masked m_ot_send_pairs[2*bix+0, 2*bix+1] and decrypt it with PRNG(m_ot_out)
//				recv_chunks = recv.split((l+7)/8);
//				byte bit_value = ((m_ot_recv_bits[bix/8]) >> (bix%8)) & 0x01;
//				prng.srand(*ot_out_it);
//				*ot_out_it = recv_chunks[bit_value] ^ prng.rand(l);
//			m_timer_gen += MPI_Wtime() - start;
//		}
//	GEN_END
//
//	return comm_sz;
//}
//
//
////
//// Parallelizing Fig. 2 of "Extending Oblivious Transfers Efficiently"
//// by Ishai, Kilian, Nissim, and Petrank from CRYPTO'03
////
//uint64_t BetterYao::ot_ext_random(const size_t sigma, const size_t k, const size_t l)
//{
//	double start;
//	uint64_t comm_sz = 0;
//
//	Bytes send, recv, bufr;
//	vector<Bytes> bufr_chunks;
//
//	vector<Bytes>                   C, S, R, W;
//	vector<vector<Bytes> >          T;
//	vector<vector<Bytes> >         &Q = T;
//	vector<vector<vector<Bytes> > > X, Y;
//
//	start = MPI_Wtime();
//		// expand bit_cnt such that workload for each node is balanced
//		uint32_t m = ((m_ot_ext_bit_cnt+Env::node_amnt()-1)/Env::node_amnt())*Env::node_amnt();
//		Bytes    r = m_ot_ext_recv_bits;
//		r.resize((m+7)/8);
//
//		// expand the amount of seed OT such that workload for each node is balanced
//		size_t new_sigma = (sigma+Env::node_amnt()-1)/Env::node_amnt(); // number of copies for each process
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	// Step 1. allocating S, X, R, and T
//	EVL_BEGIN // evaluator (OT-EXT receiver)
//		assert(m_ot_ext_recv_bits.size() >= ((m_ot_ext_bit_cnt+7)/8));
//
//		start = MPI_Wtime();
//			R.resize(new_sigma); // new_sigma x m
//			T.resize(new_sigma); // new_sigma x k x m
//
//			for (size_t px = 0; px < new_sigma; px++)
//			{
//				R[px] = m_prng.rand(m);
//				T[px].resize(k);
//
//				for (size_t jx = 0; jx < k; jx++)
//				{
//					T[px][jx] = m_prng.rand(m);
//				}
//			}
//		m_timer_evl += MPI_Wtime() - start;
//	EVL_END
//
//	GEN_BEGIN // generator (OT-EXT sender)
//		start = MPI_Wtime();
//			S.resize(new_sigma); // new_sigma x k
//			X.resize(new_sigma); // new_sigma x m x 2 x l
//			Q.resize(new_sigma);
//
//			for (size_t px = 0; px < new_sigma; px++)
//			{
//				S[px] = m_prng.rand(k);
//				X[px].resize(m);
//
//				for (size_t jx = 0; jx < m; jx++)
//				{
//					X[px][jx].resize(2);
//					X[px][jx][0] = m_prng.rand(l);
//					X[px][jx][1] = m_prng.rand(l);
//				}
//			}
//		m_timer_gen += MPI_Wtime() - start;
//	GEN_END
//
//	m_ot_send_pairs.reserve(2*k);
//
//	// Step 2: run real OT
//	for (size_t px = 0; px < new_sigma; px++)
//	{
//		m_ot_bit_cnt = k;
//
//		EVL_BEGIN // evaluator (OT-EXT receiver/OT sender)
//			start = MPI_Wtime();
//				m_ot_send_pairs.clear();
//				m_ot_send_pairs.reserve(2*k);
//				for (size_t ix = 0; ix < k; ix++)
//				{
//					m_ot_send_pairs.push_back(T[px][ix]);
//					m_ot_send_pairs.push_back(T[px][ix]^R[px]);
//				}
//			m_timer_evl += MPI_Wtime() - start;
//		EVL_END
//
//		GEN_BEGIN // generator (OT-EXT sender/OT receiver)
//			start = MPI_Wtime();
//				m_ot_recv_bits = S[px];
//			m_timer_gen += MPI_Wtime() - start;
//		GEN_END
//
//		// real OT
//		comm_sz += ot(m);
//
//		GEN_BEGIN // generator (OT-EXT sender/OT receiver)
//			start = MPI_Wtime();
//				Q[px].resize(k);
//				for (size_t ix = 0; ix < k; ix++)
//				{
//					Q[px][ix] = m_ot_out[ix]; // also Q for the generator
//				}
//			m_timer_gen += MPI_Wtime() - start;
//		GEN_END
//	}
//
//	// Step 3: cut-and-choose
//	if (Env::is_root())
//	{
//		EVL_BEGIN // evaluator (OT-EXT receiver)
//			start = MPI_Wtime();
//				m_all_chks = EVL_RECV();
//			m_timer_com += MPI_Wtime() - start;
//		EVL_END
//
//		GEN_BEGIN // generator (OT-EXT sender)
//			// random permutation in order to find the checking set
//			start = MPI_Wtime();
//				vector<int> indices(new_sigma*Env::node_amnt());
//
//				for (size_t px = 0; px < indices.size(); px++) { indices[px] = px; }
//				for (size_t px = 0; px < indices.size(); px++)
//				{
//					int rand_ix = m_prng.rand_range(indices.size()-px);
//					std::swap(indices[px], indices[px+rand_ix]);
//				}
//
//				m_all_chks.resize(indices.size(), 0);
//				for (size_t px = 0; px < indices.size()/2; px++)
//					m_all_chks[indices[px]] = 1;
//			m_timer_gen += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				GEN_SEND(m_all_chks);
//			m_timer_com += MPI_Wtime() - start;
//		GEN_END
//
//		comm_sz += m_all_chks.size();
//	}
//
//	start = MPI_Wtime();
//		m_chks.resize(new_sigma);
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		MPI_Scatter(&m_all_chks[0], m_chks.size(), MPI_BYTE, &m_chks[0], m_chks.size(), MPI_BYTE, 0, m_mpi_comm);
//	m_timer_mpi += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		m_all_chks.resize(new_sigma*Env::node_amnt());
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		MPI_Bcast(&m_all_chks[0], m_all_chks.size(), MPI_BYTE, 0, m_mpi_comm);
//	m_timer_mpi += MPI_Wtime() - start;
//
//	int verify = 1;
//
//	// check the chosen seed OTs
//	for (size_t px = 0; px < m_chks.size(); px++)
//	{
//		if (!m_chks[px])
//			continue;
//
//		EVL_BEGIN // evaluator (OT-EXT receiver)
//			start = MPI_Wtime();
//				send = R[px];
//				bufr = Bytes(T[px]);
//			m_timer_evl += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				EVL_SEND(send);
//				EVL_SEND(bufr);
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += send.size() + bufr.size();
//		EVL_END
//
//		GEN_BEGIN // generator (OT-EXT sender)
//			start = MPI_Wtime();
//				recv = GEN_RECV();
//				bufr = GEN_RECV();
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += recv.size() + bufr.size();
//
//			start = MPI_Wtime();
//				Bytes         r_px = recv;
//				vector<Bytes> Q_px = bufr.split((m+7)/8);
//
//				for (size_t ix = 0; ix < k; ix++)
//				{
//					verify &= S[px].get_ith_bit(ix)?
//						(Q[px][ix] == (Q_px[ix] ^ r_px)) :
//						(Q[px][ix] == Q_px[ix]);
//				}
//			m_timer_gen += MPI_Wtime() - start;
//		GEN_END
//	}
//
//	// gather the cut-and-choose results
//	GEN_BEGIN // generator (OT-EXT sender)
//		int all_verify;
//		start = MPI_Wtime();
//			MPI_Reduce(&verify, &all_verify, 1, MPI_INT, MPI_LAND, 0, m_mpi_comm);
//		m_timer_mpi += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			if (Env::is_root() && !all_verify)
//			{
//				LOG4CXX_FATAL(logger, "Verification failed");
//				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
//			}
//		m_timer_gen += MPI_Wtime() - start;
//	GEN_END
//
//	//
//	// Step 4: calculate Y
//	//
//
//	// matrix transpose from sigma * k * m -> sigma * m * k (for matrix T and Q)
//	start = MPI_Wtime();
//		vector<vector<Bytes> >  src = T;
//		vector<vector<Bytes> > &dst = T;
//
//		for (size_t px = 0; px < src.size(); px++)
//		{
//			dst[px].clear();
//			dst[px].resize(m);
//			for (size_t ix = 0; ix < m; ix++)
//			{
//				dst[px][ix].resize((k+7)/8, 0);
//				for (size_t jx = 0; jx < k; jx++)
//				{
//					dst[px][ix].set_ith_bit(jx, src[px][jx].get_ith_bit(ix));
//				}
//			}
//		}
//	m_timer_evl += MPI_Wtime() - start;
//	m_timer_gen += MPI_Wtime() - start;
//
//	// send over Y and share with other slave processes
//	EVL_BEGIN // evaluator (OT-EXT receiver/OT sender)
//		start = MPI_Wtime();
//			recv = EVL_RECV();
//		m_timer_com += MPI_Wtime() - start;
//
//		comm_sz += recv.size();
//
//		start = MPI_Wtime();
//			bufr.resize(recv.size()*Env::node_amnt());
//		m_timer_evl += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			MPI_Allgather(&recv[0], recv.size(), MPI_BYTE, &bufr[0], recv.size(), MPI_BYTE, m_mpi_comm);
//		m_timer_mpi += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			bufr_chunks = bufr.split((l+7)/8);
//
//			Y.resize(new_sigma*Env::node_amnt());
//			for (size_t px = 0, idx = 0; px < Y.size(); px++)
//			{
//				Y[px].resize(m);
//				for (size_t jx = 0; jx < m; jx++)
//				{
//					Y[px][jx].resize(2);
//
//					Y[px][jx][0] = bufr_chunks[idx++];
//					Y[px][jx][1] = bufr_chunks[idx++];
//				}
//			}
//		m_timer_evl += MPI_Wtime() - start;
//	EVL_END
//
//	GEN_BEGIN // generator (OT-EXT sender/OT receiver)
//		start = MPI_Wtime();
//			send.clear();
//			send.reserve(new_sigma*m*2*((l+7)/8));
//
//			for (size_t px = 0; px < new_sigma; px++)
//			{
//				for (size_t jx = 0; jx < m; jx++)
//				{
//					send += X[px][jx][0] ^ (T[px][jx]).hash(l);
//					send += X[px][jx][1] ^ (T[px][jx]^S[px]).hash(l);
//				}
//			}
//		m_timer_gen += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			GEN_SEND(send);
//		m_timer_com += MPI_Wtime() - start;
//
//		comm_sz += send.size();
//	GEN_END
//
//	EVL_BEGIN // evaluator (OT-EXT receiver)
//		start = MPI_Wtime();
//			// share r
//			recv.clear(); recv.reserve(R.size()*((m+7)/8));
//			for (size_t px = 0; px < R.size(); px++) { recv += R[px]; }
//
//			bufr.resize(recv.size()*Env::node_amnt());
//		m_timer_evl += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			MPI_Allgather(&recv[0], recv.size(), MPI_BYTE, &bufr[0], recv.size(), MPI_BYTE, m_mpi_comm);
//		m_timer_mpi += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			R = bufr.split((m+7)/8);
//
//			// share T
//			recv.clear(); recv.reserve(T.size()*m*((k+7)/8));
//			for (size_t px = 0; px < T.size(); px++)
//				recv += Bytes(T[px]);
//
//			bufr.resize(recv.size()*Env::node_amnt());
//		m_timer_evl += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			MPI_Allgather(&recv[0], recv.size(), MPI_BYTE, &bufr[0], recv.size(), MPI_BYTE, m_mpi_comm);
//		m_timer_mpi += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			bufr_chunks = bufr.split(m*((k+7)/8));
//			T.resize(new_sigma*Env::node_amnt());
//			for (size_t px = 0; px < T.size(); px++)
//			{
//				T[px] = bufr_chunks[px].split((k+7)/8);
//			}
//		m_timer_evl += MPI_Wtime() - start;
//	EVL_END
//
//	GEN_BEGIN // generator (OT-EXT sender)
//		start = MPI_Wtime();
//			// share X
//			recv.clear(); recv.reserve(new_sigma*m*2*((l+7)/8));
//			for (size_t px = 0; px < X.size(); px++)
//			{
//				for (size_t jx = 0; jx < X[px].size(); jx++)
//				{
//					recv += X[px][jx][0];
//					recv += X[px][jx][1];
//				}
//			}
//			bufr.resize(recv.size()*Env::node_amnt());
//		m_timer_gen += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			MPI_Allgather(&recv[0], recv.size(), MPI_BYTE, &bufr[0], recv.size(), MPI_BYTE, m_mpi_comm);
//		m_timer_mpi += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			bufr_chunks = bufr.split((l+7)/8);
//
//			X.resize(new_sigma*Env::node_amnt());
//			for (size_t px = 0, idx = 0; px < X.size(); px++)
//			{
//				X[px].resize(m);
//				for (size_t jx = 0; jx < m; jx++)
//				{
//					X[px][jx].resize(2);
//
//					X[px][jx][0] = bufr_chunks[idx++];
//					X[px][jx][1] = bufr_chunks[idx++];
//				}
//			}
//		m_timer_gen += MPI_Wtime() - start;
//	GEN_END
//
//	// send c
//	size_t local_bit_cnt = m / Env::node_amnt();
//	size_t local_bit_beg = local_bit_cnt *  Env::group_rank();
//	size_t local_bit_end = local_bit_cnt * (Env::group_rank()+1);
//
//	EVL_BEGIN // evaluator (OT-EXT receiver)
//		start = MPI_Wtime();
//			send.clear();
//			for (size_t px = 0; px < R.size(); px++)
//				for (size_t jx = local_bit_beg; jx < local_bit_end; jx++)
//			{
//				send.push_back(r.get_ith_bit(jx) ^ R[px].get_ith_bit(jx));
//			}
//		m_timer_evl += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			EVL_SEND(send);
//		m_timer_com += MPI_Wtime() - start;
//
//		comm_sz += send.size();
//	EVL_END
//
//	GEN_BEGIN // generator (OT-EXT sender)
//		start = MPI_Wtime();
//			recv.clear();
//			recv += GEN_RECV();
//		m_timer_com += MPI_Wtime() - start;
//
//		comm_sz += recv.size();
//
//		start = MPI_Wtime();
//			C = recv.split(local_bit_cnt);
//		m_timer_gen += MPI_Wtime() - start;
//	GEN_END
//
//	// send W
//	EVL_BEGIN // evaluator (OT-EXT receiver)
//		start = MPI_Wtime();
//			recv.clear();
//			recv += EVL_RECV();
//		m_timer_com += MPI_Wtime() - start;
//
//		comm_sz += recv.size();
//
//		start = MPI_Wtime();
//			W = recv.split((l+7)/8);
//			m_ot_ext_out.reserve(W.size());
//			for (size_t jx = local_bit_beg, bix = 0; jx < local_bit_end; jx ++, bix++)
//				m_ot_ext_out.push_back(W[2*bix + r.get_ith_bit(jx)]);
//
//			for (size_t px = 0; px < m_all_chks.size(); px++)
//			{
//				if (m_all_chks[px])
//					continue;
//
//				for (size_t jx = local_bit_beg, bix = 0; jx < local_bit_end; jx ++, bix++)
//				{
//					m_ot_ext_out[bix] ^=
//						Y[px][jx][R[px].get_ith_bit(jx)] ^ T[px][jx].hash(l);
//				}
//			}
//		m_timer_evl += MPI_Wtime() - start;
//	EVL_END
//
//	GEN_BEGIN // generator (OT-EXT sender)
//		start = MPI_Wtime();
//			W.reserve((m+Env::node_amnt()-1)/Env::node_amnt()*2);
//
//			for (size_t jx = local_bit_beg; jx < local_bit_end; jx ++)
//			{
//				W.push_back(m_prng.rand(l));
//				W.push_back(m_prng.rand(l));
//			}
//			m_ot_ext_out = W;
//
//			for (size_t px = 0; px < m_all_chks.size(); px++)
//			{
//				if (m_all_chks[px])
//					continue;
//
//				for (size_t jx = local_bit_beg, bix = 0; jx < local_bit_end; jx ++, bix++)
//				{
//					W[2*bix+0] ^= X[px][jx][0^C[px][bix]];
//					W[2*bix+1] ^= X[px][jx][1^C[px][bix]];
//				}
//			}
//			send.clear();
//			for (size_t ix = 0; ix < W.size(); ix++) { send += W[ix]; }
//		m_timer_gen += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			GEN_SEND(send);
//		m_timer_com += MPI_Wtime() - start;
//
//		comm_sz += send.size();
//	GEN_END
//
//	return comm_sz;
//}
//
//
//uint64_t BetterYao::ot_ext()
//{
//	step_init();
//
//	double start;
//	uint64_t comm_sz = 0;
//
//	comm_sz += proc_evl_in();
//
//	m_ot_ext_bit_cnt = m_evl_inp_rand_cnt;
//
//	EVL_BEGIN
//		m_ot_ext_recv_bits = m_evl_inp_rand;
//	EVL_END
//
//	comm_sz += ot_init();
//	comm_sz += ot_ext_random(2*Env::k(), Env::k(), Env::k());
//
//	start = MPI_Wtime();
//		Bytes send, recv;
//		vector<Bytes> recv_chunks;
//
//		vector<Prng> prngs(m_ot_ext_out.size());
//		vector<Prng>::iterator prngs_it = prngs.begin();
//		vector<Bytes>::const_iterator out_it = m_ot_ext_out.begin();
//
//		for (; prngs_it != prngs.end(); prngs_it++, out_it++) { prngs_it->srand(*out_it); }
//
//		send.resize(Env::s()*m_ot_ext_out.size()*Env::key_size_in_bytes());
//		recv.resize(send.size());
//
//		send.clear();
//
//		for (size_t ix = 0; ix < Env::node_amnt(); ix++)
//			for (prngs_it = prngs.begin(); prngs_it != prngs.end(); prngs_it++)
//		{
//			send += prngs_it->rand(Env::k()*Env::node_load());
//		}
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		MPI_Alltoall
//		(
//			&send[0], send.size()/Env::node_amnt(), MPI_BYTE,
//			&recv[0], send.size()/Env::node_amnt(), MPI_BYTE,
//			m_mpi_comm
//		);
//	m_timer_mpi += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		recv_chunks = recv.split(Env::key_size_in_bytes());
//		m_ot_keys.resize(Env::node_load());
//		for (size_t ix = 0; ix < m_ot_keys.size(); ix++)
//		{
//			m_ot_keys[ix].reserve(m_ot_ext_bit_cnt*2);
//		}
//
//		out_it = recv_chunks.begin();
//
//		size_t bit_cnt = (Env::is_evl())? m_ot_ext_bit_cnt : m_ot_ext_bit_cnt*2;
//
//		for (size_t ix = 0; ix < bit_cnt; ix++)
//			for (size_t kx = 0; kx < Env::node_load(); kx++)
//		{
//			m_ot_keys[kx].push_back(*out_it++);
//		}
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	comm_sz += comb_evl_in();
//
//	step_report(comm_sz, "ob-transfer");
//
//	return comm_sz;
//}
//
//
////
//// Implementation of Random Combination Input technique in D.1 of ``Secure
//// Two-Party Computation is Practical'' by Benny Pinkas, Thomas Schneider,
//// Nigel Smart, and Stephen Williams. from ASIACRYPT'09
////
//uint64_t BetterYao::proc_evl_in()
//{
//	Bytes bufr;
//	vector<Bytes> bufr_chunk;
//
//	double start;
//	uint64_t comm_sz = 0;
//
//	// rename variables for better readability
//	uint32_t        n_2 = Env::circuit().evl_inp_cnt();
//	Bytes          &x_2 = m_evl_inp;
//
//	uint32_t &n_2_prime = m_evl_inp_rand_cnt;
//	Bytes    &x_2_prime = m_evl_inp_rand;
//
//	vector<Bytes>    &A = m_evl_inp_selector;
//
//	// n_2_prime = max(4*n_2, 8*s)
//	n_2_prime = std::max(4*n_2, 8*Env::s());
//
//	if (Env::is_root())
//	{
//		EVL_BEGIN // evaluator
//			start = MPI_Wtime();
//				// sampling new inputs
//				x_2_prime = m_prng.rand(n_2_prime);
//				x_2_prime.set_ith_bit(0, 1); // let first bit be zero for convenience
//
//				// sampling input selectors
//				A.resize(n_2);
//				for (size_t ix = 0; ix < A.size(); ix++)
//				{
//					A[ix] = m_prng.rand(n_2_prime);
//
//					// let x_2[i] == \sum_j A[i][j] & x_2_prime[j]
//					byte b = 0;
//					for (size_t jx = 0; jx < n_2_prime; jx++)
//						b ^= A[ix].get_ith_bit(jx) & x_2_prime.get_ith_bit(jx);
//
//					// flip A[ix][0] if b doesn't match x_2[ix]
//					A[ix].set_ith_bit(0, b^x_2.get_ith_bit(ix)^A[ix].get_ith_bit(0));
//				}
//				bufr = Bytes(A);
//			m_timer_evl += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				EVL_SEND(bufr);
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += bufr.size();
//		EVL_END
//
//		GEN_BEGIN // generator
//			start = MPI_Wtime();
//				bufr = GEN_RECV();
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += bufr.size();
//		GEN_END
//	}
//
//	// send x_2_prime to slave evaluator
//	EVL_BEGIN
//		start = MPI_Wtime();
//			x_2_prime.resize((n_2_prime+7)/8);
//		m_timer_evl += MPI_Wtime() - start;
//
//		start = MPI_Wtime();
//			MPI_Bcast(&x_2_prime[0], x_2_prime.size(), MPI_BYTE, 0, m_mpi_comm);
//		m_timer_mpi += MPI_Wtime() - start;
//	EVL_END
//
//	// send A to slave processes
//	start = MPI_Wtime();
//		bufr.resize(Env::circuit().evl_inp_cnt()*((n_2_prime+7)/8));
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		MPI_Bcast(&bufr[0], bufr.size(), MPI_BYTE, 0, m_mpi_comm);
//	m_timer_mpi += MPI_Wtime() - start;
//
//	start = MPI_Wtime();
//		A = bufr.split((n_2_prime+7)/8);
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	assert(A.size() == n_2);
//
//	return comm_sz;
//}
//
//
//void BetterYao::inv_proc_evl_in()
//{
//	EVL_BEGIN
//		Bytes new_evl_inp(m_evl_inp);
//
//		for (size_t bix = 0; bix < Env::circuit().evl_inp_cnt(); bix++)
//		{
//			byte bit = 0;
//			for (size_t ix = 0; ix < m_evl_inp_rand_cnt; ix++)
//			{
//				bit ^= m_evl_inp_selector[bix].get_ith_bit(ix)&m_evl_inp_rand.get_ith_bit(ix);
//			}
//			new_evl_inp.set_ith_bit(bix, bit);
//		}
//
//		std::cout
//				<< "     m_evl_inp: " << m_evl_inp.to_hex() << std::endl
//				<< "m_evl_rand_inp: " << m_evl_inp_rand.to_hex() << std::endl
//				<< "   new_evl_inp: " << new_evl_inp.to_hex() << std::endl;
//	EVL_END
//}
//
//
//uint64_t BetterYao::comb_evl_in()
//{
//	double start;
//	Bytes bufr;
//	vector<Bytes> bufr_chunk, rand_key_chunk;
//	uint64_t comm_sz = 0;
//
//	Prng prng;
//	Bytes R = prng.rand(Env::k());
//
//	for (size_t ix = 0; ix < Env::node_load(); ix++)
//	{
//		EVL_BEGIN // evaluator
//			start = MPI_Wtime();
//				bufr = EVL_RECV(); // retrieve the one-time-padded pairs
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += bufr.size();
//
//			start = MPI_Wtime();
//				bufr_chunk = bufr.split(Env::key_size_in_bytes());
//				for (size_t bix = 0; bix < m_evl_inp_rand_cnt; bix++)
//				{
//					m_ot_keys[ix][bix] ^= bufr_chunk[2*bix+m_evl_inp_rand.get_ith_bit(bix)];
//				}
//				rand_key_chunk = m_ot_keys[ix];
//			m_timer_evl += MPI_Wtime() - start;
//		EVL_END
//
//		GEN_BEGIN // generator
//			start = MPI_Wtime();
//				rand_key_chunk = prng.rand(Env::k()*m_evl_inp_rand_cnt).split(Env::key_size_in_bytes());
//
//				bufr.clear();
//				bufr.reserve(m_ot_keys[ix].size()*Env::key_size_in_bytes());
//
//				for (size_t bix = 0; bix < m_evl_inp_rand_cnt; bix++)
//				{
//					m_ot_keys[ix][2*bix+0] ^= rand_key_chunk[bix];
//					m_ot_keys[ix][2*bix+1] ^= rand_key_chunk[bix] ^ R;
//				}
//				bufr = Bytes(m_ot_keys[ix]);
//			m_timer_gen += MPI_Wtime() - start;
//
//			start = MPI_Wtime();
//				GEN_SEND(bufr);
//			m_timer_com += MPI_Wtime() - start;
//
//			comm_sz += bufr.size();
//		GEN_END
//
//		start = MPI_Wtime();
//			bufr_chunk = Bytes(Env::circuit().evl_inp_cnt()*Env::key_size_in_bytes()).split(Env::key_size_in_bytes());
//			for (size_t bix = 0; bix < bufr_chunk.size(); bix++)
//				for (size_t six = 0; six < m_evl_inp_rand_cnt; six++)
//			{
//				if (m_evl_inp_selector[bix].get_ith_bit(six))
//					bufr_chunk[bix] ^= rand_key_chunk[six];
//			}
//		m_timer_evl += MPI_Wtime() - start;
//		m_timer_gen += MPI_Wtime() - start;
//
//		EVL_BEGIN
//			start = MPI_Wtime();
//				m_ot_keys[ix] = bufr_chunk;
//			m_timer_evl += MPI_Wtime() - start;
//		EVL_END
//
//		GEN_BEGIN
//			start = MPI_Wtime();
//				m_ot_keys[ix].resize(Env::circuit().evl_inp_cnt()*2);
//				for (size_t bix = 0; bix < bufr_chunk.size(); bix++)
//				{
//					m_ot_keys[ix][2*bix+0] = bufr_chunk[bix];
//					m_ot_keys[ix][2*bix+1] = bufr_chunk[bix]^R;
//				}
//			m_timer_gen += MPI_Wtime() - start;
//		GEN_END
//	}
//
//	start = MPI_Wtime();
//		m_ot_ext_recv_bits = m_evl_inp;
//		m_ot_ext_bit_cnt = Env::circuit().evl_inp_cnt();
//	m_timer_gen += MPI_Wtime() - start;
//	m_timer_evl += MPI_Wtime() - start;
//
//	return comm_sz;
//}
