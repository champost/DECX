/*
 * RateMatrix.cpp
 *
 *  Created on: Aug 14, 2009
 *      Author: smitty
 */

#define VERBOSE false

#include "RateModel.h"
#include "RateMatrixUtils.h"
#include "Utils.h"
//#include "AncSplit.h"

#include <pthread.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <map>
#include <math.h>
using namespace std;

#include <armadillo>
using namespace arma;

//octave usage
//#include <octave/oct.h>

//#define CBR

RateModel::RateModel(int na, bool ge, vector<double> pers, bool sp):
	globalext(ge),nareas(na),numthreads(0),periods(pers),sparse(sp),
	default_adjacency(true),maxareas(1){}

void RateModel::set_nthreads(int nthreads){
	numthreads = nthreads;
}

int RateModel::get_nthreads(){
	return numthreads;
}

void RateModel::setup_dists(){
	map< int, vector<int> > a = iterate_all_bv(nareas);
	if (globalext){
		vector<int> empt;
		for (unsigned int i=0;i<a[0].size();i++){
			empt.push_back(0);
		}
		dists.push_back(empt);
	}
	map<int, vector<int> >::iterator pos;
	for (pos = a.begin(); pos != a.end(); ++pos){
		int f = pos->first;
		dists.push_back(a[f]);
	}
	/*
	 calculate the distribution map
	 */
	for(unsigned int i=0;i<dists.size();i++){
		distsintmap[dists[i]] = i;
	}
	for(unsigned int i=0;i<dists.size();i++){
		intdistsmap[i] = dists[i];
	}
	/*
	 * precalculate the iterdists
	 */
	iter_all_dist_splits();

	/*
	 print out a visual representation of the matrix
	 */
	if (VERBOSE){
		cout << "dists" <<endl;
		for (unsigned int j=0; j< dists.size(); j++){
			cout << j << " ";
			for (unsigned int i=0;i<dists[j].size();i++){
				cout << dists[j][i];
			}
			cout << endl;
		}
	}
}

/*
 * need to make a generator function for setting distributions
 */
void RateModel::setup_dists(vector<vector<int> > indists, bool include){
	if(include == true){
		dists = indists;
//		if(calculate_vector_int_sum(&dists[0]) > 0){
		if(accumulate(dists[0].begin(),dists[0].end(),0) > 0){
			vector<int> empt;
			for (unsigned int i=0;i<dists[0].size();i++){
				empt.push_back(0);
			}
			dists.push_back(empt);
		}
	}else{//exclude is sent
		vector<int> empt;
		for (int i=0;i<nareas;i++){
			empt.push_back(0);
		}
		dists.push_back(empt);

		map< int, vector<int> > a = iterate_all_bv(nareas);
		map<int, vector<int> >::iterator pos;
		for (pos = a.begin(); pos != a.end(); ++pos){
			int f = pos->first;
			bool inh = false;
			for(unsigned int j=0;j<indists.size();j++){
				//if(is_vector_int_equal_to_vector_int(indists[j],a[f])){
				if(indists[j]==a[f]){
					inh = true;
				}
			}
			if(inh == false)
				dists.push_back(a[f]);
		}
	}
	/*
	 calculate the distribution map
	 */
	for(unsigned int i=0;i<dists.size();i++){
		distsintmap[dists[i]] = i;
	}
	for(unsigned int i=0;i<dists.size();i++){
		intdistsmap[i] = dists[i];
	}
	/*
	precalculate the iterdists
	 */
//	iter_all_dist_splits();
	iter_all_dist_splits_per_period();

	/*
	 print out a visual representation of the matrix
	 */
	if (VERBOSE){
		cout << "dists" <<endl;
		for (unsigned int j=0; j< dists.size(); j++){
			cout << j << " ";
			for (unsigned int i=0;i<dists[j].size();i++){
				cout << dists[j][i];
			}
			cout << " " << print_area_vector(dists[j],areanamemaprev) << endl;
		}
		cout << endl;
	}

	cout << "Total number of considered ranges : " << dists.size() << endl;
	for (unsigned int prd = 0; prd < periods.size(); prd++)
		cout << "\nPeriod : " << prd + 1 << endl
			 << "Number of considered ranges during this period : " << incldists_per_period[prd].size() << endl;
	cout << endl;
}

void RateModel::setup_adjacency(string filename, vector<string> areaNames)
{
	vector<bool> adjMatCol(nareas,true);
	vector<vector<bool> > adjMatRow(nareas,adjMatCol);
	adjMat = vector<vector<vector<bool> > > (periods.size(),adjMatRow);
	default_adjacency = false;

	//read file
	ifstream ifs(filename.c_str());
	string line;
	int period = 0;int fromarea = 0;
	cout << "\nReading adjacency matrix file..." << endl;
	while(getline(ifs,line)) {
		//	CBR (18.10.2013), in case empty lines greater than 3 chars in size have been specified
		TrimSpaces(line);
		if (line.size() > 0) {
			if (fromarea == 0) {
				for (unsigned int j = 0; j < areaNames.size(); j++)
					cout << "\t" << areaNames[j];
				cout << endl << endl;
			}
			vector<string> tokens;
			string del(" ,\t");
			tokens.clear();
			Tokenize(line, tokens, del);
			for (unsigned int j = 0; j < tokens.size(); j++) {
				TrimSpaces(tokens[j]);
			}
			cout << areaNames[fromarea] << "\t";
			for (unsigned int j = 0; j < (fromarea + 1); j++) {
				if (atoi(tokens[j].c_str()) == 0)
					adjMat[period][fromarea][j] = adjMat[period][j][fromarea] = false;
				cout << tokens[j] << "\t";
			}
			cout << endl;
			if (fromarea < nareas - 1) {
				++fromarea;
			}
			else {
				fromarea = 0;
				++period;
				cout << endl;
			}
		}
	}
	ifs.close();
}

