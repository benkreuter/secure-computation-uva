#ifndef YAOBASE_H_
#define YAOBASE_H_

#include <string>
#include <vector>

#include "mpi.h"

#include "Env.h"
#include "NetIO.h"


#ifdef GEN_CODE

	// User mode (code for the generator)
	#define GEN_BEGIN
	#define GEN_END
	#define EVL_BEGIN     if (0) {
	#define EVL_END       }
	#define GEN_SEND(d)   Env::remote()->write_bytes(d)
	#define EVL_RECV()    Env::remote()->read_bytes()
	#define EVL_SEND(d)   Env::remote()->write_bytes(d)
	#define GEN_RECV()    Env::remote()->read_bytes()

#elif defined EVL_CODE

	// User mode (code for the evaluator)
	#define GEN_BEGIN     if (0) {
	#define GEN_END       }
	#define EVL_BEGIN
	#define EVL_END
	#define GEN_SEND(d)   Env::remote()->write_bytes(d)
	#define EVL_RECV()    Env::remote()->read_bytes()
	#define EVL_SEND(d)   Env::remote()->write_bytes(d)
	#define GEN_RECV()    Env::remote()->read_bytes()

#else

	// Simulation mode
	#define GEN_BEGIN     if (!Env::is_evl()) {
	#define GEN_END       }
	#define GEN_SEND(d)   send_data(Env::world_rank()+1, (d))
	#define GEN_RECV()    recv_data(Env::world_rank()+1)
	#define EVL_BEGIN     if ( Env::is_evl()) {
	#define EVL_END       }
	#define EVL_SEND(d)   send_data(Env::world_rank()-1, (d))
	#define EVL_RECV()    recv_data(Env::world_rank()-1)

#endif


class YaoBase {
public:
	YaoBase(EnvParams &params);
	virtual ~YaoBase();

	virtual void start() = 0;

private:
	void init_cluster(EnvParams &params);
	void init_network(EnvParams &params);
	void init_environ(EnvParams &params);
	void init_private(EnvParams &params);

protected:
	// subroutines for the communication in the Simulation mode
	Bytes recv_data(int src_node);
	void send_data(int dst_node, const Bytes &data);

	// subroutines for profiling
	void step_init();
	void step_report(uint64_t comm_sz, std::string step_name);
	void final_report();

protected:
	// variables for MPI
	MPI_Comm            m_mpi_comm;

	// variables for profiling
	double              m_timer_gen;
	double              m_timer_evl;
	double              m_timer_com; // inter-cluster communication
	double              m_timer_mpi; // intra-cluster communication

	vector<double>      m_timer_gen_vec;
	vector<double>      m_timer_evl_vec;
	vector<double>      m_timer_mpi_vec;
	vector<double>      m_timer_com_vec;

	vector<std::string> m_step_name_vec;
	vector<uint64_t>    m_comm_sz_vec;

	// variables for Yao protocol
	Bytes               m_evl_inp;
	Bytes               m_gen_inp;
	Bytes               m_gen_out;
	Bytes               m_evl_out;

	Prng                m_prng;
};

#endif
