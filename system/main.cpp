#include <iostream>
#include <log4cxx/propertyconfigurator.h>

#include "Yao.h"
#include "BetterYao.h"
#include "BetterYao2.h"


int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);

    log4cxx::PropertyConfigurator::configure("log4cxx.conf");

	if (argc < 8)
	{
		std::cout << "Usage:" << std::endl
			<< "\tbetteryao [secu_param] [stat_param] [circuit_file] [input_file] [ip_server] [port_base] [setting]" << std::endl
			<< std::endl
			<< "[secu_param]: multiple of 8 but 128 at most" << std::endl
			<< "[stat_param]: multiple of the cluster size" << std::endl
			<< " [ip_server]: the IP (not domain name) of the IP exchanger" << std::endl
			<< " [port_base]: for the \"IP address in use\" hassles" << std::endl
			<< "   [setting]: 0 = honest-but-curious; 1 = malicious (I+2C); 2 = malicious (I+C)" << std::endl
			<< std::endl;
		exit(EXIT_FAILURE);
	}

	EnvParams params;

	params.secu_param   = atoi(argv[1]);
	params.stat_param   = atoi(argv[2]);

	params.circuit_file = argv[3];
	params.private_file = argv[4];
	params.ipserve_addr = argv[5];

	params.port_base    = atoi(argv[6]);

	int setting         = atoi(argv[7]);

	YaoBase *sys;

	switch (setting)
	{
	case 0:
		sys = new Yao(params);
		break;

	case 1:
		sys = new BetterYao(params);
		break;

	case 2:
		sys = new BetterYao2(params);
		break;
	}

	sys->start();
    delete sys; // delete MPI objects before MPI_Finalize()

	MPI_Finalize();

	return 0;
}