void RateModel::set_adj_bool(bool adjBool)
{
	default_adjacency = adjBool;
}

/*
void RateModel::remove_dist(vector<int> dist);*/

/*
 * just give Dmask a bunch of ones
 * specify particular ones in the Dmask_cell
 */
void RateModel::setup_Dmask(){
	vector<double> cols(nareas, 1);
	vector< vector<double> > rows(nareas, cols);
	Dmask = vector< vector< vector<double> > > (periods.size(), rows);
}

void RateModel::set_Dmask_cell(int period, int area, int area2, double prob, bool sym){
	Dmask[period][area][area2] = prob;
	if (sym)
		Dmask[period][area2][area] = prob;
}

void RateModel::setup_D(double d){
	vector<double> cols(nareas, 1*d);
	vector< vector<double> > rows(nareas, cols);
	D = vector< vector< vector<double> > > (periods.size(), rows);
	for (unsigned int i=0;i<D.size();i++){
		for (unsigned int j=0;j<D[i].size();j++){
			D[i][j][j] = 0.0;
			for (unsigned int k=0;k<D[i][j].size();k++){
				D[i][j][k] = D[i][j][k] * Dmask[i][j][k];
			}
		}
	}
	if (VERBOSE){
		cout << "D" <<endl;
		for (unsigned int i=0;i<D.size();i++){
			for (unsigned int j=0;j<D[i].size();j++){
				for (unsigned int k=0;k<D[i][j].size();k++){
					cout << D[i][j][k] << " ";
				}
				cout << endl;
			}
			cout << endl;
		}
	}
}

/*
 * this is for estimating the D matrix
 * in this case, a setup dmatrix is being sent and
 * the dmask is then applied to it
 */

void RateModel::setup_D_provided(double d, vector< vector< vector<double> > > & D_mask_in){
	vector<double> cols(nareas, 1*d);
	vector< vector<double> > rows(nareas, cols);
	D = vector< vector< vector<double> > > (periods.size(), rows);
	for (unsigned int i=0;i<D.size();i++){
		for (unsigned int j=0;j<D[i].size();j++){
			D[i][j][j] = 0.0;
			for (unsigned int k=0;k<D[i][j].size();k++){
				D[i][j][k] = D[i][j][k] * Dmask[i][j][k]*D_mask_in[i][j][k];
			}
		}
	}
	if (VERBOSE){
		cout << "D" <<endl;
		for (unsigned int i=0;i<D.size();i++){
			for (unsigned int j=0;j<D[i].size();j++){
				for (unsigned int k=0;k<D[i][j].size();k++){
					cout << D[i][j][k] << " ";
				}
				cout << endl;
			}
			cout << endl;
		}
	}
}

void RateModel::setup_E(double e){
	vector<double> cols(nareas, 1*e);
	E = vector<vector<double> > (periods.size(), cols);
}

void RateModel::set_Qdiag(int period){
	for (unsigned int i=0;i<dists.size();i++){
		double sum =(calculate_vector_double_sum(Q[period][i]) - Q[period][i][i]) * -1.0;
		Q[period][i][i] = sum;
	}
}

void RateModel::setup_Q(){
	vector<double> cols(dists.size(), 0);
	vector< vector<double> > rows(dists.size(), cols);
	Q = vector< vector< vector<double> > > (periods.size(), rows);
	for(unsigned int p=0; p < Q.size(); p++){//periods
		for(unsigned int i=0;i<dists.size();i++){//dists
			//int s1 = calculate_vector_int_sum(&dists[i]);
			int s1 = accumulate(dists[i].begin(),dists[i].end(),0);
			if(s1 > 0){
				for(unsigned int j=0;j<dists.size();j++){//dists
					int sxor = calculate_vector_int_sum_xor(dists[i], dists[j]);
					if (sxor == 1){
						//int s2 = calculate_vector_int_sum(&dists[j]);
						int s2 = accumulate(dists[j].begin(),dists[j].end(),0);
						int dest = locate_vector_int_single_xor(dists[i],dists[j]);
						double rate = 0.0;
						if (s1 < s2){
							for (unsigned int src=0;src<dists[i].size();src++){
								if(dists[i][src] != 0){
									rate += D[p][src][dest] ;//* Dmask[p][src][dest];
								}
							}
						}else{
							rate = E[p][dest];
						}
						Q[p][i][j] = rate;
					}
				}
			}
		}
		set_Qdiag(p);
	}
	/*
	 * sparse needs to be transposed for matrix exponential calculation
	 */
	if(sparse == true){
		vector<double> cols(dists.size(), 0);
		vector< vector<double> > rows(dists.size(), cols);
		QT = vector< vector< vector<double> > > (periods.size(), rows);
		for(unsigned int p=0; p < QT.size(); p++){//periods
			for(unsigned int i=0;i<dists.size();i++){//dists
				for(unsigned int j=0;j<dists.size();j++){//dists
					QT[p][j][i] = Q[p][i][j];
				}
			}
		}
		//setting up the coo numbs
		nzs = vector<int>(Q.size(),0);
		for(unsigned int p=0; p < Q.size(); p++){//periods
			nzs[p] = get_size_for_coo(Q[p],1);
		}
		//setup matrix
		ia_s.clear();
		ja_s.clear();
		a_s.clear();
		for(unsigned int p=0; p < Q.size(); p++){//periods
			vector<int> ia = vector<int>(nzs[p]);
			vector<int> ja = vector<int>(nzs[p]);
			vector<double> a = vector<double>(nzs[p]);
			convert_matrix_to_coo_for_fortran_vector(QT[p],ia,ja,a);//need to multiply these all these by t
			ia_s.push_back(ia);
			ja_s.push_back(ja);
			a_s.push_back(a);
		}
	}
	if(VERBOSE){
	cout << "Q" <<endl;
		for (unsigned int i=0;i<Q.size();i++){
			for (unsigned int j=0;j<Q[i].size();j++){
				for (unsigned int k=0;k<Q[i][j].size();k++){
					cout << Q[i][j][k] << " ";
				}
				cout << endl;
			}
			cout << endl;
		}
	}
}

