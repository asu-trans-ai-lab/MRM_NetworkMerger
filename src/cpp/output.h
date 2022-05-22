/* Portions Copyright 2019-2021 Xuesong Zhou and Peiheng Li, Cafer Avci

 * If you help write or modify the code, please also list your names here.
 * The reason of having Copyright info here is to ensure all the modified version, as a whole, under the GPL
 * and further prevent a violation of the GPL.
 *
 * More about "How to use GNU licenses for your own software"
 * http://www.gnu.org/licenses/gpl-howto.html
 */

 // Peiheng, 02/03/21, remove them later after adopting better casting
#pragma warning(disable : 4305 4267 4018)
// stop warning: "conversion from 'int' to 'float', possible loss of data"
#pragma warning(disable: 4244)

#ifdef _WIN32
#include "pch.h"
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <functional>
#include <stack>
#include <list>
#include <vector>
#include <map>
#include <omp.h>
#include "config.h"
#include "utils.h"
#include <filesystem>

using std::max;
using std::min;
using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::ifstream;
using std::ofstream;
using std::istringstream;

#include "DTA.h"
// FILE* g_pFileOutputLog = nullptr;

void g_OutputModelFiles(int mode)
{
	if (mode == 1)
	{

//		namespace fs = std::filesystem;

		FILE* g_pFileModelNode = fopen("merged_node.csv", "w");

		if (g_pFileModelNode != NULL)
		{
			fprintf(g_pFileModelNode, "node_id,node_no,layer_no,MRM_gate_flag,node_type,is_boundary,#_of_outgoing_nodes,activity_node_flag,agent_type,zone_id,cell_code,info_zone_flag,x_coord,y_coord\n");
			for (int i = 0; i < g_node_vector.size(); i++)
			{


				if (g_node_vector[i].node_id >= 0)  //for all physical links
				{

					fprintf(g_pFileModelNode, "%d,%d,%d,%d,%s,%d,%d,%d,%s, %ld,%s,%d,%f,%f\n",
						g_node_vector[i].node_id,
						g_node_vector[i].node_seq_no,
						g_node_vector[i].layer_no,
						g_node_vector[i].MRM_gate_flag,
						g_node_vector[i].node_type.c_str(),
						g_node_vector[i].is_boundary,
						g_node_vector[i].m_outgoing_link_seq_no_vector.size(),
						g_node_vector[i].is_activity_node,
						g_node_vector[i].agent_type_str.c_str(),

						g_node_vector[i].zone_org_id,
						g_node_vector[i].cell_str.c_str(),
						0,
						g_node_vector[i].x,
						g_node_vector[i].y
					);

				}

			}

			fclose(g_pFileModelNode);
		}
		else
		{
			dtalog.output() << "Error: File \\hybrid\\node.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
			g_program_stop();


		}

	}

	if (mode == 2)
	{
		FILE* g_pFileModelLink = fopen("merged_link.csv", "w");

		if (g_pFileModelLink != NULL)
		{
			fprintf(g_pFileModelLink, "link_id,link_no,layer_no,from_node_id,to_node_id,from_gate_flag,to_gate_flag,link_type,link_type_name,lanes,link_distance_VDF,free_speed,cutoff_speed,fftt,capacity,allow_uses,");
			fprintf(g_pFileModelLink, "BPR_plf,BPR_alpha,BPR_beta,QVDF_qdf,QVDF_alpha,QVDF_beta,QVDF_cd,QVDF_n,");
			fprintf(g_pFileModelLink, "geometry\n");

			//VDF_fftt1,VDF_cap1,VDF_alpha1,VDF_beta1
			for (int i = 0; i < g_link_vector.size(); i++)
			{
				if (g_link_vector[i].link_type <= -100)  // invisible link
				{
					continue;
				}

				fprintf(g_pFileModelLink, "%s,%d,%d,%d,%d,%d,%d,%d,%s,%d,%f,%f,%f,%f,%f,%s,%f,%f,%f,%f,%f,%f,%f,%f,",
					g_link_vector[i].link_id.c_str(),
					g_link_vector[i].link_seq_no,
					g_link_vector[i].layer_no,
					g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
					g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
					g_node_vector[g_link_vector[i].from_node_seq_no].MRM_gate_flag,
					g_node_vector[g_link_vector[i].to_node_seq_no].MRM_gate_flag,
					g_link_vector[i].link_type,
					g_link_vector[i].link_type_name.c_str(),
					g_link_vector[i].number_of_lanes,
					g_link_vector[i].link_distance_VDF,
					g_link_vector[i].free_speed,
					g_link_vector[i].v_congestion_cutoff,
					g_link_vector[i].free_flow_travel_time_in_min,
					g_link_vector[i].lane_capacity,
					g_link_vector[i].VDF_period[0].allowed_uses.c_str(),
					g_link_vector[i].VDF_period[0].peak_load_factor,
					g_link_vector[i].VDF_period[0].alpha,
					g_link_vector[i].VDF_period[0].beta,

					g_link_vector[i].VDF_period[0].queue_demand_factor,
					g_link_vector[i].VDF_period[0].Q_alpha,
					g_link_vector[i].VDF_period[0].Q_beta,
					g_link_vector[i].VDF_period[0].Q_cd,
					g_link_vector[i].VDF_period[0].Q_n
				);

				if (g_link_vector[i].geometry.size() > 0)
				{
					fprintf(g_pFileModelLink, "\"%s\",", g_link_vector[i].geometry.c_str());
				}
				else
				{
					fprintf(g_pFileModelLink, "\"LINESTRING (");

					fprintf(g_pFileModelLink, "%f %f,", g_node_vector[g_link_vector[i].from_node_seq_no].x, g_node_vector[g_link_vector[i].from_node_seq_no].y);
					fprintf(g_pFileModelLink, "%f %f", g_node_vector[g_link_vector[i].to_node_seq_no].x, g_node_vector[g_link_vector[i].to_node_seq_no].y);

					fprintf(g_pFileModelLink, ")\"");
				}

				fprintf(g_pFileModelLink, "\n");

			}


			fclose(g_pFileModelLink);
		}
		else
		{
			dtalog.output() << "Error: File hybrid\\link.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
			g_program_stop();

		}

	}
	if (mode == 3)
	{
		int connector_count = 0;
		for (int i = 0; i < g_link_vector.size(); i++)
		{
			if (g_link_vector[i].link_type == 1000)  // connector
			{
				connector_count += 1;
			}
		}

		if (connector_count >= 1)
		{
			FILE* g_pFileModelLink = fopen("access_link.csv", "w");

			if (g_pFileModelLink != NULL)
			{
				fprintf(g_pFileModelLink, "link_id,link_no,from_node_id,to_node_id,link_type,link_type_name,lanes,link_distance_VDF,free_speed,fftt,capacity,allow_uses,geometry\n");

				//VDF_fftt1,VDF_cap1,VDF_alpha1,VDF_beta1
				for (int i = 0; i < g_link_vector.size(); i++)
				{
					if (g_link_vector[i].link_type == 1000)  // connector
					{

						fprintf(g_pFileModelLink, "%s,%d,%d,%d,%d,%s,%d,%f,%f,%f,%f,%s,",
							g_link_vector[i].link_id.c_str(),
							g_link_vector[i].link_seq_no,
							g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
							g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
							g_link_vector[i].link_type,
							g_link_vector[i].link_type_name.c_str(),
							g_link_vector[i].number_of_lanes,
							g_link_vector[i].link_distance_VDF,
							g_link_vector[i].free_speed,
							g_link_vector[i].free_flow_travel_time_in_min,
							g_link_vector[i].lane_capacity,
							g_link_vector[i].VDF_period[0].allowed_uses.c_str()
						);

						if (g_link_vector[i].geometry.size() > 0)
						{
							fprintf(g_pFileModelLink, "\"%s\",", g_link_vector[i].geometry.c_str());
						}
						else
						{
							fprintf(g_pFileModelLink, "\"LINESTRING (");

							fprintf(g_pFileModelLink, "%f %f,", g_node_vector[g_link_vector[i].from_node_seq_no].x, g_node_vector[g_link_vector[i].from_node_seq_no].y);
							fprintf(g_pFileModelLink, "%f %f", g_node_vector[g_link_vector[i].to_node_seq_no].x, g_node_vector[g_link_vector[i].to_node_seq_no].y);

							fprintf(g_pFileModelLink, ")\"");
						}

						fprintf(g_pFileModelLink, "\n");

					}


				}

				fclose(g_pFileModelLink);
			}
			else
			{
				dtalog.output() << "Error: File access_link.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
				g_program_stop();

			}

		}

	}


}
