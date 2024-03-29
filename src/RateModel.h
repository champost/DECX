/*
 * RateMatrix.h
 *
 *  Created on: Aug 14, 2009
 *      Author: smitty
 */

#ifndef RATEMODEL_H_
#define RATEMODEL_H_


//#include "AncSplit.h"

#include <vector>
#include <map>
#include <string>
using namespace std;

//#include <armadillo>
//using namespace arma;

//octave usage
//#include <octave/oct.h>

class RateModel{
private:
	bool globalext;
	int nareas;
	int maxareas;
	int numthreads;
	vector<string> labels;
	vector<double> periods;
	vector<vector<int> > dists;

	bool classic_vicariance;
	bool rapid_anagenesis;

	//	adjacency conditioned data types
	bool default_adjacency;
	vector<vector<vector<bool> > > adjMat;
	vector<vector<vector<int> > > incldists_per_period;
	vector<vector<int> > incldistsint_per_period;
	vector<vector<vector<int> > > excldists_per_period;
	map<vector<int>, map<int,vector<vector<vector<int> > > > > iter_dists_per_period;

	map<int,string> areanamemaprev;
	map<vector<int>,vector<vector<vector<int> > > > iter_dists;
	map<vector<int>,string> distsmap;
	map<vector<int>, int> distsintmap;
	map<int,vector<int> > intdistsmap;
	vector< vector< vector<double> > >D;
	vector< vector< vector<double> > >Dmask;
	vector< vector<double> > E;
	vector< vector< vector<double> > >Q;
	vector< vector< vector<double> > >QT;//transposed for sparse
	vector< vector< vector<double> > >P;
	vector<int> nzs;
	vector<vector<int> > ia_s;
	vector<vector<int> > ja_s;
	vector<vector<double> > a_s;
	void iter_all_dist_splits();
	void iter_all_dist_splits_per_period();

public:
	RateModel(int na, bool ge, vector<double> pers, bool sp, bool cv, bool ra);
	void set_nthreads(int nthreads);
	int get_nthreads();
	void setup_dists();
	void setup_dists(vector<vector<int> >, bool, const bool display_ranges_detail);
	void setup_adjacency(vector<vector<vector<bool>>>);
	void set_adj_bool(bool adjBool);
	vector< vector<int> > generate_adjacent_dists(int maxareas, map<int,string> areanamemaprev);
	vector< vector<int> >  iterate_all_from_num_max_areas(int m, int n);
	void include_tip_dists(map<string,vector<int> > distrib_data, vector<vector<int> > &includedists, map<int,string> areanamemaprev);
	void setup_Dmask();
	void setup_D_provided(double d, vector< vector< vector<double> > > & D_mask_in);
	void set_Dmask_cell(int period, int area, int area2, double prob, bool sym);
	void setup_D(double d);
	void setup_E(double e);
	void set_Qdiag(int period);
	void setup_Q();
	void set_Qdiag_with_adjacency(int period);
	void setup_Q_with_adjacency();
	vector<vector<double > > setup_fortran_P(int period, double t, bool store_p_matrices);
	vector<vector<double > > setup_sparse_full_P(int period, double t);
	vector<double > setup_sparse_single_column_P(int period, double t, int column);
//	vector<vector<double > > setup_pthread_sparse_P(int period, double t, vector<int> & columns);
	string Q_repr(int period);
	string P_repr(int period);
	vector<vector<int> > enumerate_dists();
	vector<vector<vector<int> > > iter_dist_splits(vector<int> & dist);
	map<int,vector<vector<vector<int> > > > iter_dist_splits_per_period(vector<int> & dist, int distSize);
	//vector<AncSplit> iter_ancsplits(vector<int> dist);
	vector<vector<int> > * getDists();
	map<vector<int>,int> * get_dists_int_map();
	map<int,vector<int> > * get_int_dists_map();
	vector<vector<vector<int> > > * get_iter_dist_splits(vector<int> & dist);
	vector<vector<vector<int> > > * get_iter_dist_splits_per_period(vector<int> & dist, int period);
	vector<vector<int> > * get_incldists_per_period(int period);
	vector<int> * get_incldistsint_per_period(int period);
	vector<vector<int> > * get_excldists_per_period(int period);
	map<int,string> * get_areanamemaprev();
	void remove_dist(vector<int> dist);
	bool sparse;
	int get_num_areas();
	int get_num_periods();
	/*
	 testing storing once optimization has occured
	 map of period and map of bl and p matrix
	 map<period,map<branch length,p matrix>>
	 */
	map<int,map<double, vector<vector<double> > > > stored_p_matrices;

	/*
	 * get things from stmap
	 */
	vector< vector< vector<double> > > & get_Q();
	//this should be used for getting the eigenvectors and eigenvalues
//	bool get_eigenvec_eigenval_from_Q(cx_mat * eigenvalues, cx_mat * eigenvectors, int period);
	//bool get_eigenvec_eigenval_from_Q_octave(ComplexMatrix * eigenvalues, ComplexMatrix * eigenvectors, int period);

	//REQUIRES BOOST
	//vector<vector<double > > setup_P(int period, double t);
};

#endif /* RATEMATRIX_H_ */
