/*
 * BioGeoTree.h
 *
 *  Created on: Aug 15, 2009
 *      Author: Stephen A. Smith
 */

#ifndef BIOGEOTREE_H_
#define BIOGEOTREE_H_

#include <vector>
#include <string>
#include <map>
using namespace std;

#include "RateModel.h"
#include "AncSplit.h"
#include "BranchSegment.h"
#include "tree.h"
#include "node.h"
#include "vector_node_object.h"
#include "BioGeoTreeTools.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

//#include <armadillo>
//using namespace arma;

//octave usage
//#include <octave/oct.h>


class BioGeoTree{
private:
	Tree * tree;
	vector<double> periods;
	string age;
	string dc;
	string en;
	string andc;
	vector<int> * columns;
	vector<int> * whichcolumns;
	RateModel * rootratemodel;
	map<int, vector<int> > * distmap; // a map of int and dist
	bool store_p_matrices;
	bool use_stored_matrices;
	bool ultrametric;	//	is false when at least one of the input trees is non-ultrametric
	BioGeoTreeTools tt;

	//reverse bits
	string revB;
	bool rev;
	//end reverse bits

	//simulate bits
	bool sim;
	double sim_D;
	double sim_E;

	const gsl_rng_type * T;
	gsl_rng * r;
	unsigned long int ran_seed;
	//end simulate bits

	//estimate bits
	bool readSimStates;
	map<int, vector<int> > trueStates;
	double true_D;
	double true_E;
	//end estimate bits

	//stochastic mapping bits
	string rev_exp_number;
	string rev_exp_time;
	bool stochastic;
	//map of period int and then branch length Superdouble
//	map<int,map<double, mat > > stored_EN_matrices;
//	map<int,map<double,cx_mat> > stored_EN_CX_matrices;
//	map<int,map<double, mat > > stored_ER_matrices;
	//end mapping bits

	/*
	 * benchmark variables
	 */
//	clock_t cl1;
//	clock_t cl2;
//	clock_t c3;
//	clock_t c4;
//	clock_t c5;
//	clock_t c6;

public:
//	BioGeoTree() {};
	BioGeoTree(Tree * tr, vector<double> ps);
	void set_store_p_matrices(bool);
	void set_use_stored_matrices(bool);
	void print_segs();
	void set_default_model(RateModel * mod);
	void update_default_model(RateModel * mod);
	Superdouble eval_likelihood(bool marg);
	void set_excluded_dist(vector<int> ind,Node * node);
	void set_tip_conditionals(map<string,vector<int> > distrib_data);
	void set_node_constraints(vector<vector<vector<int> > > exdists_per_period, map<int,string> areanamemaprev);
	vector<Superdouble> conditionals(Node & node, bool marg, bool sparse);
	//void ancdist_conditional_lh(bpp::Node & node, bool marg);
	void ancdist_conditional_lh(Node & node, bool marg);
	void set_ultrametric(bool ultMet);

/*
	fossil data
 */
	void setFossilatNodeByMRCA(vector<string> nodeNames, int fossilarea);
	void setFossilatNodeByMRCA_id(Node * id, int fossilarea);
	void setFossilatBranchByMRCA(vector<string> nodeNames, int fossilarea, double age);
	void setFossilatInternalBranchByMRCA_id(Node * id, int fossilarea, double age);
	void setFossilatExternalBranchByMRCA_id(Node * id, int fossilarea, double age);

/*
	for calculating forward and reverse
 */
	void prepare_ancstate_reverse();
	void reverse(Node &);
	map<vector<int>,vector<AncSplit> > calculate_ancsplit_reverse(Node & node,bool marg);
	vector<Superdouble> calculate_ancstate_reverse(Node & node,bool marg);
/*
	for forward simulations
 */
	void prepare_simulation(unsigned long int seed, bool ranPar);
	void simulate(Node & node);
	double getSim_D();
	double getSim_E();
	void setSim_D(const double disp);
	void setSim_E(const double ext);

	void read_true_states(string truestatesfile);
	vector<int> * get_true_state(int num);
	double getTrue_D();
	double getTrue_E();

	~BioGeoTree();
	//need to override these at some point
//	BioGeoTree(const BioGeoTree &L);             // copy constructor
//    BioGeoTree & operator=(const BioGeoTree &L);

/*
 * for calculating forward and reverse for expected values (stochastic mapping)
 */
//	void prepare_stochmap_reverse_all_nodes(int, int);
	vector<Superdouble> calculate_reverse_stochmap(Node &, bool);
	vector<Superdouble> calculate_reverse_stochmap_TEST(Node & node,bool time);


/*
	for timing things
 */
	Superdouble ti;
	Superdouble ti2;
	Superdouble ti3;
};

#endif /* BIOGEOTREE_H_ */
