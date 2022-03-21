/*
 * main.cpp
 *
 *  Created on: Aug 14, 2009
 *      Author: Stephen A. Smith
 */

#include <ctime>
#include <vector>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <map>
#include <set>
#include <cmath>
#include <cfloat>
#include <iomanip>
#include <ctime>
#include <numeric>

using namespace std;

#include "RateMatrixUtils.h"
#include "BioGeoTreeTools.h"
#include "RateModel.h"
#include "BioGeoTree.h"
#include "OptimizeBioGeo.h"
//#include "OptimizeBioGeoAllDispersal.h"
//#include "OptimizeBioGeoAllDispersal_nlopt.h"
#include "InputReader.h"
#include "Utils.h"
//#include "BayesianBioGeo.h"
//#include "BayesianBioGeoAllDispersal.h"
#include "vector_node_object.h"
#include "superdouble.h"
#include "string_node_object.h"
#include "config_parsing.hpp"

#ifdef XYZ
#include "gmpfrxx/gmpfrxx.h"
#endif

//#define DEBUG

int main(int argc, char* argv[]){

  using namespace config;

  // Get a few things out of the way.
  if (argc < 2) {
    std::cerr << "No configuration file provided. Exiting." << std::endl;
    exit(1);
  } else if (argc > 2) {
    std::cerr << "Too many arguments. Exiting." << std::endl;
    exit(1);
  }

  // Parse TOML configuration file.
  std::string toml_file{argv[1]};
  toml::table parsed;
  try {
    parsed = toml::parse_file(toml_file);
  } catch (const toml::parse_error& err) {
    std::cerr << "Failed to parse DECX config file:" << std::endl;
    std::cerr << "  " << err << std::endl;
    exit(2);
  }
  Reader config{&parsed};

  //============================================================================
  // The input parameters file has been refreshed below with TOML format,
  // the idea was to improve file structure and error handling
  // without touching the original code below.
  // So, while the parse procedure is refreshed,
  // the data structures used later in the program are left as-is.

  // Input files table. --------------------------------------------------------
  config.into_table("input_files");

  const std::string treefile{config.require_file("tree")};
  const std::string datafile{config.require_file("data")};
  const std::string adjacencyfile{config.seek_file("adjacency").value_or("")};
  const std::optional<std::string> rate_matrix_file{
      config.seek_file("rate_matrix")};

  // Parameters table ----------------------------------------------------------
  config.step_up();
  config.into_table("parameters");
  AncestralState ancestral_states{config.read_ancestral_state()};
  ReportType report_type(config.read_report_type());
  const bool classic_vicariance{
      config.seek_boolean("classic_vicariance").value_or(false)};
  const bool rapid_anagenesis{
      config.seek_boolean("rapid_anagenesis").value_or(false)};
  std::vector<double> periods{config.read_periods()};

  // Geographical parameters ---------------------------------------------------
  config.step_up();
  config.into_table("areas");
  std::vector<std::string> area_names{
      config.read_unique_identifiers("names", "Area")};

  int max_areas{1};
  std::vector<std::vector<int>> includedists;
  std::vector<std::vector<int>> excludedists;
  if (config.into_optional_table("distributions")) {
    std::string constraint_spec{
        config.seek_string("constraint").value_or("include")};
    if (constraint_spec == "exclude") {
      std::cerr << "The feature `constraint = \"exclude\"` is not "
                   "supported by DECX anymore."
                << std::endl;
      config.source_and_exit();
    } else if (constraint_spec != "include") {
      std::cerr << "Invalid distribution constraint: \"" << constraint_spec
                << "\". Valid constraints are: \"include\" and \"exclude\"."
                << std::endl;
      config.source_and_exit();
    }
    const auto& max_spec{config.seek_integer("max")};
    if (max_spec.has_value()) {
      if (config.has_node("set")) {
        std::cerr << "Incompatible distributions specification: "
                  << "'max' and 'set' cannot be both given." << std::endl;
        config.source_and_exit();
      }
      max_areas = max_spec.value();
    } else {
      includedists = config.read_distributions("set", area_names);
    }
    config.step_up();
  }

  config.step_up();

  // MRCA specifications -------------------------------------------------------

  // Read mrca data: Name ↦ species
  std::map<std::string, std::vector<std::string>> mrcas;
  // Fixed nodes: Name ↦ distribution
  std::map<std::string, std::vector<int>> fixnodewithmrca;
  // Fossil MRCA seem organized differently.
  std::vector<std::string> fossilmrca; // name
  std::vector<std::string> fossiltype; // 'N'/'n' or 'B'/'b'
  std::vector<std::string> fossilarea; // distribution
  // "0's for N type and # for B type" according to original comment.
  std::vector<double> fossilage;
  config.read_mrcas(mrcas,
                    fixnodewithmrca,
                    fossilmrca,
                    fossiltype,
                    fossilarea,
                    fossilage,
                    area_names);

  // Output settings -----------------------------------------------------------

  // Output files suffix.
  config.into_table("output");
  const std::string fileTag{config.require_string("file_tag")};
  //============================================================================

  // The following variables have been kept from original code,
  // just not to break it.

		string logfile;
		string truestatesfile;
		vector<vector<vector<int> > > exdists_per_period;
		vector<vector<vector<bool> > > adjMat;
		vector<string> isolatedAreas;
		map<string,int> areanamemap;
		map<int,string> areanamemaprev;
		//then everything will be computed
		bool treecolors;
		vector<string> areacolors;
		time_t likStartTime, likEndTime;

		bool marginal = true; // false means joint
		int numthreads = 0;
		bool sparse = false;
		bool bayesian = false;
		int numreps = 10000;
		bool default_adjacency_matrix = true;
		bool _stop_on_settings_display_ = false;
		bool _stop_on_initial_likelihood_ = false;
		bool LHOODS = false;
		bool NodeLHOODS = false;

		int maxiterations = 1000;
		double stoppingprecision = 0.0001;

		double dispersal = 0.1;
		double extinction = 0.1;
		bool estimate = true;
		bool simulate = false;
		int simNum = 1;
		unsigned long int seed = 314159265;
		bool ultrametric = true;	//	is false when at least one of the input trees is non-ultrametric
		bool readTrueStates = false;
		bool plot_output = false;

		/*
		 * for stochastic mapping
		 */
		vector<vector<vector<int> > > stochastic_number_from_tos;
		vector<vector<int> > stochastic_time_dists;

		//estimating the dispersal mask
		bool estimate_dispersal_mask = false;

		BioGeoTreeTools tt;

  //============================================================================
  // The code below for legacy input file format parsing
  // is kept as raw (archeo-)documentation,
  // but is not executed anymore.
  if (false) {

		/*************
		 * read the configuration file
		 **************/
		ifstream ifs(argv[1]);
		string line;
		while(getline(ifs,line)){
			if(line.size()>0){
				if(&line[0]!="#"){
					vector<string> tokens;
					string del("=");
					tokens.clear();
					Tokenize(line, tokens, del);
					for(unsigned int j=0;j<tokens.size();j++){
						TrimSpaces(tokens[j]);
					}
					if(!strcmp(tokens[0].c_str(),  "likelihoodfile")){
						LHOODS = true;
					}else if(!strcmp(tokens[0].c_str(),  "nodelikelihoodfile")){
						NodeLHOODS = true;
					}else if(!strcmp(tokens[0].c_str(),  "_stop_on_settings_display_")){
						_stop_on_settings_display_ = true;
					}else if(!strcmp(tokens[0].c_str(),  "_stop_on_initial_likelihood_")){
						_stop_on_initial_likelihood_ = true;
					}else if(!strcmp(tokens[0].c_str(), "isolated")){
						vector<string> searchtokens;
						Tokenize(tokens[1], searchtokens, ", 	");
						for(unsigned int j=0;j<searchtokens.size();j++){
							TrimSpaces(searchtokens[j]);
						}
						isolatedAreas = searchtokens;
					}else if(!strcmp(tokens[0].c_str(), "areacolors")){
						vector<string> searchtokens;
						Tokenize(tokens[1], searchtokens, ", 	");
						for(unsigned int j=0;j<searchtokens.size();j++){
							TrimSpaces(searchtokens[j]);
						}
						areacolors = searchtokens;
					}else if(!strcmp(tokens[0].c_str(),  "treecolors")){
						treecolors = true;
					}else if(!strcmp(tokens[0].c_str(),  "calctype")){
						string calctype = tokens[1];
						if(calctype.compare("m") != 0 && calctype.compare("M") != 0){
							marginal = false;
						}
					}else if(!strcmp(tokens[0].c_str(),  "sparse")){
						sparse = true;
					}else if(!strcmp(tokens[0].c_str(),  "estimate_dispersal_mask")){
						estimate_dispersal_mask = true;
					}else if(!strcmp(tokens[0].c_str(),  "numthreads")){
						numthreads = atoi(tokens[1].c_str());
					}else if(!strcmp(tokens[0].c_str(), "stochastic_time")){
						report_type = ReportType::States; // requires ancestral states
            ancestral_states.set_all();
						vector<string> searchtokens;
						Tokenize(tokens[1], searchtokens, ", 	");
						for(unsigned int j=0;j<searchtokens.size();j++){
							TrimSpaces(searchtokens[j]);
						}
						for(unsigned int j=0;j<searchtokens.size();j++){
							vector<int> dist;
							for(unsigned int k=0;k<searchtokens[j].size();k++){
								char c = (searchtokens[j].c_str())[k];
								dist.push_back(atoi(&c));
							}
							stochastic_time_dists.push_back(dist);
						}
					}else if(!strcmp(tokens[0].c_str(), "stochastic_number")){
						report_type = ReportType::States; // requires ancestral states
            ancestral_states.set_all();
						vector<string> searchtokens;
						Tokenize(tokens[1], searchtokens, ", 	");
						for(unsigned int j=0;j<searchtokens.size();j++){
							TrimSpaces(searchtokens[j]);
						}
						if(searchtokens.size() != 2){
							cout << "ERROR: distributions for stochastic_number need to be in the form from_to" << endl;
						}else{
							vector<vector<int> > dists;
							vector<int> dist0;
							for(unsigned int k=0;k<searchtokens[0].size();k++){
								char c = (searchtokens[0].c_str())[k];
								dist0.push_back(atoi(&c));
							}vector<int> dist1;
							for(unsigned int k=0;k<searchtokens[1].size();k++){
								char c = (searchtokens[1].c_str())[k];
								dist1.push_back(atoi(&c));
							}dists.push_back(dist0);dists.push_back(dist1);
							stochastic_number_from_tos.push_back(dists);
						}
					}else if (!strcmp(tokens[0].c_str(), "bayesian")){
						bayesian = true;
						if(tokens.size() > 1){
							numreps = atoi(tokens[1].c_str());
						}
					}else if ((!strcmp(tokens[0].c_str(), "dispersal")) || (!strcmp(tokens[0].c_str(), "sim_dispersal"))){
						if(tokens.size() > 1){
							dispersal = atof(tokens[1].c_str());
							cout << "setting dispersal: " << dispersal << endl;
							if (!strcmp(tokens[0].c_str(), "sim_dispersal"))
								estimate = false;
						}
					}else if ((!strcmp(tokens[0].c_str(), "extinction")) || (!strcmp(tokens[0].c_str(), "sim_extinction"))){
						if(tokens.size() > 1){
							extinction = atof(tokens[1].c_str());
							cout << "setting extinction: " << extinction << endl;
							if (!strcmp(tokens[0].c_str(), "sim_extinction"))
								estimate = false;
						}
					}else if(!strcmp(tokens[0].c_str(), "maxiterations")){
						maxiterations = atoi(tokens[1].c_str());
						cout << "setting maxiterations: " << maxiterations << endl;
					}else if(!strcmp(tokens[0].c_str(), "stoppingprecision")){
						stoppingprecision = atof(tokens[1].c_str());
						cout << "setting stoppingprecision: " << stoppingprecision << endl;
					}else if(!strcmp(tokens[0].c_str(), "simbiogeotree")){
						simulate = true;
						if(tokens.size() > 1)
							simNum = atoi(tokens[1].c_str());
					}else if(!strcmp(tokens[0].c_str(), "seed")){
						seed = atol(tokens[1].c_str());
						cout << "User-specified seed is: " << seed << endl;
					}else if(!strcmp(tokens[0].c_str(), "read_true_states")){
						truestatesfile = tokens[1];
						readTrueStates = true;
						simulate = false;
					}else if(!strcmp(tokens[0].c_str(),  "not_ultrametric")){
						ultrametric = false;
					}else if(!strcmp(tokens[0].c_str(),  "plot_output")){
						plot_output = true;
					}
				}
			}
		}
		ifs.close();
		/*****************
		 * finish reading the configuration file
		 *****************/
  }
  //============================================================================

		/*
		 * after reading the input file
		 */
		InputReader ir;
		cout << "reading tree..." << endl;
		vector<Tree *> intrees;
		map<string,vector<int> > data;
		ir.readMultipleTreeFile(treefile,intrees);
		if (!simulate) {
			cout << "reading data..." << endl;
			data = ir.readStandardInputData(datafile);
			cout << "checking data..." << endl;
			ir.checkData(data,intrees);
		}
		else
			ir.nspecies = intrees[0]->getExternalNodeCount();

		/*
		 * read area names
		 */
		if(area_names.size() > 0){
			cout << "reading area names" << endl;
			for(unsigned int i=0;i<area_names.size();i++){
				areanamemap[area_names[i]] = i;
				areanamemaprev[i] = area_names[i];
				cout << i <<"=" <<area_names[i] << endl;
			}
			if (simulate)
				ir.nareas = area_names.size();
		}
		else{
			if (!simulate) {
				for(int i=0;i<ir.nareas;i++){
					std::ostringstream osstream;
					osstream << i;
					std::string string_x = osstream.str();
					areanamemap[string_x] = i;
					areanamemaprev[i] = string_x;
				}
			}
			else {
				cout << "Information on the total number of areas is missing! "
						"Please use the `names = [...]` parameter under [areas] table." << endl;
				exit(-1);
			}
		}
		/*
		 * need to figure out how to work with multiple trees best
		 */
		if(periods.size() < 1){
			periods.push_back(10000);
		}
		RateModel rm(ir.nareas,true,periods,sparse,classic_vicariance,rapid_anagenesis);
		if(numthreads != 0){
			rm.set_nthreads(numthreads);
			cout << "Setting the number of threads: " << numthreads << endl;
		}
		rm.setup_Dmask();

		/*
		 * if there is a ratematrixfile then it will be processed
		 */
		if(rate_matrix_file.has_value()){
			cout << "Reading rate matrix file" << endl;
			vector<vector<vector<double> > > dmconfig = processRateMatrixConfigFile(rate_matrix_file.value(),ir.nareas,periods.size());
			for(unsigned int i=0;i<dmconfig.size();i++){
				for(unsigned int j=0;j<dmconfig[i].size();j++){
					for(unsigned int k=0;k<dmconfig[i][j].size();k++){
						if(dmconfig[i][j][k] != 1){
							cout << dmconfig[i][j][k] << "\t";
						}else{
							cout << " . " << "\t";
						}
//						cout << " ";
					}
					cout << endl;
				}cout << endl;
			}
			for(unsigned int i=0;i<dmconfig.size();i++){
				for(unsigned int j=0;j<dmconfig[i].size();j++){
					for(unsigned int k=0;k<dmconfig[i][j].size();k++){
						rm.set_Dmask_cell(i,j,k,dmconfig[i][j][k],false);
					}
				}
			}
		}
	    /*
		* if there is a adjacencymatrixfile then it will be processed
		*/
		if (adjacencyfile.size() > 0)
			rm.setup_adjacency(adjacencyfile, area_names);
		else {
			cout << "\nUsing the default adjacency matrix for all time slices..." << endl;
			for (unsigned int j = 0; j < area_names.size(); j++)
				cout << "\t" << area_names[j];
			cout << endl << endl;
			for (unsigned int i = 0; i < area_names.size(); i++) {
				cout << area_names[i] << "\t";
				for (unsigned int j = 0; j <= i; j++)
					cout << 1 << "\t";
				cout << endl;
			}
			cout << endl;
		}
		/*
	  need to add check to make sure that the tips are included in possible distributions
		 */
//		if(includedists.size() > 0 || excludedists.size() > 0 || maxareas >= 2){
//			if(excludedists.size() > 0){
//				rm.setup_dists(excludedists,false);
//			}else{
//				if(maxareas >= 2)
//					includedists = generate_dists_from_num_max_areas(ir.nareas,maxareas);
//				rm.setup_dists(includedists,true);
//			}
//		}else{
//			rm.setup_dists();
//		}
		includedists = rm.generate_adjacent_dists(max_areas, areanamemaprev);
		if (!simulate)
			rm.include_tip_dists(data, includedists, areanamemaprev);
		rm.setup_dists(includedists,true);
		rm.setup_D(dispersal);
		rm.setup_E(extinction);
//		rm.setup_Q();
		rm.setup_Q_with_adjacency();

		/*
		 * outfile for tree reconstructed states
		 */
		ofstream outTreeFile;
		ofstream outTreeKeyFile;

		/*
		 * outfile for stochastic expectations
		 */
		ofstream outStochTimeFile;
		ofstream outStochNumberFile;

#ifdef DEBUG
		ofstream tmp;
		tmp.open(string("tmp.tre").c_str(),ios::out);
		tmp << intrees[0]->getRoot()->getNewick(true,"onlykey") << ";"<< endl;
		tmp.close();
#endif

		/*
		 * start calculating on all trees
		 */
		for(unsigned int i=0;i<intrees.size();i++){
			BioGeoTree bgt(intrees[i],periods);
			/*
			 * specify whether the tree is ultrametric
			 */
			bgt.set_ultrametric(ultrametric);
			/*
			 * set adjacency matrix constraints
			 */
//			if (!default_adjacency_matrix)
//				bgt.set_node_constraints(exdists_per_period, areanamemaprev);
			/*
			 * record the mrcas
			 */
			map<string,Node *> mrcanodeint;
			map<string,vector<string> >::iterator it;
			for(it=mrcas.begin();it != mrcas.end();it++){
				//records node by number, should maybe just point to node
				mrcanodeint[(*it).first] = intrees[i]->getMRCA((*it).second);
				//tt.getLastCommonAncestor(*intrees[i],nodeIds);
				cout << "Reading mrca: " << (*it).first << " = ";
				for (unsigned int k = 0;k < (*it).second.size(); k ++){
					cout <<  (*it).second[k]<< " ";
				}
				cout <<endl;
			}

			/*
			 * set fixed nodes
			 */
			map<string,vector<int> >::iterator fnit;
			for(fnit = fixnodewithmrca.begin(); fnit != fixnodewithmrca.end(); fnit++){
				vector<int> dista = (*fnit).second;
				for(unsigned int k=0;k<rm.getDists()->size();k++){
					bool isnot = true;
					for(unsigned int j=0;j<dista.size();j++){
						if(dista[j] != rm.getDists()->at(k)[j])
							isnot = false;
					}
					if(isnot == false){
						bgt.set_excluded_dist(rm.getDists()->at(k),mrcanodeint[(*fnit).first]);
						//bgt.set_excluded_dist(rm.getDists()->at(k),intrees[0]->getNode(mrcanodeint[(*fnit).first]));
					}
				}
				cout << "fixing " << (*fnit).first << " = ";print_vector_int((*fnit).second);
			}

			cout << "setting default model..." << endl;
			bgt.set_default_model(&rm);
			if (!simulate) {
				cout << "setting up tips..." << endl;
				bgt.set_tip_conditionals(data);
			}

			/*
			 * setting up fossils
			 */
			for (std::size_t k{0}; k < fossiltype.size(); ++k) {
        // TODO: remove checks if config file parsing
        // make it impossible to screw it up.
        const auto& type{fossiltype.at(k)};
        const auto& mrca{fossilmrca.at(k)};
        if (!mrcanodeint.count(mrca)) {
          std::cerr << "ERROR: undefined MRCA: " << mrca << std::endl;
          exit(1);
        }
        const auto& node_id{mrcanodeint.at(mrca)};
        const auto& area_name{fossilarea.at(k)};
        if (!areanamemap.count(area_name)) {
          std::cerr << "ERROR: undefined area name for " << mrca
                    << ": " << area_name << std::endl;
          exit(1);
        }
        const auto& area_id{areanamemap.at(area_name)};
        const auto& age{fossilage.at(k)};
				if(type == "n" || type == "N"){
					bgt.setFossilatNodeByMRCA_id(node_id, area_id);
					cout << "Setting node fossil at mrca: " << mrca << " at area: " << area_name << endl;
				}else if(type == "b" || type == "B"){
					if (node_id->isInternal()) {
						bgt.setFossilatInternalBranchByMRCA_id(node_id, area_id, age);
						cout << "Setting INTERNAL branch fossil at mrca: " << mrca << " at area: " << area_name << " at age: " << age << endl;
					}
					else {
						bgt.setFossilatExternalBranchByMRCA_id(node_id, area_id, age);
						cout << "Setting EXTERNAL branch fossil at mrca: " << mrca << " at area: " << area_name << " at age: " << age << endl;
					}
				}
			}

			if (_stop_on_settings_display_) {
				cout << "\n STOPPING after having displayed settings" << endl;
				exit(1);
			}

			if (simulate) {
				if (intrees.size() == 1) {
					time(&likStartTime);

					if (simNum > 1)
						cout << "simulating " << simNum << " biogeotrees..." << endl;
					else
						cout << "simulating a single biogeotree..." << endl;
					for (int sims = 1; sims <= simNum; sims++) {
						if (simNum > 1)
							cout << sims << "\t";
						if (estimate == false) {
							bgt.setSim_D(dispersal);
							bgt.setSim_E(extinction);
							bgt.prepare_simulation(seed + sims - 1, false);
						}
						else
							bgt.prepare_simulation(seed + sims - 1, true);

						//	output simulated states at the nodes in Newick format
						ofstream simTree;
						if (simNum > 1) {
							stringstream stst;
							stst << "SimTree." << sims << ".tre";
							simTree.open(stst.str().c_str(),ios::out);
						}
						else
							simTree.open(string("SimTree.tre").c_str(),ios::out);
						simTree << intrees[0]->getRoot()->getNewick(true,"simstate") << ";"<< endl;
						simTree.close();

						//	output leaf distributions
						ofstream simDistrib;
						set<vector<int> > leafDistrib;
						if (simNum > 1) {
							stringstream stst;
							stst << "SimDistrib." << sims << ".txt";
							simDistrib.open(stst.str().c_str(),ios::out);
						}
						else
							simDistrib.open(string("SimDistrib.txt").c_str(),ios::out);
						simDistrib << intrees[0]->getExternalNodeCount() << " " << rm.get_num_areas() << endl;
						for(size_t i = 0; i < intrees[0]->getExternalNodeCount(); i++) {
							simDistrib << intrees[0]->getExternalNode(i)->getName() << "\t";
							vector<int> tipDist = (*rm.get_int_dists_map())[*intrees[0]->getExternalNode(i)->getIntObject("simdistidx")];
							for(size_t j = 0; j < tipDist.size(); j++)
								simDistrib << tipDist[j];
							simDistrib << endl;
							leafDistrib.insert(tipDist);
						}
						simDistrib.close();

						//	output interior node distributions
						ofstream simStates;
						if (simNum > 1) {
							stringstream stst;
							stst << "SimStates." << sims << ".txt";
							simStates.open(stst.str().c_str(),ios::out);
						}
						else
							simStates.open(string("SimStates.txt").c_str(),ios::out);
						simStates << bgt.getSim_D() << endl << bgt.getSim_E() << endl;
						for(size_t i = 0; i < intrees[0]->getInternalNodeCount(); i++) {
							simStates << intrees[0]->getInternalNode(i)->getNumber() << "\t";
							vector<int> nodeDist = (*rm.get_int_dists_map())[*intrees[0]->getInternalNode(i)->getIntObject("simdistidx")];
							for(size_t j = 0; j < nodeDist.size(); j++)
								simStates << nodeDist[j];
							simStates << endl;
						}
						simStates.close();

						cout << "\tleaf_dists : " << leafDistrib.size() << endl;
					}
					time(&likEndTime);
					cout << "Time taken for simulation: " <<  float(likEndTime - likStartTime) << " s." << endl << endl;
				}
				else {
					cout << "Simulating ancestral states for multiple trees simultaneously is currently not possible!" << endl;
					exit(-1);
				}
			}
			else {
				/*
				 * read the true ancestral states (i.e. a simulated dataset)
				 */
				if (readTrueStates) {
					cout << "\nreading the true ancestral states for this tree..." << endl;
					bgt.read_true_states(truestatesfile);
					cout << endl;
				}

				/*
				 * initial likelihood calculation
				 */
				cout << "starting likelihood calculations" << endl;
				time(&likStartTime);
				cout << "initial -ln likelihood: " << double(bgt.eval_likelihood(marginal)) <<endl;
				time(&likEndTime);
				cout << "Time taken for initial -ln likelihood: " <<  float(likEndTime - likStartTime) << " s." << endl << endl;

				time(&likStartTime);

				if (_stop_on_initial_likelihood_) {
					cout << "\n STOPPING after having calculated the initial likelihood" << endl;
					exit(1);
				}

				/*
				 * optimize likelihood
				 */
				Superdouble nlnlike = 0;
				double optDisp, optExt, optLik;
				if (estimate == true){
					if(estimate_dispersal_mask == false){
						cout << "Optimizing (simplex) -ln likelihood." << endl;
						OptimizeBioGeo opt(&bgt,&rm,marginal,maxiterations,stoppingprecision);
						vector<double> disext  = opt.optimize_global_dispersal_extinction(dispersal, extinction);
						cout << "dis: " << disext[0] << " ext: " << disext[1] << endl;
						optDisp = disext[0];
						optExt = disext[1];
						rm.setup_D(disext[0]);
						rm.setup_E(disext[1]);
//						rm.setup_Q();
						rm.setup_Q_with_adjacency();
						bgt.update_default_model(&rm);
						bgt.set_store_p_matrices(true);
//						cout << "final -ln likelihood: "<< double(bgt.eval_likelihood(marginal)) <<endl;
						optLik = double(bgt.eval_likelihood(marginal));
						cout << "final -ln likelihood: "<< optLik << endl;
						bgt.set_store_p_matrices(false);
					}
					/*
					else{//optimize all the dispersal matrix
						cout << "Optimizing (simplex) -ln likelihood with all dispersal parameters free." << endl;
						//OptimizeBioGeoAllDispersal opt(&bgt,&rm,marginal);
						//vector<double> disextrm  = opt.optimize_globa//l_dispersal_extinction();
						vector<double> disextrm = optimize_dispersal_extinction_all_nlopt(&bgt,&rm);
						cout << "dis: " << disextrm[0] << " ext: " << disextrm[1] << endl;
						vector<double> cols(rm.get_num_areas(), 0);
						vector< vector<double> > rows(rm.get_num_areas(), cols);
						vector< vector< vector<double> > > D_mask = vector< vector< vector<double> > > (periods.size(), rows);
						int count = 2;
						for (unsigned int i=0;i<D_mask.size();i++){
							for (unsigned int j=0;j<D_mask[i].size();j++){
								D_mask[i][j][j] = 0.0;
								for (unsigned int k=0;k<D_mask[i][j].size();k++){
									if(k!= j){
										D_mask[i][j][k] = disextrm[count];
										count += 1;
									}
								}
							}
						}
						cout << "D_mask" <<endl;
						for (unsigned int i=0;i<D_mask.size();i++){
							cout << periods.at(i) << endl;
							cout << "\t";
							for(unsigned int j=0;j<D_mask[i].size();j++){
								cout << area_names[j] << "\t" ;
							}
							cout << endl;
							for (unsigned int j=0;j<D_mask[i].size();j++){
								cout << area_names[j] << "\t" ;
								for (unsigned int k=0;k<D_mask[i][j].size();k++){
									cout << D_mask[i][j][k] << "\t";
								}
								cout << endl;
							}
							cout << endl;
						}
						rm.setup_D_provided(disextrm[0],D_mask);
						rm.setup_E(disextrm[1]);
						rm.setup_Q();
						bgt.update_default_model(&rm);
						bgt.set_store_p_matrices(true);
						nlnlike = bgt.eval_likelihood(marginal);
						cout << "final -ln likelihood: "<< double(nlnlike) <<endl;
						bgt.set_store_p_matrices(false);
					}
					*/
				}else{
					rm.setup_D(dispersal);
					rm.setup_E(extinction);
//					rm.setup_Q();
					rm.setup_Q_with_adjacency();
					bgt.update_default_model(&rm);
					bgt.set_store_p_matrices(true);
					cout << "final -ln likelihood: "<< double(bgt.eval_likelihood(marginal)) <<endl;
					bgt.set_store_p_matrices(false);
				}


				time(&likEndTime);
				cout << "Time taken for final -ln likelihood: " <<  float(likEndTime - likStartTime) << " s." << endl << endl;

				if (intrees.size() == 1) {
					ofstream LHOODFile;
					if (LHOODS) {
						LHOODFile.open("LHOODS.txt",ios::out | ios::app);
						LHOODFile << optDisp << "\t" << optExt << "\t" << optLik << "\t";
						if (fixnodewithmrca.find("ROOT") != fixnodewithmrca.end()) {
							vector<int> in = fixnodewithmrca["ROOT"];
							for (unsigned int i = 0; i < in.size(); i++)
								LHOODFile << in[i];

							LHOODFile << "\t" << print_area_vector(in,areanamemaprev) << "\t";
						}
						LHOODFile << float(likEndTime - likStartTime) << "s"  << endl;
						LHOODFile.close();
					}
				}

				/*
				 * testing BAYESIAN
				 */
//				if (bayesian == true){
//					//BayesianBioGeo bayes(&bgt,&rm,marginal,numreps);
//					//bayes.run_global_dispersal_extinction();
//					BayesianBioGeoAllDispersal bayes(&bgt,&rm,marginal,numreps);
//					bayes.run_global_dispersal_extinction();
//				}

				/*
				 * ancestral splits calculation
				 */
				if(ancestral_states.some()){
					bgt.set_use_stored_matrices(true);

					bgt.prepare_ancstate_reverse();
					Superdouble totlike = 0; // calculate_vector_double_sum(rast) , should be the same for every node

					if(ancestral_states.all){

						ofstream outTipLabelFile, outAreaNameFile;
						ofstream outBestStateFreqFile, outAreaFreqFile, outTreeFile;
						if (plot_output) {
							outTipLabelFile.open(string("tip.labels.out").c_str(),ios::out);
							outAreaNameFile.open(string("area.names.txt").c_str(),ios::out);
							outBestStateFreqFile.open(string("node.best.state.out").c_str(),ios::out);
							outAreaFreqFile.open(string("node.areas.out").c_str(),ios::out);
							outTreeFile.open(string("user.tree.tre").c_str(),ios::out);


							for(int j=0;j<intrees[i]->getExternalNodeCount();j++){
								Node * currNode = intrees[i]->getExternalNode(j);
								vector<int> tipDist = data[currNode->getName()];
	//							outTipLabelFile << currNode->getName() << "\t" << print_area_vector(tipDist,areanamemaprev) << endl;
								outTipLabelFile << print_area_vector(tipDist,areanamemaprev) << endl;
							}

							for(unsigned int area = 0; area < area_names.size(); area++)
								outAreaNameFile << area_names[area] << "\t";
							outAreaNameFile << endl;

							if ((intrees.size() == 1))
								outTreeFile << *(intrees[0]->getNewickStr());
							outTipLabelFile.close();
							outAreaNameFile.close();
							outTreeFile.close();
						}

						for(int j=0;j<intrees[i]->getInternalNodeCount();j++){

              // Old paired ifs replaced by switch when introducing ReportType.
              switch (report_type) {
                case config::ReportType::Splits: {

								cout << "Ancestral splits for:\t" << intrees[i]->getInternalNode(j)->getNumber() <<endl;
								map<vector<int>,vector<AncSplit> > ras = bgt.calculate_ancsplit_reverse(*intrees[i]->getInternalNode(j),marginal);
								//bgt.ancstate_calculation_all_dists(*intrees[i]->getNode(j),marginal);
								tt.summarizeSplits(intrees[i]->getInternalNode(j),ras,areanamemaprev,&rm);
								cout << endl;

                  break;
                }
                case config::ReportType::States: {

								cout << "Ancestral states for:\t" << intrees[i]->getInternalNode(j)->getNumber() <<endl;
								vector<Superdouble> rast = bgt.calculate_ancstate_reverse(*intrees[i]->getInternalNode(j),marginal);
								totlike = calculate_vector_Superdouble_sum(rast);

								ofstream NodeLHOODFile;
								if (NodeLHOODS && (intrees.size() == 1)) {
									stringstream fname, stst;
									fname << "Node_" << intrees[i]->getInternalNode(j)->getNumber() << ".txt";

									NodeLHOODFile.open(fname.str().c_str(),ios::out | ios::app);
									if (fixnodewithmrca.find("ROOT") != fixnodewithmrca.end()) {
										vector<int> in = fixnodewithmrca["ROOT"];

										NodeLHOODFile << "ROOT fixed at ";
										for (unsigned int areabit = 0; areabit < in.size(); areabit++)
											NodeLHOODFile << in[areabit];

										NodeLHOODFile << " (" << print_area_vector(in,areanamemaprev) << ")\t";
										tt.summarizeAncState(intrees[i]->getInternalNode(j),rast,areanamemaprev,&rm, NodeLHOODS, NodeLHOODFile);
									}
								}
								else {
									tt.summarizeAncState(intrees[i]->getInternalNode(j),rast,areanamemaprev,&rm, false, NodeLHOODFile);

									if (plot_output) {
//										outPieFreqFile << intrees[i]->getInternalNode(j)->getNumber() << "\t";

										Superdouble zero(0);
										Superdouble best(rast[1]);
										int bestdistindex = 1;
										vector<vector<int> > * allDists = rm.getDists();

										//	freqs for the best range
										for (unsigned int dist = 2; dist < rast.size(); dist++) {
											if ((rast[dist] != zero) && (rast[dist] >= best)) {
												best = rast[dist];
												bestdistindex = dist;
											}
										}
										vector<int> nodeDist = allDists->at(bestdistindex);
										outBestStateFreqFile << print_area_vector(nodeDist,areanamemaprev);
//										int sum = accumulate(nodeDist.begin(),nodeDist.end(),0);
//										for (unsigned int area = 0; area < nodeDist.size(); area++)
//											outBestStateFreqFile << double(nodeDist[area])/double(sum) << "\t";


										//	freqs only for all areas
										for (unsigned int area = 0; area < area_names.size(); area++) {
											Superdouble sum = 0;
											for (unsigned int dist = 1; dist < rast.size(); dist++) {
												if ((rast[dist] > zero) && (allDists->at(dist)[area] == 1))
													sum+= rast[dist]/Superdouble(accumulate(allDists->at(dist).begin(),allDists->at(dist).end(),0))/totlike;
											}
											outAreaFreqFile << double(sum) << "\t";
										}

										outBestStateFreqFile << endl;
										outAreaFreqFile << endl;
									}


								}
								NodeLHOODFile.close();
								cout << endl;
                  break;
                }
              }
						}
						/*
						 * key file output
						 */
						if (!readTrueStates) {
							if (intrees.size() > 1)
								outTreeKeyFile.open((treefile+fileTag+".bgkey.tre").c_str(),ios::app);
							else
								outTreeKeyFile.open((treefile+fileTag+".bgkey.tre").c_str(),ios::out);
							//need to output numbers for internal nodes and states for the tips
							for(int j=0;j<intrees[i]->getExternalNodeCount();j++){
								Node * currNode = intrees[i]->getExternalNode(j);
								StringNodeObject str(print_area_vector(data[currNode->getName()],areanamemaprev));
								currNode->assocObject("state",str);
							}
							outTreeKeyFile << intrees[i]->getRoot()->getNewick(true,"number") << ";"<< endl;
							outTreeKeyFile.close();
							for(int j=0;j<intrees[i]->getExternalNodeCount();j++){
								if (intrees[i]->getExternalNode(j)->getObject("state")!= NULL)
									delete intrees[i]->getExternalNode(j)->getObject("state");
							}
						}

						if (plot_output) {
							outBestStateFreqFile.close();
							outAreaFreqFile.close();
							string callstring = "R --vanilla --args < plot_pies.R";
							system(callstring.c_str());
						}
					}else{
            auto& ancstates{ancestral_states.states};
						for(unsigned int j=0;j<ancstates.size();j++){
              // Old paired ifs replaced by switch when introducing ReportType.
              switch (report_type) {
                case config::ReportType::Splits: {

								cout << "Ancestral splits for: " << ancstates[j] <<endl;
								map<vector<int>,vector<AncSplit> > ras = bgt.calculate_ancsplit_reverse(*mrcanodeint[ancstates[j]],marginal);
								tt.summarizeSplits(mrcanodeint[ancstates[j]],ras,areanamemaprev,&rm);
                  break;
                }
                case config::ReportType::States: {

								cout << "Ancestral states for: " << ancstates[j] <<endl;
								vector<Superdouble> rast = bgt.calculate_ancstate_reverse(*mrcanodeint[ancstates[j]],marginal);

								ofstream NodeLHOODFile;
								if (NodeLHOODS && (intrees.size() == 1)) {
									stringstream fname, stst;
									fname << "Node_" << intrees[i]->getInternalNode(j)->getNumber() << ".txt";

									NodeLHOODFile.open(fname.str().c_str(),ios::out | ios::app);
									if (fixnodewithmrca.find("ROOT") != fixnodewithmrca.end()) {
										vector<int> in = fixnodewithmrca["ROOT"];

										NodeLHOODFile << "ROOT fixed at ";
										for (unsigned int areabit = 0; areabit < in.size(); areabit++)
											NodeLHOODFile << in[areabit];

										NodeLHOODFile << " (" << print_area_vector(in,areanamemaprev) << ")\t";
										tt.summarizeAncState(mrcanodeint[ancstates[j]],rast,areanamemaprev,&rm, NodeLHOODS, NodeLHOODFile);
									}
								}
								else
									tt.summarizeAncState(mrcanodeint[ancstates[j]],rast,areanamemaprev,&rm, false, NodeLHOODFile);
								NodeLHOODFile.close();
								cout << endl;
                  break;
                }
              }
						}
					}
					if(report_type == ReportType::Splits && !readTrueStates){
						if (intrees.size() > 1) {
							outTreeFile.open((treefile+fileTag+".bgsplits.tre").c_str(),ios::app);
            }
						else {
							outTreeFile.open((treefile+fileTag+".bgsplits.tre").c_str(),ios::out);
            }
						//need to output object "split"
						for(int j=0;j<intrees[i]->getExternalNodeCount();j++){
							Node * currNode = intrees[i]->getExternalNode(j);
							StringNodeObject str(print_area_vector(data[currNode->getName()],areanamemaprev));
							currNode->assocObject("split",str);
						}
						outTreeFile << intrees[i]->getRoot()->getNewick(true,"split") << ";"<< endl;
						outTreeFile.close();
						for(int j=0;j<intrees[i]->getNodeCount();j++){
							if (intrees[i]->getNode(j)->getObject("split")!= NULL)
								delete intrees[i]->getNode(j)->getObject("split");
						}
					}
					if(report_type == ReportType::States){
						if (!readTrueStates) {
							if (intrees.size() > 1)
								outTreeFile.open((treefile+fileTag+".bgstates.tre").c_str(),ios::app);
							else
								outTreeFile.open((treefile+fileTag+".bgstates.tre").c_str(),ios::out);
							//need to output object "state"
							for(int j=0;j<intrees[i]->getExternalNodeCount();j++){
								Node * currNode = intrees[i]->getExternalNode(j);
								StringNodeObject str(print_area_vector(data[currNode->getName()],areanamemaprev));
								currNode->assocObject("state",str);
							}
							outTreeFile << intrees[i]->getRoot()->getNewick(true,"state") << ";"<< endl;
							outTreeFile.close();
							for(int j=0;j<intrees[i]->getNodeCount();j++){
								if (intrees[i]->getNode(j)->getObject("state")!= NULL)
									delete intrees[i]->getNode(j)->getObject("state");
							}
						}
						else {
							//	output xor diff between true and inferred ancestral states
							ofstream checkStates;
							checkStates.open(("CheckStates"+fileTag+".txt").c_str(),ios::out);
							checkStates << "True dispersal rate : " << bgt.getTrue_D() << endl
										<< "True extinction rate : " << bgt.getTrue_E() << endl
										<< "\nEstimated dispersal rate : " << optDisp << endl
										<< "Estimated extinction rate : " << optExt << endl
										<< "\nEstimated dispersal bias : " << bgt.getTrue_D() - optDisp << endl
										<< "Estimated extinction bias : " << bgt.getTrue_E() - optExt << endl;
							int matchCount = 0;
							for(size_t i = 0; i < intrees[0]->getInternalNodeCount(); i++) {
								int num = intrees[0]->getInternalNode(i)->getNumber();
								vector<int> nodeDist = (*rm.get_int_dists_map())[*intrees[0]->getInternalNode(i)->getIntObject("bestdistidx")];
								if (calculate_vector_int_sum_xor(*bgt.get_true_state(num),nodeDist) == 0)
									++matchCount;
							}
							checkStates << "\nFraction of correctly estimated states : " <<  (double) matchCount / intrees[0]->getExternalNodeCount() << endl
										<< "Number of ancestral nodes : " << intrees[0]->getExternalNodeCount() << endl;
							checkStates.close();
						}
					}


	/*
					//cout << bgt.ti/CLOCKS_PER_SEC << " secs for anc" << endl;

//					stochastic mapping calculations
//					REQUIRES that ancestral calculation be done

					if(stochastic_time_dists.size() > 0){
						cout << "calculating stochastic mapping time spent" << endl;
						for(unsigned int k=0;k<stochastic_time_dists.size();k++){
							cout << tt.get_string_from_dist_int((*rm.get_dists_int_map())[stochastic_time_dists[k]],areanamemaprev,&rm)<< endl;
							bgt.prepare_stochmap_reverse_all_nodes((*rm.get_dists_int_map())[stochastic_time_dists[k]],
									(*rm.get_dists_int_map())[stochastic_time_dists[k]]);
							bgt.prepare_ancstate_reverse();
							outStochTimeFile.open((treefile+".bgstochtime.tre").c_str(),ios::app );
							for(int j=0;j<intrees[i]->getNodeCount();j++){
								if(intrees[i]->getNode(j) != intrees[i]->getRoot()){
									vector<Superdouble> rsm = bgt.calculate_reverse_stochmap(*intrees[i]->getNode(j),true);
									//cout << calculate_vector_double_sum(rsm) / totlike << endl;
									VectorNodeObject<double> stres(1);
									stres[0] = calculate_vector_Superdouble_sum(rsm) / totlike;
									intrees[i]->getNode(j)->assocObject("stoch", stres);
								}
							}
							//need to output object "stoch"
							outStochTimeFile << intrees[i]->getRoot()->getNewickOBL("stoch") <<
									tt.get_string_from_dist_int((*rm.get_dists_int_map())[stochastic_time_dists[k]],areanamemaprev,&rm) << ";"<< endl;
							outStochTimeFile.close();
							for(int j=0;j<intrees[i]->getNodeCount();j++){
								if (intrees[i]->getNode(j)->getObject("stoch")!= NULL)
									delete intrees[i]->getNode(j)->getObject("stoch");
							}
						}
					}
					if(stochastic_number_from_tos.size() > 0){
						cout << "calculating stochastic mapping number of transitions" << endl;
						for(unsigned int k=0;k<stochastic_number_from_tos.size();k++){
							cout << tt.get_string_from_dist_int((*rm.get_dists_int_map())[stochastic_number_from_tos[k][0]],areanamemaprev,&rm)<< " -> "
									<< tt.get_string_from_dist_int((*rm.get_dists_int_map())[stochastic_number_from_tos[k][1]],areanamemaprev,&rm) << endl;
							bgt.prepare_stochmap_reverse_all_nodes((*rm.get_dists_int_map())[stochastic_number_from_tos[k][0]],
									(*rm.get_dists_int_map())[stochastic_number_from_tos[k][1]]);
							bgt.prepare_ancstate_reverse();
							outStochTimeFile.open((treefile+".bgstochnumber.tre").c_str(),ios::app );
							for(int j=0;j<intrees[i]->getNodeCount();j++){
								if(intrees[i]->getNode(j) != intrees[i]->getRoot()){
									vector<Superdouble> rsm = bgt.calculate_reverse_stochmap(*intrees[i]->getNode(j),false);
									//cout << calculate_vector_double_sum(rsm) / totlike << endl;
									VectorNodeObject<double> stres(1);
									stres[0] = calculate_vector_Superdouble_sum(rsm) / totlike;
									intrees[i]->getNode(j)->assocObject("stoch", stres);
								}
							}
							//need to output object "stoch"
							outStochTimeFile << intrees[i]->getRoot()->getNewickOBL("stoch") <<";"<< endl;
							//tt.get_string_from_dist_int((*rm.get_dists_int_map())[stochastic_number_from_tos[k][0]],areanamemaprev,&rm)<< "->"
							//<< tt.get_string_from_dist_int((*rm.get_dists_int_map())[stochastic_number_from_tos[k][1]],areanamemaprev,&rm) << ";"<< endl;
							outStochTimeFile.close();
							for(int j=0;j<intrees[i]->getNodeCount();j++){
								if (intrees[i]->getNode(j)->getObject("stoch")!= NULL)
									delete intrees[i]->getNode(j)->getObject("stoch");
							}
						}
					}
//					 end stochastic mapping

	*/

				}
				//need to delete the biogeostuff
			}
		}

		for(unsigned int i=0;i<intrees.size();i++){
			delete intrees[i];
		}
	cout << "done!" << endl;
	return 0;
}