void RateModel::set_Qdiag_with_adjacency(int period){
	for (unsigned int i=0;i<incldists_per_period[period].size();i++){
		double sum =(calculate_vector_double_sum(Q[period][i]) - Q[period][i][i]) * -1.0;
		Q[period][i][i] = sum;
	}
}

void RateModel::setup_Q_with_adjacency(){
	for(unsigned int p=0; p < periods.size(); p++){//periods
		vector<double> cols(incldists_per_period[p].size(), 0);
		vector< vector<double> > rows(incldists_per_period[p].size(), cols);
		Q.push_back(rows);
		for(unsigned int i=0;i<incldists_per_period[p].size();i++){//incldists_per_period[p]
			int s1 = accumulate(incldists_per_period[p][i].begin(),incldists_per_period[p][i].end(),0);
			if ((s1 > 0) && s1 <= maxareas){
				for(unsigned int j=0;j<incldists_per_period[p].size();j++){//incldists_per_period[p]
					int sxor = calculate_vector_int_sum_xor(incldists_per_period[p][i], incldists_per_period[p][j]);
					if (sxor == 1){
						int s2 = accumulate(incldists_per_period[p][j].begin(),incldists_per_period[p][j].end(),0);
						int dest = locate_vector_int_single_xor(incldists_per_period[p][i],incldists_per_period[p][j]);
						double rate = 0.0;
						if (s1 < s2){
							for (unsigned int src=0;src<incldists_per_period[p][i].size();src++){
								if(incldists_per_period[p][i][src] != 0){
									rate += D[p][src][dest] ;//* Dmask[p][src][dest];
								}
							}
						}else{
							rate = E[p][dest];
						}
						Q[p][i][j] = rate;
					}
				}
			}
			//	special case of "big tip" distributions which are added only during the most recent time period
			else if ((p == 0) && (s1 > maxareas)) {
				int tip_anc_size = 1;
				//	looping through all the "left" dist splits of this big tip distribution
				for (size_t k = 0; k < iter_dists_per_period[incldists_per_period[p][i]][p][0].size();k++) {
					int split_dist_size = calculate_vector_int_sum(&iter_dists_per_period[incldists_per_period[p][i]][p][0][k]);
					//	searching for the smallest possible contraction of the range size of this big tip
					if ((split_dist_size > tip_anc_size) && (split_dist_size < s1))
						tip_anc_size = split_dist_size;
				}

				// 	search for tip_anc_size'd contiguous sub-ranges/splits for the big tip
				map<int,int> combidx2distidxmap;
				int counter = 0;
				for (unsigned int area = 0; area < nareas; area++) {
					if (incldists_per_period[p][i][area] == 1) {
						combidx2distidxmap[counter] = area;
						++counter;
					}
				}

				// 	generate tip_anc_size'd splits
				vector<vector<int> > range_comb_idxs = iterate(s1, tip_anc_size);

				for (unsigned int k = 0; k < range_comb_idxs.size(); k++) {
					vector<int> split_dist = comb_idx2bit_vect(range_comb_idxs[k], nareas, combidx2distidxmap);
					//	keep only contiguous splits
					if (count(incldists_per_period[p].begin(),incldists_per_period[p].end(), split_dist) > 0) {
						int split_dist_index = distance(incldists_per_period[p].begin(),find(incldists_per_period[p].begin(),incldists_per_period[p].end(),split_dist));
						vector<int> xor_dist = calculate_vector_int_xor_vector(incldists_per_period[p][i],split_dist);
						//	consider dispersal from these splits towards the big tip
						double rate = 0.0;
						for (size_t dest = 0; dest < xor_dist.size(); dest++)
							if (xor_dist[dest] == 1)
								for (size_t src = 0; src < split_dist.size(); src++)
									if(split_dist[src] != 0)
										rate += D[p][src][dest];

						Q[p][split_dist_index][i] = rate;
					}
				}
			}
		}
		set_Qdiag_with_adjacency(p);
	}
	/*
	 * sparse matrices will be handled later
	 */
	if(sparse == true) {
		cerr << "Sparse Q matrices are not yet supported with this version of Lagrange"
				"\nPlease contact Champak Reddy (champak.br@gmail.com) if this functionality is needed" << endl;
		exit(-1);
	}
	if(VERBOSE){
		cout << "Q" <<endl;
		for (unsigned int i=0;i<Q.size();i++){
			for (unsigned int j=0;j<Q[i].size();j++){
				for (unsigned int k=0;k<Q[i][j].size();k++){
					cout << Q[i][j][k] << " ";
				}
				cout << endl;
			}
			cout << endl;
		}
	}
}



