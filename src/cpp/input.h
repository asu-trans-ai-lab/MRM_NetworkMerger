/* Portions Copyright 2019-2021 Xuesong Zhou and Peiheng Li, Cafer Avci, Entai Wang

 * If you help write or modify the code, please also list your names here.
 * The reason of having Copyright info here is to ensure all the modified version, as a whole, under the GPL
 * and further prevent a violation of the GPL.
 *
 * More about "How to use GNU licenses for your own software"
 * http://www.gnu.org/licenses/gpl-howto.html
 */

 // Peiheng, 02/03/21, remove them later after adopting better casting
#pragma warning(disable : 4305 4267 4018 )
// stop warning: "conversion from 'int' to 'float', possible loss of data"
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#pragma warning(disable: 4477)



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
#include "geometry.h"

bool g_read_subarea_CSV_file(Assignment& assignment)
{
	CCSVParser parser;
	if (parser.OpenCSVFile("subarea.csv",false) )
	{
		while (parser.ReadRecord())
		{
			string geo_string;
			if (parser.GetValueByFieldName("geometry", geo_string))
			{
				// overwrite when the field "geometry" exists
				CGeometry geometry(geo_string);

				std::vector<CCoordinate> CoordinateVector = geometry.GetCoordinateList();

				if (CoordinateVector.size() > 0)
				{
					for (int p = 0; p < CoordinateVector.size(); p++)
					{
						GDPoint pt;
						pt.x = CoordinateVector[p].X;
						pt.y = CoordinateVector[p].Y;

						assignment.g_subarea_shape_points.push_back(pt);
					}
				}

				break;
			}

		}
		parser.CloseCSVFile();
	}


	return true;
}

bool g_read_micronet_subarea_CSV_file(Assignment& assignment)
{
	CCSVParser parser;
	if (parser.OpenCSVFile("hdnet\\subarea_MRM.csv", false))
	{
		while (parser.ReadRecord())
		{
			string geo_string;
			if (parser.GetValueByFieldName("geometry", geo_string))
			{
				// overwrite when the field "geometry" exists
				CGeometry geometry(geo_string);

				std::vector<CCoordinate> CoordinateVector = geometry.GetCoordinateList();

				if (CoordinateVector.size() > 0)
				{
					for (int p = 0; p < CoordinateVector.size(); p++)
					{
						GDPoint pt;
						pt.x = CoordinateVector[p].X;
						pt.y = CoordinateVector[p].Y;

						assignment.g_MRM_subarea_shape_points.push_back(pt);
					}
				}

				break;
			}

		}
		parser.CloseCSVFile();
		
		assignment.summary_file << ", # of shape point records in hdnet\\subarea_MRM.csv=," << assignment.g_subarea_shape_points.size() << "," << endl;

	}
	else
	{
		dtalog.output() << "Error: file hdnet\\subarea_MRM.csv is not found." << endl;
		g_program_stop();
	}
	return true;
}


