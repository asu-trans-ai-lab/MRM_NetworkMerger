/* Portions Copyright 2019 Xuesong Zhou and Peiheng Li
 *
 * If you help write or modify the code, please also list your names here.
 * The reason of having Copyright info here is to ensure all the modified version, as a whole, under the GPL
 * and further prevent a violation of the GPL.
 *
 * More about "How to use GNU licenses for your own software"
 * http://www.gnu.org/licenses/gpl-howto.html
 */

#ifdef _WIN32
#include "pch.h"
#endif

#include "config.h"
#include "utils.h"

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace std;

int main()
{
	// reset all the log files to defult 0: not output; if want to output these logs set to 1
	dtalog.output() << "MRMNetworkMerger Log" << std::fixed << std::setw(12) << '\n';
	dtalog.debug_level() = 0;
	dtalog.log_sig() = 0;
	dtalog.log_odme() = 0;
	dtalog.log_path() = 0;
	dtalog.log_dta() = 0;
	dtalog.log_ue() = 0;

	int column_generation_iterations = 0;
	int column_updating_iterations = 0;
	int ODME_iterations = 0;
	int sensitivity_analysis_iterations = 0;
	int number_of_memory_blocks = 4;
	float info_updating_freq_in_min = 5;
	int simulation_iterations = 0;

	int signal_updating_output = 0;
	// generate link performance and agent file
	int assignment_mode = 1;
	bool flag_default = false;
	int default_volume = 1;
	int link_length_in_meter_flag = 0;

	
	CCSVParser parser_settings;
	parser_settings.IsFirstLineHeader = false;

	// obtain initial flow values
	network_assignment(assignment_mode, column_generation_iterations, column_updating_iterations, ODME_iterations, sensitivity_analysis_iterations, simulation_iterations, number_of_memory_blocks);

	return 0;
}