extern"C" {
	void wrapalldmexpv_(int * n,int* m,double * t,double* v,double * w,double* tol,
		double* anorm,double* wsp,int * lwsp,int* iwsp,int *liwsp, int * itrace,int *iflag,
		int *ia, int *ja, double *a, int *nz, double * res);
	void wrapsingledmexpv_(int * n,int* m,double * t,double* v,double * w,double* tol,
			double* anorm,double* wsp,int * lwsp,int* iwsp,int *liwsp, int * itrace,int *iflag,
			int *ia, int *ja, double *a, int *nz, double * res);
	void wrapdgpadm_(int * ideg,int * m,double * t,double * H,int * ldh,
			double * wsp,int * lwsp,int * ipiv,int * iexph,int *ns,int *iflag );
}

/*
 * runs the basic padm fortran expokit full matrix exp
 */
vector<vector<double > > RateModel::setup_fortran_P(int period, double t, bool store_p_matrices){
	/*
	return P, the matrix of dist-to-dist transition probabilities,
	from the model's rate matrix (Q) over a time duration (t)
	*/
	int ideg = 6;
	int m = Q[period].size();
	int ldh = m;
	double tol = 1;
	int iflag = 0;
	int lwsp = 4*m*m+6+1;
	double * wsp = new double[lwsp];
	int * ipiv = new int[m];
	int iexph = 0;
	int ns = 0;
	double * H = new double [m*m];
	convert_matrix_to_single_row_for_fortran(Q[period],t,H);
	wrapdgpadm_(&ideg,&m,&tol,H,&ldh,wsp,&lwsp,ipiv,&iexph,&ns,&iflag);
	vector<vector<double> > p (Q[period].size(), vector<double>(Q[period].size()));
	for(int i=0;i<m;i++){
		for(int j=0;j<m;j++){
			p[i][j] = wsp[iexph+(j-1)*m+(i-1)+m];
		}
	}
	delete [] wsp;
	delete [] ipiv;
	delete [] H;
	for(unsigned int i=0; i<p.size(); i++){
		double sum = 0.0;
		for (unsigned int j=0; j<p[i].size(); j++){
			sum += p[i][j];
		}
		for (unsigned int j=0; j<p[i].size(); j++){
			p[i][j] = (p[i][j]/sum);
		}
	}


	//filter out impossible dists
	//vector<vector<int> > dis = enumerate_dists();
	/*
	for (unsigned int i=0;i<dists.size();i++){
		//if (calculate_vector_int_sum(&dists[i]) > 0){
		if(accumulate(dists[i].begin(),dists[i].end(),0) > 0){
			for(unsigned int j=0;j<dists[i].size();j++){
				if(dists[i][j]==1){//present
					double sum1 =calculate_vector_double_sum(Dmask[period][j]);
					double sum2 = 0.0;
					for(unsigned int k=0;k<Dmask[period].size();k++){
						sum2 += Dmask[period][k][j];
					}
					if(sum1+sum2 == 0){
						for(unsigned int k=0;k<p[period].size();k++){
							p[period][k] = p[period][k]*0.0;
						}
						break;
					}
				}
			}
		}
	}*/

	/*
	 if store_p_matrices we will store them
	 */
	if(store_p_matrices == true){
		stored_p_matrices[period][t] = p;
	}

	if(VERBOSE){
	cout << "p " << period << " "<< t << endl;
		for (unsigned int i=0;i<p.size();i++){
			for (unsigned int j=0;j<p[i].size();j++){
				cout << p[i][j] << " ";
			}
			cout << endl;
		}
	}
	return p;
}

/*
 * runs the sparse matrix fortran expokit matrix exp
 */
vector<vector<double > > RateModel::setup_sparse_full_P(int period, double t){
	int n = Q[period].size();
	int m = Q[period].size()-1;//tweak
	int nz = get_size_for_coo(Q[period],1);
	int * ia = new int [nz];
	int * ja = new int [nz];
	double * a = new double [nz];
	convert_matrix_to_coo_for_fortran(Q[period],t,ia,ja,a);
	double * v = new double [n];
	for(int i=0;i<n;i++){
		v[i] = 0;
	}v[0]= 1;
	double * w = new double [n];
	int ideg = 6;
	double tol = 1;
	int iflag = 0;
	int lwsp = n*(m+1)+n+pow((m+2.),2)+4*pow((m+2.),2)+ideg+1;
	double * wsp = new double[lwsp];
	int liwsp = m+2;
	int * iwsp = new int [liwsp];
	double t1 = 1;
	double anorm = 0;
	int itrace = 0;
	double * res = new double [n*n];
	wrapalldmexpv_(&n,&m,&t1,v,w,&tol,&anorm,wsp,&lwsp,iwsp,&liwsp,
			&itrace,&iflag,ia,ja,a,&nz,res);

	vector<vector<double> > p (Q[period].size(), vector<double>(Q[period].size()));
	int count = 0;
	for(int i=0;i<n;i++){
		for(int j=0;j<n;j++){
			p[j][i] = res[count];
			count += 1;
		}
	}

	//filter out impossible dists
	//vector<vector<int> > dis = enumerate_dists();
	for (unsigned int i=0;i<dists.size();i++){
		//if (calculate_vector_int_sum(&dists[i]) > 0){
		if(accumulate(dists[i].begin(),dists[i].end(),0) > 0){
			for(unsigned int j=0;j<dists[i].size();j++){
				if(dists[i][j]==1){//present
					double sum1 =calculate_vector_double_sum(Dmask[period][j]);
					double sum2 = 0.0;
					for(unsigned int k=0;k<Dmask[period].size();k++){
						sum2 += Dmask[period][k][j];
					}
					if(sum1+sum2 == 0){
						for(unsigned int k=0;k<p[period].size();k++){
							p[period][k] = p[period][k]*0.0;
						}
						break;
					}
				}
			}
		}
	}
	delete []res;
	delete []iwsp;
	delete []w;
	delete []wsp;
	delete []a;
	delete []ia;
	delete []ja;
	if(VERBOSE){
		cout << "p " << period << " "<< t << endl;
		for (unsigned int i=0;i<p.size();i++){
			for (unsigned int j=0;j<p[i].size();j++){
				cout << p[i][j] << " ";
			}
			cout << endl;
		}
	}
	return p;
}

