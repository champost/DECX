/*
 * Utils.h
 *
 *  Created on: Mar 10, 2009
 *      Author: smitty
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <vector>
#include <string>
#include <map>

using namespace std;
void Tokenize(const string& str, vector<string>& tokens,const string& delimiters = " ");
void TrimSpaces( string& str);
long comb(int m, int n);
vector< vector<int> > iterate(int M, int N);
vector<int> comb_at_index(int m, int n, int i);
vector< vector<int> > dists_by_maxsize(int nareas, int maxsize);
vector<int> idx2bitvect(vector <int> indices, int M);
vector< vector<int> >  iterate_all(int m);
map< int, vector<int> > iterate_all_bv(int m);
map< int, vector<int> > iterate_all_bv2(int m);
vector< vector<int> >  iterate_all_from_num_max_areas(int m, int n);
bool connected_dist_BGL(const vector <int> &indices, const vector <vector<bool> > &adjMat);

#endif /* UTILS_H_ */