int g_MRM_subarea_filtering(Assignment& assignment)
{
	int gate_micro_node_count = 0;
	int gate_macro_node_count = 0;

	if (g_read_micronet_subarea_CSV_file(assignment) == false) // MRM step 1:
		return 0;

	// micro file: reading nodes
	CCSVParser parser;
	if (assignment.g_MRM_subarea_shape_points.size() >= 3)  // with subarea MRM file
	{
		if (parser.OpenCSVFile("hdnet\\node.csv", true))
		{
			dtalog.output() << "reading hdnet\\node.csv" << endl;

			while (parser.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
			{
				int node_id;
				if (!parser.GetValueByFieldName("node_id", node_id))
					continue;

				// essential MRM step 1: add BIG M to the node id
				node_id = node_id + MICRONET_NODE_ID_BIG_M;

				double x_coord;
				double y_coord;

				parser.GetValueByFieldName("x_coord", x_coord, true, false);
				parser.GetValueByFieldName("y_coord", y_coord, true, false);


				// micro network filter:
				GDPoint pt;
				pt.x = x_coord;
				pt.y = y_coord;
				// essential MRM step 2: test if inside the micro subarea
				int micro_subarea_inside_flag = g_test_point_in_polygon(pt, assignment.g_MRM_subarea_shape_points);
				assignment.g_node_id_to_MRM_subarea_mapping[node_id] = micro_subarea_inside_flag;

			}

		}
		else
		{
			dtalog.output() << "Error: File hdnet\\node.csv is not found." << endl;
			g_program_stop();
		}

		parser.CloseCSVFile();

	}

	CCSVParser parser_link;

	if (parser_link.OpenCSVFile("hdnet\\link.csv", true))
	{
		dtalog.output() << "reading hdnet\\link.csv" <<endl;

		CLink link;

		while (parser_link.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
		{

			long from_node_id = -1;
			if (!parser_link.GetValueByFieldName("from_node_id", from_node_id))
				continue;

			from_node_id = from_node_id + MICRONET_NODE_ID_BIG_M;

			long to_node_id = -1;
			if (!parser_link.GetValueByFieldName("to_node_id", to_node_id))
				continue;

			to_node_id = to_node_id + MICRONET_NODE_ID_BIG_M;

			if (assignment.g_node_id_to_MRM_subarea_mapping.find(from_node_id) == assignment.g_node_id_to_MRM_subarea_mapping.end())
			{
				dtalog.output() << "Error: from_node_id " << from_node_id << " in file hdnet\\link.csv is not defined in hdnet\\node.csv." << endl;
				continue; //has not been defined
			}

			if (assignment.g_node_id_to_MRM_subarea_mapping.find(to_node_id) == assignment.g_node_id_to_MRM_subarea_mapping.end())
			{
				dtalog.output() << "Error: to_node_id " << to_node_id << " in file hdnet\\link.csv is not defined in hdnet\\node.csv." << endl;
				continue; //has not been defined
			}
// micro flagging
			// micro apply two flags
			if (assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 1 && assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 0)  // bridge link
			{
				assignment.g_micro_node_id_to_MRM_bridge_mapping[from_node_id] = 3;
				assignment.g_micro_node_id_to_MRM_bridge_mapping[to_node_id] = 2;

				gate_micro_node_count++;
			}

			if (assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 0 && assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 1)  // bridge link
			{
				assignment.g_micro_node_id_to_MRM_bridge_mapping[from_node_id] = 2;
				assignment.g_micro_node_id_to_MRM_bridge_mapping[to_node_id] = 3;
				gate_micro_node_count++;
			}

		}
		parser_link.CloseCSVFile();
	}
	else
	{
		dtalog.output() << "Error: File hdnet\\link.csv is not found." << endl;
		g_program_stop();

	}
	/////////////master/macro network;
	if (assignment.g_MRM_subarea_shape_points.size() >= 3)  // with subarea MRM file
	{
		if (parser.OpenCSVFile("node.csv", true))
		{
			while (parser.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
			{
				int node_id;
				if (!parser.GetValueByFieldName("node_id", node_id))
					continue;

				double x_coord;
				double y_coord;

				parser.GetValueByFieldName("x_coord", x_coord, true, false);
				parser.GetValueByFieldName("y_coord", y_coord, true, false);


				// micro network filter:
				GDPoint pt;
				pt.x = x_coord;
				pt.y = y_coord;
				// essential MRM step 2: test if inside the micro subarea
				int subarea_inside_flag = g_test_point_in_polygon(pt, assignment.g_MRM_subarea_shape_points);


				int zone_id = -1;
				if (parser.GetValueByFieldName("zone_id", zone_id) && zone_id >=1 && subarea_inside_flag ==1)
				{
					assignment.subareaMRM_zone_id_X_mapping[zone_id] = x_coord;
					assignment.subareaMRM_zone_id_Y_mapping[zone_id] = y_coord;
					
					//establish all the internal zone id to xy mapping 
				}
				// read zone center x and y


				assignment.g_node_id_to_MRM_subarea_mapping[node_id] = subarea_inside_flag;

			}

		}
		parser.CloseCSVFile();

	
		assignment.summary_file << ", # of inside zones," << assignment.subareaMRM_zone_id_Y_mapping.size() << "," << endl;

	if (parser_link.OpenCSVFile("link.csv", true))
	{

		CLink link;

		while (parser_link.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
		{

			long from_node_id = -1;
			if (!parser_link.GetValueByFieldName("from_node_id", from_node_id))
				continue;

			long to_node_id = -1;
			if (!parser_link.GetValueByFieldName("to_node_id", to_node_id))
				continue;


			if (assignment.g_node_id_to_MRM_subarea_mapping.find(from_node_id) == assignment.g_node_id_to_MRM_subarea_mapping.end())
			{
				dtalog.output() << "Error: from_node_id " << from_node_id << " in file link.csv is not defined in node.csv." << endl;
				continue; //has not been defined
			}

			if (assignment.g_node_id_to_MRM_subarea_mapping.find(to_node_id) == assignment.g_node_id_to_MRM_subarea_mapping.end())
			{
				dtalog.output() << "Error: to_node_id " << to_node_id << " in file link.csv is not defined in node.csv." << endl;
				continue; //has not been defined
			}

			if (from_node_id == 9718 && to_node_id == 9441)
			{

				int mapping_from = assignment.g_node_id_to_MRM_subarea_mapping[from_node_id];
				int mapping_to = assignment.g_node_id_to_MRM_subarea_mapping[to_node_id];
				int debug_flag = 1;

			}

			//
			if (assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 1 && assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 0)  // bridge link
			{
//macro flagging
				assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping[from_node_id] = 3;
				gate_macro_node_count++;
			}

			if (assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 0 && assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 1)
			{
				assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping[to_node_id] = 3;

				gate_macro_node_count++;
			}

		}
		parser_link.CloseCSVFile();
	}

	assignment.summary_file << ", # of micro gate nodes," << gate_micro_node_count << "," << endl;
	assignment.summary_file << ", # of macro gate nodes," << gate_macro_node_count << "," << endl;
	assignment.summary_file << ", # of macro outgoing gate nodes," << assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.size() << "," << endl;
	assignment.summary_file << ", # of macro incoming gate nodes," << assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.size() << "," << endl;

	}
	return gate_micro_node_count;
}

void g_add_new_bridge_link(int internal_from_node_seq_no, int internal_to_node_seq_no, string agent_type_str, int link_type)
{
	// create a link object
	CLink link;
	link.link_id = "bridge link ";
	link.layer_no = 2; // bridge 
	link.from_node_seq_no = internal_from_node_seq_no;
	link.to_node_seq_no = internal_to_node_seq_no;
	link.link_seq_no = assignment.g_number_of_links;
	link.to_node_seq_no = internal_to_node_seq_no;
	link.link_type = link_type;
	//only for outgoing connectors

	//BPR
	link.traffic_flow_code = 0;
	link.spatial_capacity_in_vehicles = 99999;
	link.lane_capacity = 999999;
	link.link_spatial_capacity = 99999;
	link.link_distance_VDF = 0.00001;
	link.free_flow_travel_time_in_min = 0.1;

	for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
	{
		//setup default values
		link.VDF_period[tau].lane_based_ultimate_hourly_capacity = 999999;
		// 60.0 for 60 min per hour
		link.VDF_period[tau].FFTT = 0.0001;
		link.VDF_period[tau].alpha = 0;
		link.VDF_period[tau].beta = 0;
		link.VDF_period[tau].allowed_uses = agent_type_str;

		link.travel_time_per_period[tau] = 0;

	}

	// add this link to the corresponding node as part of outgoing node/link
	g_node_vector[internal_from_node_seq_no].m_outgoing_link_seq_no_vector.push_back(link.link_seq_no);
	// add this link to the corresponding node as part of outgoing node/link
	g_node_vector[internal_to_node_seq_no].m_incoming_link_seq_no_vector.push_back(link.link_seq_no);
	// add this link to the corresponding node as part of outgoing node/link
	g_node_vector[internal_from_node_seq_no].m_to_node_seq_no_vector.push_back(link.to_node_seq_no);
	// add this link to the corresponding node as part of outgoing node/link
	g_node_vector[internal_from_node_seq_no].m_to_node_2_link_seq_no_map[link.to_node_seq_no] = link.link_seq_no;

	g_link_vector.push_back(link);

	assignment.g_number_of_links++;
}

int g_MRM_subarea_adding_bridge(Assignment& assignment)
{
	assignment.MRM_log_file << "start adding bridges between micro and macro gate nodes." << endl;


	int bridge_link_count = 0;

	std::vector<int> incoming_bridge_macro_link_no_vector;

	std::vector<int> incoming_bridge_micro_link_no_vector;

	std::vector<int> outgoing_bridge_macro_link_no_vector;

	std::vector<int> outgoing_bridge_micro_link_no_vector;


	for (int i = 0; i < g_link_vector.size(); ++i)
	{

		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;

		if (g_link_vector[i].layer_no == 0)  //macro node
		{
		

			if (from_node_id == 9481)
			{
				int debug_code = 1;
			}

			if (
				assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 0 && 
				assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.find(to_node_id) != assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.end() &&
				assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping[to_node_id] == 3)
			{
				incoming_bridge_macro_link_no_vector.push_back(i);
				g_link_vector[i].link_type = -200; // invisible
			}

			if (assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 0 && 
				assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.find(from_node_id) != assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.end() &&
				assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping[from_node_id] ==3 )
			{
				outgoing_bridge_macro_link_no_vector.push_back(i);
				g_link_vector[i].link_type = -200; // invisible
			}
		}

		if (g_link_vector[i].layer_no == 1)  //micro node
		{

			if (assignment.g_micro_node_id_to_MRM_bridge_mapping.find(from_node_id) != assignment.g_micro_node_id_to_MRM_bridge_mapping.end()
				&& 
				assignment.g_micro_node_id_to_MRM_bridge_mapping[from_node_id] == 2)
			{
				incoming_bridge_micro_link_no_vector.push_back(i);

			}
			if (assignment.g_micro_node_id_to_MRM_bridge_mapping.find(to_node_id) != assignment.g_micro_node_id_to_MRM_bridge_mapping.end()
				&&
				assignment.g_micro_node_id_to_MRM_bridge_mapping[to_node_id] == 2)
			{
				outgoing_bridge_micro_link_no_vector.push_back(i);

			}

		}
	}


	FILE* g_pFileModelNode = fopen("MRM_bridge_node.csv", "w");

	if (g_pFileModelNode != NULL)
	{
		fprintf(g_pFileModelNode, "node_id,node_no,layer_no,MRM_gate_flag,node_type,is_boundary,#_of_outgoing_nodes,activity_node_flag,agent_type,zone_id,cell_code,info_zone_flag,x_coord,y_coord\n");
	}
	else
	{
		dtalog.output() << "Error: file MRM_node.csv cannot be openned." << endl;
		g_program_stop();

	}

	FILE* g_pFileModelLink = fopen("MRM_bridge_link.csv", "w");

	if (g_pFileModelLink != NULL)
	{
		fprintf(g_pFileModelLink, "from_node_id,to_node_id,geometry\n");

	}
	else
	{
		dtalog.output() << "Error: file MRM_link.csv cannot be openned." << endl;
		g_program_stop();
	}





	int micro_link_index = 0;
	for (int microj = 0; microj < incoming_bridge_micro_link_no_vector.size(); ++microj)
	{
		int j = incoming_bridge_micro_link_no_vector[microj];
		if (g_link_vector[j].cell_type == 2)  // lane changing arc
		{
			continue;  //skip
		}
		int from_node_id = g_node_vector[g_link_vector[j].from_node_seq_no].node_id;
		int to_node_id = g_node_vector[g_link_vector[j].to_node_seq_no].node_id;
		assignment.MRM_log_file << "no." << micro_link_index << " micro link across the boundary " << from_node_id << "->" << to_node_id << "" << endl;

		int k = g_link_vector[j].from_node_seq_no;
		fprintf(g_pFileModelNode, "%d,%d,%d,%d,%s,%d,%d,%d,%s, %ld,%s,%d,%f,%f\n",
			g_node_vector[k].node_id,
			g_node_vector[k].node_seq_no,
			g_node_vector[k].layer_no,
			g_node_vector[k].MRM_gate_flag,
			g_node_vector[k].node_type.c_str(),
			g_node_vector[k].is_boundary,
			g_node_vector[k].m_outgoing_link_seq_no_vector.size(),
			g_node_vector[k].is_activity_node,
			g_node_vector[k].agent_type_str.c_str(),

			g_node_vector[k].zone_org_id,
			g_node_vector[k].cell_str.c_str(),
			0,
			g_node_vector[k].x,
			g_node_vector[k].y);

		micro_link_index++;
	}
	for (int microj = 0; microj < outgoing_bridge_micro_link_no_vector.size(); ++microj)
	{
		int j = outgoing_bridge_micro_link_no_vector[microj];
		if (g_link_vector[j].cell_type == 2)  // lane changing arc
		{
			continue;  //skip
		}
		int from_node_id = g_node_vector[g_link_vector[j].from_node_seq_no].node_id;
		int to_node_id = g_node_vector[g_link_vector[j].to_node_seq_no].node_id;
		assignment.MRM_log_file << "no." << micro_link_index << " micro link across the boundary " << from_node_id << "->" << to_node_id << "" << endl;

		int k = g_link_vector[j].to_node_seq_no;
		fprintf(g_pFileModelNode, "%d,%d,%d,%d,%s,%d,%d,%d,%s, %ld,%s,%d,%f,%f\n",
			g_node_vector[k].node_id,
			g_node_vector[k].node_seq_no,
			g_node_vector[k].layer_no,
			g_node_vector[k].MRM_gate_flag,
			g_node_vector[k].node_type.c_str(),
			g_node_vector[k].is_boundary,
			g_node_vector[k].m_outgoing_link_seq_no_vector.size(),
			g_node_vector[k].is_activity_node,
			g_node_vector[k].agent_type_str.c_str(),

			g_node_vector[k].zone_org_id,
			g_node_vector[k].cell_str.c_str(),
			0,
			g_node_vector[k].x,
			g_node_vector[k].y);

		micro_link_index++;
	}

	int macro_link_index = 0;
	for (int macroi = 0; macroi < incoming_bridge_macro_link_no_vector.size(); ++macroi)
	{

		int i = incoming_bridge_macro_link_no_vector[macroi];
		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;
		assignment.MRM_log_file << "no." << macro_link_index << " macro link across the boundary " << from_node_id << "->" << to_node_id << "" << endl;

		int k = g_link_vector[i].from_node_seq_no;
		fprintf(g_pFileModelNode, "%d,%d,%d,%d,%s,%d,%d,%d,%s, %ld,%s,%d,%f,%f\n",
			g_node_vector[k].node_id,
			g_node_vector[k].node_seq_no,
			g_node_vector[k].layer_no,
			g_node_vector[k].MRM_gate_flag,
			g_node_vector[k].node_type.c_str(),
			g_node_vector[k].is_boundary,
			g_node_vector[k].m_outgoing_link_seq_no_vector.size(),
			g_node_vector[k].is_activity_node,
			g_node_vector[k].agent_type_str.c_str(),

			g_node_vector[k].zone_org_id,
			g_node_vector[k].cell_str.c_str(),
			0,
			g_node_vector[k].x,
			g_node_vector[k].y);
		macro_link_index++;
	}

	for (int macroi = 0; macroi < outgoing_bridge_macro_link_no_vector.size(); ++macroi)
	{

		int i = outgoing_bridge_macro_link_no_vector[macroi];
		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;
		assignment.MRM_log_file << "no." << macro_link_index << " macro link across the boundary " << from_node_id << "->" << to_node_id << "" << endl;

		int k = g_link_vector[i].to_node_seq_no;
		fprintf(g_pFileModelNode, "%d,%d,%d,%d,%s,%d,%d,%d,%s, %ld,%s,%d,%f,%f\n",
			g_node_vector[k].node_id,
			g_node_vector[k].node_seq_no,
			g_node_vector[k].layer_no,
			g_node_vector[k].MRM_gate_flag,
			g_node_vector[k].node_type.c_str(),
			g_node_vector[k].is_boundary,
			g_node_vector[k].m_outgoing_link_seq_no_vector.size(),
			g_node_vector[k].is_activity_node,
			g_node_vector[k].agent_type_str.c_str(),

			g_node_vector[k].zone_org_id,
			g_node_vector[k].cell_str.c_str(),
			0,
			g_node_vector[k].x,
			g_node_vector[k].y);
		macro_link_index++;
	}
	fclose(g_pFileModelNode);

	// one micro to one macro matching 
	// from each micro incoming link, find the nearest macro incoming link  and add the connectors from the macro from node id to the micro to node id 

	for (int microj = 0; microj < incoming_bridge_micro_link_no_vector.size(); ++microj)
	{
		int j = incoming_bridge_micro_link_no_vector[microj];
		int matching_macro_node_i = -1;
		double min_distance = 99999;

		int from_node_id_micro = g_node_vector[g_link_vector[j].from_node_seq_no].node_id;

		if (g_link_vector[j].cell_type == 2)  // lane changing arc
		{
			continue;  //skip
		}

		// try 3 times
	for(int try_loop = 0; try_loop<3; try_loop++)
	{
		for (int macroi = 0; macroi < incoming_bridge_macro_link_no_vector.size(); ++macroi)
		{
			int i = incoming_bridge_macro_link_no_vector[macroi];

			int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
			int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;

			
			GDPoint p1, p2, p3, p4;
				p1.x = g_node_vector[g_link_vector[i].from_node_seq_no].x;
				p1.y = g_node_vector[g_link_vector[i].from_node_seq_no].y;

				p2.x = g_node_vector[g_link_vector[i].to_node_seq_no].x;
				p2.y = g_node_vector[g_link_vector[i].to_node_seq_no].y;

				p3.x = g_node_vector[g_link_vector[j].from_node_seq_no].x; //micro 
				p3.y = g_node_vector[g_link_vector[j].from_node_seq_no].y;

				p4.x = g_node_vector[g_link_vector[j].to_node_seq_no].x;
				p4.y = g_node_vector[g_link_vector[j].to_node_seq_no].y;

				// matching distance from micro cell to the macro link
				double local_distance2 = (g_GetPoint2Point_Distance(&p3, &p1) + g_GetPoint2Point_Distance(&p4, &p2)) / 2;
				double local_distance = (g_GetPoint2LineDistance(&p3, &p1, &p2) + g_GetPoint2LineDistance(&p4, &p1, &p2)) / 2 + local_distance2;

				double relative_angle= g_Find_PPP_RelativeAngle(&p1, &p2, &p3, &p4);

				if (try_loop == 0 && fabs(relative_angle) >= 45)  // first try to use restrictive condition
				{
					continue; 
				}

				if (try_loop == 1 && fabs(relative_angle) >= 90)  // second try to use restrictive condition
				{
					continue;
				}

				if (local_distance < min_distance)
				{
					min_distance = local_distance;
					matching_macro_node_i = g_link_vector[i].from_node_seq_no;
				}

		}
	

		if (matching_macro_node_i >= 0)
		{  // create a bridge connector with macro from node and micro to node
			int internal_from_node_seq_no = matching_macro_node_i;
			int internal_to_node_seq_no = g_link_vector[j].from_node_seq_no;


			//bool g_get_line_polygon_intersection(
			//	double Ax, double Ay,
			//	double Bx, double By,
			//	std::vector<GDPoint> subarea_shape_points)

			bool intersect_flag =  g_get_line_polygon_intersection(g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y,
				g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y,
				assignment.g_MRM_subarea_shape_points);

			if(intersect_flag == false)  // else give another try
			{

			string agent_type_str;
			g_add_new_bridge_link(internal_from_node_seq_no, internal_to_node_seq_no, agent_type_str, 0);


			fprintf(g_pFileModelLink, "%d,%d,", g_node_vector[internal_from_node_seq_no].node_id, g_node_vector[internal_to_node_seq_no].node_id);
			fprintf(g_pFileModelLink, "\"LINESTRING (");
			fprintf(g_pFileModelLink, "%f %f,", g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y);
			fprintf(g_pFileModelLink, "%f %f", g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y);
			fprintf(g_pFileModelLink, ")\"");
			fprintf(g_pFileModelLink, "\n");

			bridge_link_count++;
			try_loop = 100; //exit

			}
		}

		}

	}

	// one micro to one macro matching 
	// from each micro incoming link, find the nearest macro incoming link  and add the connectors from the macro from node id to the micro to node id 

	for (int microi = 0; microi < outgoing_bridge_micro_link_no_vector.size(); ++microi)
	{
		int i = outgoing_bridge_micro_link_no_vector[microi];
		int matching_macro_node_j = -1;
		double min_distance = 99999;

		if (g_link_vector[i].cell_type == 2)  // lane chaning arc
		{
			continue;  //skip
		}

		for (int try_loop = 0; try_loop < 3; try_loop++)
		{
			for (int macroj = 0; macroj < outgoing_bridge_macro_link_no_vector.size(); ++macroj)
			{
				int j = outgoing_bridge_macro_link_no_vector[macroj];

				GDPoint p1, p2, p3, p4;
				p1.x = g_node_vector[g_link_vector[j].from_node_seq_no].x;  // macro
				p1.y = g_node_vector[g_link_vector[j].from_node_seq_no].y;

				p2.x = g_node_vector[g_link_vector[j].to_node_seq_no].x;
				p2.y = g_node_vector[g_link_vector[j].to_node_seq_no].y;

				p3.x = g_node_vector[g_link_vector[i].from_node_seq_no].x;  // micro
				p3.y = g_node_vector[g_link_vector[i].from_node_seq_no].y;

				p4.x = g_node_vector[g_link_vector[i].to_node_seq_no].x;
				p4.y = g_node_vector[g_link_vector[i].to_node_seq_no].y;

				// matching distance from micro cell to the macro link
				double local_distance2 = (g_GetPoint2Point_Distance(&p3, &p1) + g_GetPoint2Point_Distance(&p4, &p2)) / 2;
				double local_distance = (g_GetPoint2LineDistance(&p3, &p1, &p2) + g_GetPoint2LineDistance(&p4, &p1, &p2)) / 2 + local_distance2;

				double relative_angle = g_Find_PPP_RelativeAngle(&p1, &p2, &p3, &p4);

				if (try_loop == 0 && fabs(relative_angle) >= 45)  // first try to use restrictive condition
				{
					continue;
				}

				if (try_loop == 1 && fabs(relative_angle) >= 90)  // second try to use restrictive condition
				{
					continue;
				}

				if (local_distance < min_distance)
				{
					min_distance = local_distance;
					matching_macro_node_j = g_link_vector[j].to_node_seq_no;
				}

			}


			if (matching_macro_node_j >= 0)
			{  // create a bridge connector with macro from node and micro to node
				int internal_from_node_seq_no = g_link_vector[i].to_node_seq_no;
				int internal_to_node_seq_no = matching_macro_node_j;
				bool intersect_flag = g_get_line_polygon_intersection(g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y,
					g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y,
					assignment.g_MRM_subarea_shape_points);

				if (intersect_flag == false)  // else give another try
				{
					
					string agent_type_str;
				g_add_new_bridge_link(internal_from_node_seq_no, internal_to_node_seq_no, agent_type_str, 0);

				fprintf(g_pFileModelLink, "%d,%d,", g_node_vector[internal_from_node_seq_no].node_id, g_node_vector[internal_to_node_seq_no].node_id);
				fprintf(g_pFileModelLink, "\"LINESTRING (");
				fprintf(g_pFileModelLink, "%f %f,", g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y);
				fprintf(g_pFileModelLink, "%f %f", g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y);
				fprintf(g_pFileModelLink, ")\"");
				fprintf(g_pFileModelLink, "\n");

				bridge_link_count++;
				try_loop = 100; //exit
				}
			}

		}

	}


	fclose(g_pFileModelLink);

	return bridge_link_count;
}
//
//int g_MRM_subarea_adding_bridge_macro_to_micro(Assignment& assignment)
//{
//
//	int bridge_link_count = 0;
//
//	std::vector<int> incoming_bridge_macro_link_no_vector;
//
//	std::vector<int> incoming_bridge_micro_link_no_vector;
//
//	std::vector<int> outgoing_bridge_macro_link_no_vector;
//
//	std::vector<int> outgoing_bridge_micro_link_no_vector;
//
//	FILE* g_pFileModelLink = fopen("MRM_bridge_link.csv", "w");
//
//	if (g_pFileModelLink != NULL)
//	{
//		fprintf(g_pFileModelLink, "from_node_id,to_node_id,geometry\n");
//
//	}
//	else
//	{
//		dtalog.output() << "Error: file MRM_link.csv cannot be openned." << endl;
//		g_program_stop();
//	}
//
//	for (int i = 0; i < g_link_vector.size(); ++i)
//	{
//
//		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
//		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;
//
//		if (g_link_vector[i].layer_no == 0)  //macro node
//		{
//
//
//			if (from_node_id == 9481)
//			{
//				int debug_code = 1;
//			}
//
//			if (
//				assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 0 &&
//				assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.find(to_node_id) != assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.end() &&
//				assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping[to_node_id] == 3)
//			{
//				incoming_bridge_macro_link_no_vector.push_back(i);
//				g_link_vector[i].link_type = -200; // invisible
//			}
//
//			if (assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 0 &&
//				assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.find(from_node_id) != assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.end() &&
//				assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping[from_node_id] == 3)
//			{
//				outgoing_bridge_macro_link_no_vector.push_back(i);
//				g_link_vector[i].link_type = -200; // invisible
//			}
//		}
//
//		if (g_link_vector[i].layer_no == 1)  //micro node
//		{
//
//			if (assignment.g_micro_node_id_to_MRM_bridge_mapping.find(from_node_id) != assignment.g_micro_node_id_to_MRM_bridge_mapping.end()
//				&&
//				assignment.g_micro_node_id_to_MRM_bridge_mapping[from_node_id] == 2)
//			{
//				incoming_bridge_micro_link_no_vector.push_back(i);
//
//			}
//			if (assignment.g_micro_node_id_to_MRM_bridge_mapping.find(to_node_id) != assignment.g_micro_node_id_to_MRM_bridge_mapping.end()
//				&&
//				assignment.g_micro_node_id_to_MRM_bridge_mapping[to_node_id] == 2)
//			{
//				outgoing_bridge_micro_link_no_vector.push_back(i);
//
//			}
//
//		}
//	}
//
//
//
//	int micro_link_index = 0;
//	for (int microj = 0; microj < incoming_bridge_micro_link_no_vector.size(); ++microj)
//	{
//		int j = incoming_bridge_micro_link_no_vector[microj];
//		if (g_link_vector[j].cell_type == 2)  // lane changing arc
//		{
//			continue;  //skip
//		}
//		int from_node_id = g_node_vector[g_link_vector[j].from_node_seq_no].node_id;
//		int to_node_id = g_node_vector[g_link_vector[j].to_node_seq_no].node_id;
//
//		int k = g_link_vector[j].from_node_seq_no;
//
//		micro_link_index++;
//	}
//
//	for (int microj = 0; microj < outgoing_bridge_micro_link_no_vector.size(); ++microj)
//	{
//		int j = outgoing_bridge_micro_link_no_vector[microj];
//		if (g_link_vector[j].cell_type == 2)  // lane changing arc
//		{
//			continue;  //skip
//		}
//		int from_node_id = g_node_vector[g_link_vector[j].from_node_seq_no].node_id;
//		int to_node_id = g_node_vector[g_link_vector[j].to_node_seq_no].node_id;
//
//		int k = g_link_vector[j].to_node_seq_no;
//		micro_link_index++;
//	}
//
//	int macro_link_index = 0;
//	for (int macroi = 0; macroi < incoming_bridge_macro_link_no_vector.size(); ++macroi)
//	{
//
//		int i = incoming_bridge_macro_link_no_vector[macroi];
//		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
//		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;
//
//		int k = g_link_vector[i].from_node_seq_no;
//		macro_link_index++;
//	}
//
//	for (int macroi = 0; macroi < outgoing_bridge_macro_link_no_vector.size(); ++macroi)
//	{
//
//		int i = outgoing_bridge_macro_link_no_vector[macroi];
//		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
//		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;
//
//		int k = g_link_vector[i].to_node_seq_no;
//		macro_link_index++;
//	}
//
//	// one micro to one macro matching 
//	// from each micro incoming link, find the nearest macro incoming link  and add the connectors from the macro from node id to the micro to node id 
//
//	for (int macroi = 0; macroi < incoming_bridge_macro_link_no_vector.size(); ++macroi)
//	{
//		int i = incoming_bridge_macro_link_no_vector[macroi];
//
//		int from_node_id = g_node_vector[g_link_vector[i].from_node_seq_no].node_id;
//		int to_node_id = g_node_vector[g_link_vector[i].to_node_seq_no].node_id;
//
//		// try 3 times
//		for (int try_loop = 0; try_loop < 3; try_loop++)
//		{
//			int matching_micro_node_j = -1;
//
//				for (int microj = 0; microj < incoming_bridge_micro_link_no_vector.size(); ++microj)
//			{
//				int j = incoming_bridge_micro_link_no_vector[microj];
//				double min_distance = 99999;
//
//				int from_node_id_micro = g_node_vector[g_link_vector[j].from_node_seq_no].node_id;
//
//				if (g_link_vector[j].cell_type == 2)  // lane changing arc
//				{
//					continue;  //skip
//				}
//
//
//				GDPoint p1, p2, p3, p4;
//				p1.x = g_node_vector[g_link_vector[i].from_node_seq_no].x;
//				p1.y = g_node_vector[g_link_vector[i].from_node_seq_no].y;
//
//				p2.x = g_node_vector[g_link_vector[i].to_node_seq_no].x;
//				p2.y = g_node_vector[g_link_vector[i].to_node_seq_no].y;
//
//				p3.x = g_node_vector[g_link_vector[j].from_node_seq_no].x; //micro 
//				p3.y = g_node_vector[g_link_vector[j].from_node_seq_no].y;
//
//				p4.x = g_node_vector[g_link_vector[j].to_node_seq_no].x;
//				p4.y = g_node_vector[g_link_vector[j].to_node_seq_no].y;
//
//				// matching distance from micro cell to the macro link
//				double local_distance2 = (g_GetPoint2Point_Distance(&p3, &p1) + g_GetPoint2Point_Distance(&p4, &p2)) / 2;
//				double local_distance = (g_GetPoint2LineDistance(&p3, &p1, &p2) + g_GetPoint2LineDistance(&p4, &p1, &p2)) / 2 + local_distance2;
//
//				double relative_angle = g_Find_PPP_RelativeAngle(&p1, &p2, &p3, &p4);
//
//				if (try_loop == 0 && fabs(relative_angle) >= 45)  // first try to use restrictive condition
//				{
//					continue;
//				}
//
//				if (try_loop == 1 && fabs(relative_angle) >= 90)  // second try to use restrictive condition
//				{
//					continue;
//				}
//
//				if (local_distance < min_distance)
//				{
//					min_distance = local_distance;
//					matching_micro_node_j = g_link_vector[i].from_node_seq_no;
//				}
//
//			}
//
//
//			if (matching_micro_node_j >= 0)
//			{  // create a bridge connector with macro from node and micro to node
//				int internal_from_node_seq_no = g_link_vector[i].from_node_seq_no;
//				int internal_to_node_seq_no = matching_micro_node_j;
//
//
//				//bool g_get_line_polygon_intersection(
//				//	double Ax, double Ay,
//				//	double Bx, double By,
//				//	std::vector<GDPoint> subarea_shape_points)
//
//				bool intersect_flag = g_get_line_polygon_intersection(g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y,
//					g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y,
//					assignment.g_MRM_subarea_shape_points);
//
//				if (intersect_flag == false)  // else give another try
//				{
//
//					string agent_type_str;
//					g_add_new_bridge_link(internal_from_node_seq_no, internal_to_node_seq_no, agent_type_str, 0);
//
//					fprintf(g_pFileModelLink, "%d,%d,", g_node_vector[internal_from_node_seq_no].node_id, g_node_vector[internal_to_node_seq_no].node_id);
//					fprintf(g_pFileModelLink, "\"LINESTRING (");
//					fprintf(g_pFileModelLink, "%f %f,", g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y);
//					fprintf(g_pFileModelLink, "%f %f", g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y);
//					fprintf(g_pFileModelLink, ")\"");
//					fprintf(g_pFileModelLink, "\n");
//
//
//					bridge_link_count++;
//					try_loop = 100; //exit
//
//				}
//			}
//
//		}
//
//	}
//
//	// one micro to one macro matching 
//	// from each micro incoming link, find the nearest macro incoming link  and add the connectors from the macro from node id to the micro to node id 
//
//	//for (int macroj = 0; macroj < outgoing_bridge_macro_link_no_vector.size(); ++macroj)
//	//{
//	//	int j = outgoing_bridge_macro_link_no_vector[macroj];
//	//
//	//	for (int try_loop = 0; try_loop < 3; try_loop++)
//	//	{
//	//		int matching_micro_node_i = -1;
//	//		for (int microi = 0; microi < outgoing_bridge_micro_link_no_vector.size(); ++microi)
//	//		{
//	//			int i = outgoing_bridge_micro_link_no_vector[microi];
//	//			double min_distance = 99999;
//
//	//			if (g_link_vector[i].cell_type == 2)  // lane chaning arc
//	//			{
//	//				continue;  //skip
//	//			}
//
//	//			GDPoint p1, p2, p3, p4;
//	//			p1.x = g_node_vector[g_link_vector[j].from_node_seq_no].x;  // macro
//	//			p1.y = g_node_vector[g_link_vector[j].from_node_seq_no].y;
//
//	//			p2.x = g_node_vector[g_link_vector[j].to_node_seq_no].x;
//	//			p2.y = g_node_vector[g_link_vector[j].to_node_seq_no].y;
//
//	//			p3.x = g_node_vector[g_link_vector[i].from_node_seq_no].x;  // micro
//	//			p3.y = g_node_vector[g_link_vector[i].from_node_seq_no].y;
//
//	//			p4.x = g_node_vector[g_link_vector[i].to_node_seq_no].x;
//	//			p4.y = g_node_vector[g_link_vector[i].to_node_seq_no].y;
//
//	//			// matching distance from micro cell to the macro link
//	//			double local_distance2 = (g_GetPoint2Point_Distance(&p3, &p1) + g_GetPoint2Point_Distance(&p4, &p2)) / 2;
//	//			double local_distance = (g_GetPoint2LineDistance(&p3, &p1, &p2) + g_GetPoint2LineDistance(&p4, &p1, &p2)) / 2 + local_distance2;
//
//	//			double relative_angle = g_Find_PPP_RelativeAngle(&p1, &p2, &p3, &p4);
//
//	//			if (try_loop == 0 && fabs(relative_angle) >= 45)  // first try to use restrictive condition
//	//			{
//	//				continue;
//	//			}
//
//	//			if (try_loop == 1 && fabs(relative_angle) >= 90)  // second try to use restrictive condition
//	//			{
//	//				continue;
//	//			}
//
//	//			if (local_distance < min_distance)
//	//			{
//	//				min_distance = local_distance;
//	//				matching_micro_node_i = g_link_vector[j].to_node_seq_no;
//	//			}
//
//	//		}
//
//
//	//		if (matching_micro_node_i >= 0)
//	//		{  // create a bridge connector with macro from node and micro to node
//	//			int internal_from_node_seq_no = matching_micro_node_i;
//	//			int internal_to_node_seq_no = g_link_vector[j].to_node_seq_no;
//	//			bool intersect_flag = g_get_line_polygon_intersection(g_node_vector[internal_from_node_seq_no].x, g_node_vector[internal_from_node_seq_no].y,
//	//				g_node_vector[internal_to_node_seq_no].x, g_node_vector[internal_to_node_seq_no].y,
//	//				assignment.g_MRM_subarea_shape_points);
//
//	//			if (intersect_flag == false)  // else give another try
//	//			{
//
//	//				string agent_type_str;
//	//				g_add_new_bridge_link(internal_from_node_seq_no, internal_to_node_seq_no, agent_type_str, 0);
//
//
//	//				bridge_link_count++;
//	//				try_loop = 100; //exit
//	//			}
//	//		}
//
//	//	}
//
//	//}
//
//
//	fclose(g_pFileModelLink);
//	return bridge_link_count;
//}
//

double g_CheckActivityNodes(Assignment& assignment)
{

	int activity_node_count = 0;
	for (int i = 0; i < g_node_vector.size(); i++)
	{

		if (g_node_vector[i].is_activity_node >= 1)
		{
			activity_node_count++;
		}
	}


	if (activity_node_count <= 1)
	{
		activity_node_count = 0;
		int sampling_rate = 10;

		for (int i = 0; i < g_node_vector.size(); i++)
		{

			if (i % sampling_rate == 0)
			{
				g_node_vector[i].is_activity_node = 10;//random generation
				activity_node_count++;
			}
		}

		//if (activity_node_count <= 1)
		//{
		//    activity_node_count = 0;
		//    sampling_rate = 2;

		//    for (int i = 0; i < g_node_vector.size(); i++)
		//    {

		//        if (i % sampling_rate == 0)
		//        {
		//            g_node_vector[i].is_activity_node = 10;//random generation
		//            activity_node_count++;
		//        }
		//    }
		//     still no activity nodes, define all nodes as activity nodes
		//    if (activity_node_count <= 1)
		//    {
		//        activity_node_count = 0;

		//        for (int i = 0; i < g_node_vector.size(); i++)
		//        {

		//            g_node_vector[i].is_activity_node = 10;//random generation
		//            activity_node_count++;
		//        }
		//    }
		//}


	}


	// calculate avg near by distance; 
	double total_near_by_distance = 0;
	activity_node_count = 0;
	for (int i = 0; i < g_node_vector.size(); i++)
	{
		double min_near_by_distance = 100;
		if (g_node_vector[i].is_activity_node)
		{
			activity_node_count++;
			for (int j = 0; j < g_node_vector.size(); j++)
			{
				if (i != j && g_node_vector[j].is_activity_node)
				{



					double near_by_distance = g_calculate_p2p_distance_in_meter_from_latitude_longitude(g_node_vector[i].x, g_node_vector[i].y, g_node_vector[j].x, g_node_vector[j].y);

					if (near_by_distance < min_near_by_distance)
						min_near_by_distance = near_by_distance;

				}

			}

			total_near_by_distance += min_near_by_distance;
			activity_node_count++;
		}
	}

	double nearby_distance = total_near_by_distance / max(1, activity_node_count);
	return nearby_distance;

}

int g_detect_if_zones_defined_in_node_csv(Assignment& assignment)
{
	CCSVParser parser;

	int number_of_zones = 0;
	int number_of_is_boundary = 0;


	if (parser.OpenCSVFile("node.csv", true))
	{
		while (parser.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
		{
			int node_id;
			if (!parser.GetValueByFieldName("node_id", node_id))
				continue;

			int zone_id = 0;
			int is_boundary = 0;
			parser.GetValueByFieldName("zone_id", zone_id);
			parser.GetValueByFieldName("is_boundary", is_boundary, false);

			if (zone_id >= 1)
			{
				number_of_zones++;
			}

			if (is_boundary != 0)
			{
				number_of_is_boundary++;
			}
		}

		parser.CloseCSVFile();

		if (number_of_zones >= 2)  // if node.csv or zone.csv have 2 more zones;
		{
			assignment.summary_file << ", number of zones defined in node.csv=, " << number_of_zones << endl;
			assignment.summary_file << ", number of boundary nodes defined in zone.csv=, " << number_of_zones << endl;
			return number_of_zones;
		}

	}

	if (parser.OpenCSVFile("zone.csv", true))
	{
		while (parser.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
		{
			int node_id;
			if (!parser.GetValueByFieldName("node_id", node_id))
				continue;
			int zone_id = 0;
			parser.GetValueByFieldName("zone_id", zone_id);
			if (zone_id >= 1)
			{
				number_of_zones++;
			}
		}
		parser.CloseCSVFile();
		if (number_of_zones >= 2)  // if node.csv or zone.csv have 2 more zones;
		{
			assignment.summary_file << ", number of zones defined in zone.csv=, " << number_of_zones << endl;
			return number_of_zones;
		}
	}

		return 0;
}


void g_detector_file_open_status()
{
	FILE* g_pFilePathMOE = nullptr;


	fopen_ss(&g_pFilePathMOE, "final_summary.csv", "w");
	if (!g_pFilePathMOE)
	{
		dtalog.output() << "File final_summary.csv cannot be opened." << endl;
		g_program_stop();
	}
	else
	{
		fclose(g_pFilePathMOE);
	}



}
void g_read_input_data(Assignment& assignment)
{
	g_detector_file_open_status();

	assignment.g_LoadingStartTimeInMin = 99999;
	assignment.g_LoadingEndTimeInMin = 0;

	//step 0:read demand period file
	CCSVParser parser_demand_period;
	dtalog.output() << "_____________" << endl;
	dtalog.output() << "Step 1: Reading input data" << endl;
	dtalog.output() << "_____________" << endl;

	dtalog.output() << "Step 1.1: Reading section [demand_period] in setting.csv..." << endl;

	parser_demand_period.IsFirstLineHeader = false;

	//step 1:read demand type file

	assignment.g_number_of_nodes = 0;
	assignment.g_number_of_links = 0;  // initialize  the counter to 0


	int number_of_zones = g_detect_if_zones_defined_in_node_csv(assignment);
	// = 1: normal
	//= 0, there are is boundary 
	//=-1, no information



	int internal_node_seq_no = 0;
	// step 3: read node file


	std::map<int, int> zone_id_mapping;  // this is used to mark if this zone_id has been identified or not
	std::map<int, double> zone_id_x;
	std::map<int, double> zone_id_y;

	std::map<int, float> zone_id_production;
	std::map<int, float> zone_id_attraction;

	CCSVParser parser;

	int multmodal_activity_node_count = 0;

	dtalog.output() << "Step 1.3: Reading zone data in zone.csv..." << endl;




	dtalog.output() << "Step 1.4: Reading node data in node.csv..." << endl;
	std::map<int, int> zone_id_to_analysis_district_id_mapping;

	// MRM: stage 1: filtering, output: gate nodes, bridge nodes for micro and macro networks
	int number_of_micro_gate_nodes = g_MRM_subarea_filtering(assignment);

	// MRM: stage 2: read node layer
	int max_layer_flag = 0;

	// a MRM subarea file is defined. # of micro gate nodes is positve, apply MRM mode with 2 layers
	if (assignment.g_MRM_subarea_shape_points.size() >= 3 && number_of_micro_gate_nodes > 0)
		{
			max_layer_flag = 1;
		}
		

	assignment.summary_file << ", # of node layers to be read," << max_layer_flag+1 << "," << endl;

	for(int layer_no = 0; layer_no <= max_layer_flag; layer_no ++)
	{
		dtalog.output() << "Reading node data at layer No." << layer_no << endl;

		// master file: reading nodes

		string file_name = "node.csv";

		if (layer_no == 1)
		{
			file_name = "hdnet\\node.csv";
		}

		if (parser.OpenCSVFile(file_name.c_str(), true))
		{
			// create a node object
			CNode node;

			while (parser.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
			{
				int node_id;
				if (!parser.GetValueByFieldName("node_id", node_id))
					continue;


				if (node_id == 9439)
				{
					int trace_flag = 1;
				}


				if(max_layer_flag == 1) // MRM mode
				{
					if (layer_no == 0)  // macro node layer
					{  // skip all inside non-gate node

							if (assignment.g_node_id_to_MRM_subarea_mapping.find(node_id) != assignment.g_node_id_to_MRM_subarea_mapping.end())
							{
								if (assignment.g_node_id_to_MRM_subarea_mapping[node_id] == 1  
									 // inside and not defined as gate node
									&&
										assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.find(node_id) == assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.end()
									&&
									assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.find(node_id) == assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.end()

										
										)
								{
									continue;

								}

							}



					}

					if (layer_no == 1)  // micro node layer
					{
						node_id += MICRONET_NODE_ID_BIG_M;

						if (assignment.g_node_id_to_MRM_subarea_mapping.find(node_id) != assignment.g_node_id_to_MRM_subarea_mapping.end())
						{
							// skip all outside non-gate node
							if (assignment.g_node_id_to_MRM_subarea_mapping[node_id] == 0 &&
								assignment.g_micro_node_id_to_MRM_bridge_mapping.find(node_id) == assignment.g_micro_node_id_to_MRM_bridge_mapping.end()
								)  // outside
							{
								continue;

							}

						}
					}

				}

				if (assignment.g_node_id_to_seq_no_map.find(node_id) != assignment.g_node_id_to_seq_no_map.end())
				{
					//has been defined
					continue;
				}

				


				double x_coord;
				double y_coord;

				parser.GetValueByFieldName("x_coord", x_coord, true, false);
				parser.GetValueByFieldName("y_coord", y_coord, true, false);


				// micro network filter:
				GDPoint pt;
				pt.x = x_coord;
				pt.y = y_coord;

				assignment.g_node_id_to_seq_no_map[node_id] = internal_node_seq_no;

				node.node_id = node_id;
				node.node_seq_no = internal_node_seq_no;
				node.layer_no = layer_no;
				node.x = x_coord;
				node.y = y_coord;

				if (assignment.g_micro_node_id_to_MRM_bridge_mapping.find(node_id) != assignment.g_micro_node_id_to_MRM_bridge_mapping.end())
					node.MRM_gate_flag = assignment.g_micro_node_id_to_MRM_bridge_mapping[node_id];

				if (assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.find(node_id) != assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping.end())
					node.MRM_gate_flag = assignment.g_macro_node_id_to_MRM_incoming_bridge_mapping[node_id];

				if (assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.find(node_id) != assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping.end())
					node.MRM_gate_flag = assignment.g_macro_node_id_to_MRM_outgoing_bridge_mapping[node_id];


				int zone_id = -1;

				parser.GetValueByFieldName("is_boundary", node.is_boundary, false, false);
				parser.GetValueByFieldName("node_type", node.node_type, false);// step 1 for adding access links: read node type
				parser.GetValueByFieldName("zone_id", zone_id);


				int analysis_district_id = 0;  // default 

				parser.GetValueByFieldName("district_id", analysis_district_id, false);
			
				if (analysis_district_id >= MAX_ORIGIN_DISTRICTS)
				{
					dtalog.output() << "Error: grid_id in node.csv is larger than predefined value of 20." << endl;
					g_program_stop();
				}

				if (node_id == 2235)
				{
					int idebug = 1;
				}

				if (layer_no == 1 && node.is_boundary!=0)  // micro node layer and is boundary node
				{
					double distance_to_zone = 9999999;

					std::map<int, double>::iterator it_zone;

					for (it_zone = assignment.subareaMRM_zone_id_X_mapping.begin(); it_zone != assignment.subareaMRM_zone_id_X_mapping.end(); ++it_zone)
					{
						double local_distance = pow(pow(it_zone->second - x_coord, 2) + pow(assignment.subareaMRM_zone_id_Y_mapping[it_zone->first] - y_coord, 2), 0.5);

						if (local_distance < distance_to_zone)
						{
							zone_id = it_zone->first;
							distance_to_zone = local_distance;  //bubble sort
						}


					}

				}

				if (zone_id >= 1)
				{
					zone_id_to_analysis_district_id_mapping[zone_id] = analysis_district_id;

					node.zone_org_id = zone_id;  // this note here, we use zone_org_id to ensure we will only have super centriods with zone id positive. 
					if (zone_id >= 1)
						node.is_activity_node = 1;  // from zone

					string str_agent_type;
					parser.GetValueByFieldName("agent_type", str_agent_type, false); //step 2 for adding access links: read agent_type for adding access links

					if (str_agent_type.size() > 0 && assignment.agent_type_2_seqno_mapping.find(str_agent_type) != assignment.agent_type_2_seqno_mapping.end())
					{
						node.agent_type_str = str_agent_type;
						node.agent_type_no = assignment.agent_type_2_seqno_mapping[str_agent_type];
						multmodal_activity_node_count++;
					}
				}


				// this is an activity node // we do not allow zone id of zero
				if (zone_id >= 1)
				{
					// for physcial nodes because only centriod can have valid zone_id.
					node.zone_org_id = zone_id;
					if (zone_id_mapping.find(zone_id) == zone_id_mapping.end())
					{
						//create zone
						zone_id_mapping[zone_id] = node_id;

						assignment.zone_id_X_mapping[zone_id] = node.x;
						assignment.zone_id_Y_mapping[zone_id] = node.y;
					}


				}
				if (zone_id >= 1)
				{

					assignment.node_seq_no_2_info_zone_id_mapping[internal_node_seq_no] = zone_id;
				}

				/*node.x = x;
				node.y = y;*/
				internal_node_seq_no++;

				// push it to the global node vector
				g_node_vector.push_back(node);
				assignment.g_number_of_nodes++;

				if (assignment.g_number_of_nodes % 5000 == 0)
					dtalog.output() << "reading " << assignment.g_number_of_nodes << " nodes.. " << endl;
			}

			dtalog.output() << "number of nodes = " << assignment.g_number_of_nodes << endl;
			dtalog.output() << "number of multimodal activity nodes = " << multmodal_activity_node_count << endl;
			dtalog.output() << "number of zones = " << zone_id_mapping.size() << endl;

			// fprintf(g_pFileOutputLog, "number of nodes =,%d\n", assignment.g_number_of_nodes);
			parser.CloseCSVFile();
		}
	}




	// MRM: stage 3: read link layers
	// step 4: read link file

	CCSVParser parser_link;

	int link_type_warning_count = 0;
	bool length_in_km_waring = false;
	dtalog.output() << "Step 1.6: Reading link data in link.csv... " << endl;

	for (int layer_no = 0; layer_no <= max_layer_flag; layer_no++)
	{
		dtalog.output() << "link layer= " << layer_no << endl;


		string file_name = "link.csv";

		if (layer_no == 1)
		{
			file_name = "hdnet\\link.csv";
		}

		if (parser_link.OpenCSVFile(file_name.c_str(), true))
		{
			// create a link object
			CLink link;

			while (parser_link.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
			{
				string link_type_name_str;
				parser_link.GetValueByFieldName("link_type_name", link_type_name_str, false);

				long from_node_id= -1;
				if (!parser_link.GetValueByFieldName("from_node_id", from_node_id))
					continue;

				long to_node_id =-1;
				if (!parser_link.GetValueByFieldName("to_node_id", to_node_id))
					continue;

				string linkID;
				parser_link.GetValueByFieldName("link_id", linkID,false);
				// add the to node id into the outbound (adjacent) node list

				if (from_node_id == 9439)
				{
					int trace_id = 1;
				}


				if (max_layer_flag == 1) // MRM mode
				{
					if (layer_no == 0)  // macro link layer
					{  // skip all links inside non-gate node

						if (assignment.g_node_id_to_MRM_subarea_mapping.find(from_node_id) != assignment.g_node_id_to_MRM_subarea_mapping.end()
							|| assignment.g_node_id_to_MRM_subarea_mapping.find(to_node_id) != assignment.g_node_id_to_MRM_subarea_mapping.end())
						{
							if (assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 1
						&&		assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 1)  // both upstream and downstream inside
							{
								continue;

							}
							// keep all outside link or bridge link with a gate node inside the suabrea MRM

						}

					}

					if (layer_no == 1)  // micro link layer
					{
						from_node_id += MICRONET_NODE_ID_BIG_M;
						to_node_id += MICRONET_NODE_ID_BIG_M;

						if (assignment.g_node_id_to_MRM_subarea_mapping.find(from_node_id) != assignment.g_node_id_to_MRM_subarea_mapping.end()
							&& assignment.g_node_id_to_MRM_subarea_mapping.find(to_node_id) != assignment.g_node_id_to_MRM_subarea_mapping.end())
						{
							if (assignment.g_node_id_to_MRM_subarea_mapping[from_node_id] == 0 && assignment.g_node_id_to_MRM_subarea_mapping[to_node_id] == 0)  // both upstream and downstream outside
							{
								continue;

							}
							// keep all inside link or bridge link with a gate node inside the suabrea MRM

						}
					}

				}

				if (assignment.g_node_id_to_seq_no_map.find(from_node_id) == assignment.g_node_id_to_seq_no_map.end())
				{

					int MRM_subarea_mapping_flag = assignment.g_node_id_to_MRM_subarea_mapping[from_node_id];
					int MRM_subarea_mapping_flag_to = assignment.g_node_id_to_MRM_subarea_mapping[to_node_id];

					dtalog.output() << "Error: from_node_id " << from_node_id << " in file link.csv is not defined in node.csv." << endl;
					dtalog.output() << " from_node_id mapping =" << MRM_subarea_mapping_flag << endl;
					continue; //has not been defined
				}

				if (assignment.g_node_id_to_seq_no_map.find(to_node_id) == assignment.g_node_id_to_seq_no_map.end())
				{

					int MRM_subarea_mapping_flag = assignment.g_node_id_to_MRM_subarea_mapping[from_node_id];
					int MRM_subarea_mapping_flag_to = assignment.g_node_id_to_MRM_subarea_mapping[to_node_id];

					dtalog.output() << "Error: to_node_id " << to_node_id << " in file link.csv is not defined in node.csv." << endl;
					continue; //has not been defined
				}

				//if (assignment.g_link_id_map.find(linkID) != assignment.g_link_id_map.end())
				//    dtalog.output() << "Error: link_id " << linkID.c_str() << " has been defined more than once. Please check link.csv." << endl;

				int internal_from_node_seq_no = assignment.g_node_id_to_seq_no_map[from_node_id];  // map external node number to internal node seq no.
				int internal_to_node_seq_no = assignment.g_node_id_to_seq_no_map[to_node_id];



				link.from_node_seq_no = internal_from_node_seq_no;
				link.to_node_seq_no = internal_to_node_seq_no;
				link.link_seq_no = assignment.g_number_of_links;
				link.to_node_seq_no = internal_to_node_seq_no;
				link.layer_no = layer_no;
				link.link_id = linkID;
				link.subarea_id = -1;

				if (g_node_vector[link.from_node_seq_no].subarea_id >= 1 || g_node_vector[link.to_node_seq_no].subarea_id >= 1)
				{
					link.subarea_id = g_node_vector[link.from_node_seq_no].subarea_id;
				}

				assignment.g_link_id_map[link.link_id] = 1;

				string movement_str;
				parser_link.GetValueByFieldName("mvmt_txt_id", movement_str, false);
				int cell_type = -1;
				if (parser_link.GetValueByFieldName("cell_type", cell_type, false) == true)
					link.cell_type = cell_type;

				int meso_link_id=-1;
				parser_link.GetValueByFieldName("meso_link_id", meso_link_id, false);

				if(meso_link_id>=0)
					link.meso_link_id = meso_link_id;

				parser_link.GetValueByFieldName("geometry", link.geometry, false);
				parser_link.GetValueByFieldName("link_code", link.link_code_str, false);
				parser_link.GetValueByFieldName("tmc_corridor_name", link.tmc_corridor_name, false);
				parser_link.GetValueByFieldName("link_type_name", link.link_type_name, false);

				parser_link.GetValueByFieldName("link_type_code", link.link_type_code, false);

				// and valid
				if (movement_str.size() > 0)
				{
					int main_node_id = -1;


					link.mvmt_txt_id = movement_str;
					link.main_node_id = main_node_id;
				}

				// Peiheng, 05/13/21, if setting.csv does not have corresponding link type or the whole section is missing, set it as 2 (i.e., Major arterial)
				int link_type = 2;
				parser_link.GetValueByFieldName("link_type", link_type, false);

				double length_in_meter = 1.0; // km or mile
				double free_speed = 60.0;
				double cutoff_speed = 1.0;
				double k_jam = assignment.g_LinkTypeMap[link.link_type].k_jam;
				double bwtt_speed = 12;  //miles per hour

				double lane_capacity = 1800;
				parser_link.GetValueByFieldName("length", length_in_meter);  // in meter
				parser_link.GetValueByFieldName("FT", link.FT, false, true);
				parser_link.GetValueByFieldName("AT", link.AT, false, true);
				parser_link.GetValueByFieldName("vdf_code", link.vdf_code, false);

				if (length_in_km_waring == false && length_in_meter < 0.1)
				{
					dtalog.output() << "warning: link link_distance_VDF =" << length_in_meter << " in link.csv for link " << from_node_id << "->" << to_node_id << ". Please ensure the unit of the link link_distance_VDF is meter." << endl;
					// link.link_type has been taken care by its default constructor
					length_in_km_waring = true;
				}

				if (length_in_meter < 1)
				{
					length_in_meter = 1;  // minimum link_distance_VDF
				}
				parser_link.GetValueByFieldName("free_speed", free_speed);

				if (link.link_id == "201065AB")
				{
					int idebug = 1;
				}

				cutoff_speed = free_speed * 0.85; //default; 
				parser_link.GetValueByFieldName("cutoff_speed", cutoff_speed, false);


				if (free_speed <= 0.1)
					free_speed = 60;

				free_speed = max(0.1, free_speed);

				link.free_speed = free_speed;
				link.v_congestion_cutoff = cutoff_speed;



				double number_of_lanes = 1;
				parser_link.GetValueByFieldName("lanes", number_of_lanes);
				parser_link.GetValueByFieldName("capacity", lane_capacity);
				parser_link.GetValueByFieldName("saturation_flow_rate", link.saturation_flow_rate,false);

			
				link.free_flow_travel_time_in_min = length_in_meter / 1609.0 / free_speed * 60;  // link_distance_VDF in meter 
				float fftt_in_sec = link.free_flow_travel_time_in_min * 60;  // link_distance_VDF in meter 

				link.length_in_meter = length_in_meter;
				link.link_distance_VDF = length_in_meter / 1609.0;
				link.link_distance_km = length_in_meter / 1000.0;
				link.link_distance_mile = length_in_meter / 1609.0;

				link.traffic_flow_code = assignment.g_LinkTypeMap[link.link_type].traffic_flow_code;
				link.number_of_lanes = number_of_lanes;
				link.lane_capacity = lane_capacity;

				//spatial queue and kinematic wave
				link.spatial_capacity_in_vehicles = max(1.0, link.link_distance_VDF * number_of_lanes * k_jam);

				// kinematic wave
				if (link.traffic_flow_code == 3)
					link.BWTT_in_simulation_interval = link.link_distance_VDF / bwtt_speed * 3600 / number_of_seconds_per_interval;

				link.vdf_type = assignment.g_LinkTypeMap[link.link_type].vdf_type;
				link.kjam = assignment.g_LinkTypeMap[link.link_type].k_jam;
				char VDF_field_name[50];

				for (int at = 0; at < assignment.g_AgentTypeVector.size(); at++)
				{
					double pce_at = -1; // default
					sprintf(VDF_field_name, "VDF_pce%s", assignment.g_AgentTypeVector[at].agent_type.c_str());

					parser_link.GetValueByFieldName(VDF_field_name, pce_at, false, true);

					if (pce_at > 1.001)  // log
					{
						//dtalog.output() << "link " << from_node_id << "->" << to_node_id << " has a pce of " << pce_at << " for agent type "
						//    << assignment.g_AgentTypeVector[at].agent_type.c_str() << endl;
					}


					for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
					{
						if (pce_at > 0)
						{   // if there are link based PCE values
							link.VDF_period[tau].pce[at] = pce_at;
						}
						else
						{
							link.VDF_period[tau].pce[at] = assignment.g_AgentTypeVector[at].PCE;

						}
						link.VDF_period[tau].occ[at] = assignment.g_AgentTypeVector[at].OCC;  //occ;
					}

				}

				// reading for VDF related functions 
				// step 1 read type


					//data initialization 

				for (int time_index = 0; time_index < MAX_TIMEINTERVAL_PerDay; time_index++)
				{
					link.model_speed[time_index] = free_speed;
					link.est_volume_per_hour_per_lane[time_index] = 0;
					link.est_avg_waiting_time_in_min[time_index] = 0;
					link.est_queue_length_per_lane[time_index] = 0;
				}


				for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
				{
					//setup default values
					link.VDF_period[tau].vdf_type = assignment.g_LinkTypeMap[link.link_type].vdf_type;
					link.VDF_period[tau].lane_based_ultimate_hourly_capacity = lane_capacity;
					link.VDF_period[tau].nlanes = number_of_lanes;

					link.VDF_period[tau].FFTT = link.link_distance_VDF / max(0.0001, link.free_speed) * 60.0;  // 60.0 for 60 min per hour
					link.VDF_period[tau].BPR_period_capacity = link.lane_capacity * link.number_of_lanes;
					link.VDF_period[tau].vf = link.free_speed;
					link.VDF_period[tau].v_congestion_cutoff = link.v_congestion_cutoff;
					link.VDF_period[tau].alpha = 0.15;
					link.VDF_period[tau].beta = 4;
					link.VDF_period[tau].preload = 0;

					for (int at = 0; at < assignment.g_AgentTypeVector.size(); at++)
					{
						link.VDF_period[tau].toll[at] = 0;
						link.VDF_period[tau].LR_price[at] = 0;
						link.VDF_period[tau].LR_RT_price[at] = 0;
					}

					link.VDF_period[tau].starting_time_in_hour = assignment.g_DemandPeriodVector[tau].starting_time_slot_no * MIN_PER_TIMESLOT / 60.0;
					link.VDF_period[tau].ending_time_in_hour = assignment.g_DemandPeriodVector[tau].ending_time_slot_no * MIN_PER_TIMESLOT / 60.0;
					link.VDF_period[tau].L = assignment.g_DemandPeriodVector[tau].time_period_in_hour;
					link.VDF_period[tau].t2 = assignment.g_DemandPeriodVector[tau].t2_peak_in_hour;
					link.VDF_period[tau].peak_load_factor = 1;

					int demand_period_id = assignment.g_DemandPeriodVector[tau].demand_period_id;
					sprintf(VDF_field_name, "VDF_fftt%d", demand_period_id);

					double FFTT = -1;
					parser_link.GetValueByFieldName(VDF_field_name, FFTT, false, false);  // FFTT should be per min

					bool VDF_required_field_flag = false;
					if (FFTT >= 0)
					{
						link.VDF_period[tau].FFTT = FFTT;
						VDF_required_field_flag = true;
					}

					if (link.VDF_period[tau].FFTT > 100)

					{
						dtalog.output() << "link " << from_node_id << "->" << to_node_id << " has a FFTT of " << link.VDF_period[tau].FFTT << " min at demand period " << demand_period_id
							<< " " << assignment.g_DemandPeriodVector[tau].demand_period.c_str() << endl;
					}

					sprintf(VDF_field_name, "VDF_cap%d", demand_period_id);
					parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].BPR_period_capacity, VDF_required_field_flag, false);
					sprintf(VDF_field_name, "VDF_alpha%d", demand_period_id);
					parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].alpha, VDF_required_field_flag, false);
					sprintf(VDF_field_name, "VDF_beta%d", demand_period_id);
					parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].beta, VDF_required_field_flag, false);

					sprintf(VDF_field_name, "VDF_allowed_uses%d", demand_period_id);
					if (parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].allowed_uses, false) == false)
					{
						parser_link.GetValueByFieldName("allowed_uses", link.VDF_period[tau].allowed_uses, false);
					}

					sprintf(VDF_field_name, "VDF_preload%d", demand_period_id);
					parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].preload, false, false);

					for (int at = 0; at < assignment.g_AgentTypeVector.size(); at++)
					{

						sprintf(VDF_field_name, "VDF_toll%s%d", assignment.g_AgentTypeVector[at].agent_type.c_str(), demand_period_id);
						parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].toll[at], false, false);


					}
					sprintf(VDF_field_name, "VDF_penalty%d", demand_period_id);
					parser_link.GetValueByFieldName(VDF_field_name, link.VDF_period[tau].penalty, false, false);

					if (link.cell_type >= 2) // micro lane-changing arc
					{
						// additinonal min: 1 second 1/60.0 min
						link.VDF_period[tau].penalty += 1.0 / 60.0;
					}

					parser_link.GetValueByFieldName("cycle_length", link.VDF_period[tau].cycle_length, false, false);

					if (link.VDF_period[tau].cycle_length >= 1)
					{
						link.timing_arc_flag = true;

						parser_link.GetValueByFieldName("start_green_time", link.VDF_period[tau].start_green_time);
						parser_link.GetValueByFieldName("end_green_time", link.VDF_period[tau].end_green_time);

						link.VDF_period[tau].effective_green_time = link.VDF_period[tau].end_green_time - link.VDF_period[tau].start_green_time;

						if (link.VDF_period[tau].effective_green_time < 0)
							link.VDF_period[tau].effective_green_time = link.VDF_period[tau].cycle_length;

						link.VDF_period[tau].red_time = max(1, link.VDF_period[tau].cycle_length - link.VDF_period[tau].effective_green_time);
						parser_link.GetValueByFieldName("red_time", link.VDF_period[tau].red_time, false);
						parser_link.GetValueByFieldName("green_time", link.VDF_period[tau].effective_green_time, false);

						if (link.saturation_flow_rate > 1000)  // protect the data attributes to be reasonable 
						{
							link.VDF_period[tau].saturation_flow_rate = link.saturation_flow_rate;
						}
					}

				}

				// for each period

				float default_cap = 1000;
				float default_BaseTT = 1;


				//link.m_OutflowNumLanes = number_of_lanes;//visum lane_cap is actually link_cap

				link.update_kc(free_speed);
				link.link_spatial_capacity = k_jam * number_of_lanes * link.link_distance_VDF;

				link.link_distance_VDF = max(0.00001, link.link_distance_VDF);
				for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
					link.travel_time_per_period[tau] = link.link_distance_VDF / free_speed * 60;

				// min // calculate link cost based link_distance_VDF and speed limit // later we should also read link_capacity, calculate delay

				//int sequential_copying = 0;
				//
				//parser_link.GetValueByFieldName("sequential_copying", sequential_copying);

				g_node_vector[internal_from_node_seq_no].m_outgoing_link_seq_no_vector.push_back(link.link_seq_no);  // add this link to the corresponding node as part of outgoing node/link
				g_node_vector[internal_to_node_seq_no].m_incoming_link_seq_no_vector.push_back(link.link_seq_no);  // add this link to the corresponding node as part of outgoing node/link

				g_node_vector[internal_from_node_seq_no].m_to_node_seq_no_vector.push_back(link.to_node_seq_no);  // add this link to the corresponding node as part of outgoing node/link
				g_node_vector[internal_from_node_seq_no].m_to_node_2_link_seq_no_map[link.to_node_seq_no] = link.link_seq_no;  // add this link to the corresponding node as part of outgoing node/link


				//// TMC reading 
				string tmc_code;

				parser_link.GetValueByFieldName("tmc", link.tmc_code, false);


				if (link.tmc_code.size() > 0)
				{
					parser_link.GetValueByFieldName("tmc_corridor_name", link.tmc_corridor_name, false);
					link.tmc_corridor_id = 1;
					link.tmc_road_sequence = 1;
					parser_link.GetValueByFieldName("tmc_corridor_id", link.tmc_corridor_id, false);
					parser_link.GetValueByFieldName("tmc_road_sequence", link.tmc_road_sequence, false);
				}

				g_link_vector.push_back(link);

				assignment.g_number_of_links++;

				if (assignment.g_number_of_links % 10000 == 0)
					dtalog.output() << "reading " << assignment.g_number_of_links << " links.. " << endl;
			}

			parser_link.CloseCSVFile();

			dtalog.output() << "number of links =" << g_link_vector.size() << endl;
		}

	 }

	 // MRM: stage 4: add bridge links
	 int bridge_link_count = g_MRM_subarea_adding_bridge(assignment);

	// we now know the number of links
	dtalog.output() << "number of bridge links = " << bridge_link_count << endl;


	dtalog.output() << "number of links =" << assignment.g_number_of_links << endl;
	
	assignment.summary_file << "Step 1: read network node.csv, link.csv, zone.csv "<< endl;
	assignment.summary_file << ",# of nodes = ," << g_node_vector.size() << endl;
	assignment.summary_file << ",# of links =," << g_link_vector.size() << endl;
	assignment.summary_file << ",# of zones =," << g_zone_vector.size() << endl;
	assignment.summary_file << ",# of bridge links =," << bridge_link_count << endl;
	assignment.summary_file << ",# of micro gate nodes =," << number_of_micro_gate_nodes << endl;


	assignment.summary_file << ",summary by multi-modal and demand types,demand_period,agent_type,# of links,avg_free_speed,total_length_in_km,total_capacity,avg_lane_capacity,avg_length_in_meter," << endl;
	for (int tau = 0; tau < assignment.g_DemandPeriodVector.size(); ++tau)
		for (int at = 0; at < assignment.g_AgentTypeVector.size(); at++)
		{
			assignment.summary_file << ",," << assignment.g_DemandPeriodVector[tau].demand_period.c_str() << ",";
			assignment.summary_file << assignment.g_AgentTypeVector[at].agent_type.c_str() << ",";

			int link_count = 0;
			double total_speed = 0;
			double total_length = 0;
			double total_lane_capacity = 0;
			double total_link_capacity = 0;

			for (int i = 0; i < g_link_vector.size(); i++)
			{
				if (g_link_vector[i].link_type >= 0 && g_link_vector[i].AllowAgentType(assignment.g_AgentTypeVector[at].agent_type, tau))
				{
					link_count++;
					total_speed += g_link_vector[i].free_speed;
					total_length += g_link_vector[i].length_in_meter * g_link_vector[i].number_of_lanes;
					total_lane_capacity += g_link_vector[i].lane_capacity;
					total_link_capacity += g_link_vector[i].lane_capacity * g_link_vector[i].number_of_lanes;
				}
			}
			assignment.summary_file << link_count << "," << total_speed / max(1, link_count) << "," <<
				total_length/1000.0 << "," <<
				total_link_capacity << "," <<
				total_lane_capacity / max(1, link_count) << "," << total_length / max(1, link_count) << "," << endl;
		}
	/// <summary>
	/// ///////////////////////////
	/// </summary>
	/// <param name="assignment"></param>
	assignment.summary_file << ",summary by road link type,link_type,link_type_name,# of links,avg_free_speed,total_length,total_capacity,avg_lane_capacity,avg_length_in_meter," << endl;
	std::map<int, CLinkType>::iterator it_link_type;
	int count_zone_demand = 0;
	for (it_link_type = assignment.g_LinkTypeMap.begin(); it_link_type != assignment.g_LinkTypeMap.end(); ++it_link_type)
	{
		assignment.summary_file << ",," << it_link_type->first << "," << it_link_type->second.link_type_name.c_str() << ",";

		int link_count = 0;
		double total_speed = 0;
		double total_length = 0;
		double total_lane_capacity = 0;
		double total_link_capacity = 0;

		for (int i = 0; i < g_link_vector.size(); i++)
		{
			if (g_link_vector[i].link_type >= 0 && g_link_vector[i].link_type == it_link_type->first)
			{
				link_count++;
				total_speed += g_link_vector[i].free_speed;
				total_length += g_link_vector[i].length_in_meter * g_link_vector[i].number_of_lanes;
				total_lane_capacity += g_link_vector[i].lane_capacity;
				total_link_capacity += g_link_vector[i].lane_capacity * g_link_vector[i].number_of_lanes;
			}
		}
		assignment.summary_file << link_count << "," << total_speed / max(1, link_count) << "," <<
			total_length/1000.0 << "," <<
			total_link_capacity << "," <<
			total_lane_capacity / max(1, link_count) << "," << total_length / max(1, link_count) << "," << endl;
	}


	g_OutputModelFiles(1);


}