/*
 * for returning single P columns
 */
vector<double >  RateModel::setup_sparse_single_column_P(int period, double t, int column){
	int n = Q[period].size();
	int m = nareas-1;
	int nz = nzs[period];//get_size_for_coo(Q[period],1);
	int * ia = new int [nz];
	int * ja = new int [nz];
	double * a = new double [nz];
	//convert_matrix_to_coo_for_fortran(QT[period],1,ia,ja,a);
	std::copy(ia_s[period].begin(), ia_s[period].end(), ia);
	std::copy(ja_s[period].begin(), ja_s[period].end(), ja);
	std::copy(a_s[period].begin(), a_s[period].end(), a);
	double * v = new double [n];
	for(int i=0;i<n;i++){
		v[i] = 0;
	}v[column]= 1; // only return the one column we want
	double * w = new double [n];
	int ideg = 6;
	double tol = 1;
	int iflag = 0;
	int lwsp = n*(m+1)+n+pow((m+2.),2)+4*pow((m+2.),2)+ideg+1;
	double * wsp = new double[lwsp];
	int liwsp = m+2;
	int * iwsp = new int [liwsp];
	double t1 = t;//use to be 1
	double anorm = 0;
	int itrace = 0;
	double * res = new double [n]; // only needs resulting columns
	wrapsingledmexpv_(&n,&m,&t1,v,w,&tol,&anorm,wsp,&lwsp,iwsp,&liwsp,
			&itrace,&iflag,ia,ja,a,&nz,res);

	vector<double> p (Q[period].size());
	int count = 0;
	for(int i=0;i<n;i++){
		p[i] = res[count];
		count += 1;
	}
	delete []v;
	delete []res;
	delete []iwsp;
	delete []w;
	delete []wsp;
	delete []a;
	delete []ia;
	delete []ja;
	if(VERBOSE){
		cout << "p " << period << " "<< t << " " << column <<  endl;
		for (unsigned int i=0;i<p.size();i++){
			cout << p[i] << " ";
		}
		cout << endl;
	}
	return p;
}

/*
	for returning all columns for pthread fortran sparse matrix calculation

	NOT GOING TO BE FASTER UNTIL THE FORTRAN CODE GOES TO C
 */
vector<vector<double > > RateModel::setup_pthread_sparse_P(int period, double t, vector<int> & columns){
	struct sparse_thread_data thread_data_array[numthreads];
	for(int i=0;i<numthreads;i++){
		vector <int> st_cols;
		if((i+1) < numthreads){
			for(unsigned int j=(i*(columns.size()/numthreads));j<((columns.size()/numthreads))*(i+1);j++){
				st_cols.push_back(columns[j]);
			}
		}else{//last one
			for(unsigned int j=(i*(columns.size()/numthreads));j<columns.size();j++){
				st_cols.push_back(columns[j]);
			}
		}
//		cout << "spliting: " << st_cols.size() << endl;
		thread_data_array[i].thread_id = i;
		thread_data_array[i].columns = st_cols;
		vector<vector<double> > presults;
		thread_data_array[i].presults = presults;
		thread_data_array[i].t = t;
		thread_data_array[i].period = period;
		thread_data_array[i].rm = this;
	}
	pthread_t threads[numthreads];
	void *status;
	int rc;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for(int i=0; i <numthreads; i++){
//		cout << "thread: " << i <<endl;
		rc = pthread_create(&threads[i], &attr, sparse_column_pmatrix_pthread_go, (void *) &thread_data_array[i]);
		if (rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	pthread_attr_destroy(&attr);
	for(int i=0;i<numthreads; i++){
//		cout << "joining: " << i << endl;
		pthread_join( threads[i], &status);
		if (rc){
			printf("ERROR; return code from pthread_join() is %d\n", rc);
			exit(-1);
		}
//		printf("Completed join with thread %d status= %ld\n",i, (long)status);
	}
	/*
		bring em back and combine for keep_seqs and keep_rc
	 */
	vector<vector<double> > preturn (Q[period].size(), vector<double>(Q[period].size()));
	for (int i=0;i<numthreads; i++){
		for(unsigned int j=0;j<thread_data_array[i].columns.size();j++){
			preturn[thread_data_array[i].columns[j]] = thread_data_array[i].presults[j];
		}
	}
	for(unsigned int i=0;i<Q[period].size();i++){
		if(count(columns.begin(),columns.end(),i) == 0){
			preturn[i] = vector<double>(Q[period].size(),0);
		}
	}
	return preturn;
}


vector<vector<vector<int> > > RateModel::iter_dist_splits(vector<int> & dist){
	vector< vector <vector<int> > > ret;
	vector< vector<int> > left;
	vector< vector<int> > right;
	if(accumulate(dist.begin(),dist.end(),0) == 1){
		left.push_back(dist);
		right.push_back(dist);
	}
	else{
		for(unsigned int i=0;i<dist.size();i++){
			if (dist[i]==1){
				vector<int> x(dist.size(),0);
				x[i] = 1;
				int cou = count(dists.begin(),dists.end(),x);
				if(cou > 0){
					left.push_back(x);right.push_back(dist);
					left.push_back(dist);right.push_back(x);
					vector<int> y;
					for(unsigned int j=0;j<dist.size();j++){
						if(dist[j]==x[j]){
							y.push_back(0);
						}else{
							y.push_back(1);
						}
					}
					int cou2 = count(dists.begin(),dists.end(),y);
					if(cou2 > 0){
						left.push_back(x);right.push_back(y);
						if(accumulate(y.begin(),y.end(),0) > 1){
							left.push_back(y);right.push_back(x);
						}
					}
				}
			}
		}
	}
	if(VERBOSE){
		cout << "LEFT" << endl;
		for(unsigned int i = 0; i< left.size() ; i++ ){
			print_vector_int(left[i]);
		}
		cout << "RIGHT" << endl;
		for(unsigned int i = 0; i< right.size() ; i++ ){
			print_vector_int(right[i]);
		}
	}
	ret.push_back(left);
	ret.push_back(right);
	return ret;
}

void RateModel::iter_all_dist_splits(){
	for(unsigned int i=0;i<dists.size();i++){
		iter_dists[dists[i]] = iter_dist_splits(dists[i]);
	}
}


map<int,vector<vector<vector<int> > > > RateModel::iter_dist_splits_per_period(vector<int> & dist, int distSize){
	map<int, vector<vector<vector<int> > > > ret;

	for (unsigned int per = 0; per < periods.size(); per++) {
		vector<vector<int> > left;
		vector<vector<int> > right;
		//	if this dist is connected during the specified time period
//		if ((distSize <= maxareas) && (count(incldists_per_period[per].begin(),incldists_per_period[per].end(), dist) > 0)) {
		if (count(incldists_per_period[per].begin(),incldists_per_period[per].end(), dist) > 0) {
			if (distSize == 1) {
				left.push_back(dist);
				right.push_back(dist);
			} else {
				for (unsigned int i = 0; i < nareas; i++) {
					if (dist[i] == 1) {
						vector<int> x(dist.size(), 0);
						x[i] = 1;
						int cou = count(incldists_per_period[per].begin(),incldists_per_period[per].end(), x);
						if (cou > 0) {
							left.push_back(x);
							right.push_back(dist);
							left.push_back(dist);
							right.push_back(x);
							vector<int> y;
							for (unsigned int j = 0; j < nareas; j++) {
								if (dist[j] == x[j]) {
									y.push_back(0);
								} else {
									y.push_back(1);
								}
							}
							int cou2 = count(incldists_per_period[per].begin(),incldists_per_period[per].end(), y);
							if (cou2 > 0) {
								left.push_back(x);
								right.push_back(y);
								if (accumulate(y.begin(), y.end(), 0) > 1) {
									left.push_back(y);
									right.push_back(x);
								}
							}
						}
					}
				}
				//	allows for DIVA style splits for ranges of size 4 and above
				//	ONLY if they remain connected during the respective time period
				if (distSize >= 4) {
					map<int,int> combidx2distidxmap;
					int counter = 0;
					for (unsigned int i = 0; i < nareas; i++) {
						if (dist[i] == 1) {
							combidx2distidxmap[counter] = i;
							++counter;
						}
					}

					//	keeps track of the split range sizes that have been considered for this dist
					vector<bool> split_comb_sizes(distSize + 1, true);
					split_comb_sizes[0] = split_comb_sizes[1] = split_comb_sizes[distSize - 1] = split_comb_sizes[distSize] = false;
					//	start with a left split of size 2
					for (unsigned int i = 2; split_comb_sizes[i] != false; i++) {
						vector<vector<int> > range_comb_idxs = iterate(distSize, i);
						for (unsigned int j = 0; j < range_comb_idxs.size(); j++) {
							vector<int> split1 = comb_idx2bit_vect(range_comb_idxs[j], nareas, combidx2distidxmap);
							vector<int> split2 = calculate_vector_int_xor_vector(dist, split1);
							if ((count(incldists_per_period[per].begin(),incldists_per_period[per].end(), split1) > 0)
									&& (count(incldists_per_period[per].begin(),incldists_per_period[per].end(), split2) > 0)
									&& (find(left.begin(),left.end(),split1) == left.end())) {
									left.push_back(split1); right.push_back(split2);
									left.push_back(split2); right.push_back(split1);
							}
						}
						split_comb_sizes[i] = split_comb_sizes[distSize - i] = false;
					}
				}
			}
		}
		ret[per].push_back(left);
		ret[per].push_back(right);
	}
	return ret;
}

void RateModel::iter_all_dist_splits_per_period() {
	for (unsigned int i = 0; i < dists.size(); i++) {
		int distSize = accumulate(dists[i].begin(),dists[i].end(),0);
		iter_dists_per_period[dists[i]] = iter_dist_splits_per_period(dists[i],distSize);
	}
}


vector< vector<int> > RateModel::generate_adjacent_dists(int maxareas, map<int,string> areanamemaprev)
{
	RateModel::maxareas = maxareas;
	RateModel::areanamemaprev = areanamemaprev;
	vector< vector<int> > it = iterate_all_from_num_max_areas(nareas, maxareas);
	//global extinction
	vector<int> empt(nareas,0);

	vector< vector<int> > rangemap;
	vector<int> alldistsint;
	rangemap.push_back(empt);
	alldistsint.push_back(0);
	for (unsigned int i = 0; i < it.size(); i++) {
		rangemap.push_back(idx2bitvect(it.at(i),nareas));
		alldistsint.push_back(i+1);
	}

	if (!default_adjacency) {
		vector<vector<bool> > defAdjMat(nareas, vector<bool> (nareas,true));
		for (unsigned int prd = 0; prd < periods.size(); prd++) {

#ifdef CBR
			int adjDistCounter = 0;
			cout << "\nPeriod : " << prd + 1 << endl;
#endif

			if (adjMat[prd] == defAdjMat) {
				incldists_per_period.push_back(rangemap);
				incldistsint_per_period.push_back(alldistsint);
				excldists_per_period.push_back(vector<vector<int> > ());
#ifdef CBR
				cout << "Total dists (default adjacency) : " << alldistsint.size() << endl;
#endif
			}
			else {
				vector<vector<int> > period_exdists;
				vector<vector<int> > period_incdists;
				vector<int> somedistsint;
				period_incdists.push_back(rangemap[0]);
				somedistsint.push_back(0);
				for (unsigned int i = 0; i < it.size(); i++) {

					if (connected_dist_BGL(it.at(i), adjMat[prd])) {
#ifdef CBR
						cout << i + 1 << " ";
//						cout << idx2bitvect(it.at(i),nareas);
						for (unsigned int x=0;x<it.at(i).size();x++){
							cout << areanamemaprev[it.at(i)[x]];
							if (x < (it.at(i).size() - 1))
								cout << "_";
						}
						cout << endl;
						++adjDistCounter;
#endif
						period_incdists.push_back(idx2bitvect(it.at(i),nareas));
						somedistsint.push_back(i+1);
					}
					else
						period_exdists.push_back(idx2bitvect(it.at(i),nareas));
				}
				incldists_per_period.push_back(period_incdists);
				incldistsint_per_period.push_back(somedistsint);
				excldists_per_period.push_back(period_exdists);
#ifdef CBR
				cout << "Total dists : " << adjDistCounter << endl;
#endif
			}
		}
	}
	else {
		excldists_per_period = vector<vector<vector<int> > > ();
		for (unsigned int prd = 0; prd < periods.size(); prd++) {
			incldists_per_period.push_back(rangemap);
			incldistsint_per_period.push_back(alldistsint);
		}
	}

	return rangemap;
}


vector< vector<int> >  RateModel::iterate_all_from_num_max_areas(int m, int n){
	vector< vector<int> > results;
	for (int areaSize = 1; areaSize <= n; areaSize++){
		vector< vector<int> > it = iterate(m, areaSize);
		for (unsigned int i = 0; i < it.size(); i++)
			results.push_back(it[i]);
	}
	return results;
}


void RateModel::include_tip_dists(map<string,vector<int> > distrib_data, vector<vector<int> > &includedists, map<int,string> areanamemaprev)
{
	map<string, vector<int> >::iterator pos;
	bool bigTipMsg = false, adjacentTipMsg = false;

	//	including the adjacency-conflicting tip distributions (only) for the most recent period
	if (!default_adjacency) {
		for (pos = distrib_data.begin(); pos != distrib_data.end(); ++pos) {
			string taxon = pos->first;
			int taxon_numareas = accumulate(distrib_data[taxon].begin(),distrib_data[taxon].end(),0);
			if ((taxon_numareas > 1) && (taxon_numareas <= maxareas)) {
				bool tipIncluded = false;
				vector<vector<int> >::iterator it = find(excldists_per_period[0].begin(),excldists_per_period[0].end(),distrib_data[taxon]);
				if (it != excldists_per_period[0].end()) {
					if (!adjacentTipMsg) {
						cout << "\nIncluding those tips whose range conflicts with the specified adjacency matrix..." << endl;
						adjacentTipMsg = true;
					}
					incldists_per_period[0].push_back(*it);
					incldistsint_per_period[0].push_back(distance(includedists.begin(),find(includedists.begin(),includedists.end(),distrib_data[taxon])));
					excldists_per_period[0].erase(it);
					cout << "For an example of the missing taxon distribution cf. " << taxon
						 << " (" << print_area_vector(distrib_data[taxon],areanamemaprev) << ")" << endl;
				}
			}
		}
	}

	//	"big" tip distributions are included here and excluded for all periods except the most recent one
	for (pos = distrib_data.begin(); pos != distrib_data.end(); ++pos){
		string taxon = pos->first;
		int taxon_numareas = accumulate(distrib_data[taxon].begin(),distrib_data[taxon].end(),0);
		if (taxon_numareas > maxareas) {
			if (!bigTipMsg)
				cout << "\nIncluding those tips whose range size is bigger than maxareas(= " << maxareas << ")..." << endl;

			includedists.push_back(distrib_data[taxon]);
			incldists_per_period[0].push_back(distrib_data[taxon]);
			incldistsint_per_period[0].push_back(includedists.size()-1);
			if (!default_adjacency)
				for (int i = 1; i < periods.size(); i++)
					excldists_per_period[i].push_back(distrib_data[taxon]);

			cout << taxon << " : " << taxon_numareas << " (" << print_area_vector(distrib_data[taxon],areanamemaprev) << ")" << endl;
			bigTipMsg = true;
		}
	}
	if (adjacentTipMsg || bigTipMsg)
		cout << endl;
}


vector<vector<int> > * RateModel::getDists(){
	return &dists;
}

map<vector<int>,int> * RateModel::get_dists_int_map(){
	return &distsintmap;
}

map<int,vector<int> > * RateModel::get_int_dists_map(){
	return &intdistsmap;
}

vector<vector<vector<int> > > * RateModel::get_iter_dist_splits(vector<int> & dist){
	return &iter_dists[dist];
}

vector<vector<vector<int> > > * RateModel::get_iter_dist_splits_per_period(vector<int> & dist, int period){
	return &iter_dists_per_period[dist][period];
}

vector<vector<int> > * RateModel::get_incldists_per_period(int period)
{
	return &incldists_per_period[period];
}

vector<int> * RateModel::get_incldistsint_per_period(int period)
{
	return &incldistsint_per_period[period];
}

vector<vector<int> > * RateModel::get_excldists_per_period(int period)
{
	return &excldists_per_period[period];
}

map<int,string> * RateModel::get_areanamemaprev()
{
	return &areanamemaprev;
}

int RateModel::get_num_areas(){return nareas;}

int RateModel::get_num_periods(){return periods.size();}

vector< vector< vector<double> > > & RateModel::get_Q(){
	return Q;
}

inline int signof(double d)
{
   return d >= 0 ? 1 : -1;
}

inline double roundto(double in){
	return floor(in*(1000)+0.5)/(1000);
}

/*
 * this should be used to caluculate the eigenvalues and eigenvectors
 * as U * Q * U-1 -- eigen decomposition
 *
 * this should use the armadillo library
 */
bool RateModel::get_eigenvec_eigenval_from_Q(cx_mat * eigval, cx_mat * eigvec, int period){
	mat tQ(int(Q[period].size()),int(Q[period].size())); tQ.fill(0);
	for(unsigned int i=0;i<Q[period].size();i++){
		for(unsigned int j=0;j<Q[period].size();j++){
			tQ(i,j) = Q[period][i][j];
			//cout << Q[0][i][j] << " ";
		}
		//cout << endl;
	}
	//cout << endl;
	cx_colvec eigva;
	cx_mat eigve;
	eig_gen(eigva,eigve,tQ);
	bool isImag = false;
	for(unsigned int i=0;i<Q[period].size();i++){
		for(unsigned int j=0;j<Q[period].size();j++){
			if(i==j)
				(*eigval)(i,j) = eigva(i);
			else
				(*eigval)(i,j) = 0;
			(*eigvec)(i,j) = eigve(i,j);
			if(imag((*eigvec)(i,j)) > 0 || imag((*eigval)(i,j)))
				isImag = true;
		}
	}
	//cout << eigva << endl;
	//cout << tQ - ((*eigvec) * (*eigval) * inv(*eigvec)) <<endl;
	return isImag;
}

//trying not to use octave at the moment
/*
 * 
 * bool RateModel::get_eigenvec_eigenval_from_Q_octave(ComplexMatrix * eigval, ComplexMatrix * eigvec, int period){
	ComplexMatrix tQ = ComplexMatrix(int(Q[period].size()),int(Q[period].size()));
	for(unsigned int i=0;i<Q[period].size();i++){
		for(unsigned int j=0;j<Q[period].size();j++){
			tQ(i,j) = Q[period][i][j];
	//		cout << Q[0][i][j] << " ";
		}
	//	cout << endl;
	}
	//cout << endl;
	EIG eig = EIG(tQ);
	bool isImag = false;
	for(unsigned int i=0;i<Q[period].size();i++){
		for(unsigned int j=0;j<Q[period].size();j++){
			if(i==j){
				(*eigval)(i,j) = eig.eigenvalues()(i);
			}else{
				(*eigval)(i,j) = 0;
			}
			(*eigvec)(i,j) = eig.eigenvectors()(i,j);
			if(imag((*eigvec)(i,j)) > 0 || imag((*eigval)(i,j)))
				isImag = true;
		}
	}
	return isImag;
	//cout <<(eig.eigenvalues() * eig.eigenvectors()) << endl;
}*/

/**/


//REQUIRES BOOST AND IS SLOWER BUT TO ACTIVATE UNCOMMENT
/*vector<vector<double > > RateModel::setup_P(int period, double t){
	//
	//return P, the matrix of dist-to-dist transition probabilities,
	//from the model's rate matrix (Q) over a time duration (t)
	//
	vector<vector<double> > p = QMatrixToPmatrix(Q[period], t);

	//filter out impossible dists
	//vector<vector<int> > dis = enumerate_dists();
	for (unsigned int i=0;i<dists.size();i++){
		//if (calculate_vector_int_sum(&dists[i]) > 0){
		if(accumulate(dists[i].begin(),dists[i].end(),0) > 0){
			for(unsigned int j=0;j<dists[i].size();j++){
				if(dists[i][j]==1){//present
					double sum1 =calculate_vector_double_sum(Dmask[period][j]);
					double sum2 = 0.0;
					for(unsigned int k=0;k<Dmask[period].size();k++){
						sum2 += Dmask[period][k][j];
					}
					if(sum1+sum2 == 0){
						for(unsigned int k=0;k<p[period].size();k++){
							p[period][k] = p[period][k]*0.0;
						}
						break;
					}
				}
			}
		}
	}
	if(VERBOSE){
		cout << "p " << period << " "<< t << endl;
		for (unsigned int i=0;i<p.size();i++){
			for (unsigned int j=0;j<p[i].size();j++){
				cout << p[i][j] << " ";
			}
			cout << endl;
		}
	}
	return p;
}*/



