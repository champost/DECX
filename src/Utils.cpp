/*
 * Utils.cpp
 *
 *  Created on: Mar 10, 2009
 *      Author: Stephen A. Smith
 */

#include <iostream>
#include <vector>
#include <map>
#include <string>

using namespace std;

#include "Utils.h"

void Tokenize(const string& str, vector<string>& tokens,
                      const string& delimiters){
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

void TrimSpaces( string& str)  {
	// Trim Both leading and trailing spaces
	size_t startpos = str.find_first_not_of(" \t\r\n"); // Find the first character position after excluding leading blank spaces
	size_t endpos = str.find_last_not_of(" \t\r\n"); // Find the first character position from reverse af

	// if all spaces or empty return an empty string
	if(( string::npos == startpos ) || ( string::npos == endpos))
	{
		str = "";
	}
	else
		str = str.substr( startpos, endpos-startpos+1 );

	/*
		    // Code for  Trim Leading Spaces only
		    size_t startpos = str.find_first_not_of(� \t�); // Find the first character position after excluding leading blank spaces
		    if( string::npos != startpos )
		        str = str.substr( startpos );
	 */

	/*
		    // Code for Trim trailing Spaces only
		    size_t endpos = str.find_last_not_of(� \t�); // Find the first character position from reverse af
		    if( string::npos != endpos )
		        str = str.substr( 0, endpos+1 );
	 */
}


long comb(int m, int n){
	/*m, n -> number of combinations of m items, n at a time.

    m >= n >= 0 required.
	 */

	if (! m >= n && n >= 0){
		//raise ValueError("m >= n >= 0 required: " + `m, n`)
	}
	if (n > (m >> 1)){
		n = m-n;
	}
	if (n == 0){
		return 1;
	}
	long result = long(m);
	int i = 2;
	m = m-1; n = n-1;
	while (n!=0){
		//# assert (result * m) % i == 0
		result = result * m / i;
		i = i+1;
		n = n-1;
		m = m-1;
	}
	return result;
}


vector<int> comb_at_index(int m, int n, int i){
	if(!(m >= n)&&!(n >= 1))
		cout << "m >= n >= 1 required" << endl;
	long c = comb(m,n);
	if(!(0 <= i)&&!(i < c))
		cout << "0 <= i < comb(m,n) required" << endl;
	vector <int> result;
	c = c * n / m;
	for(int j=0;j<=m;j++){
		if (i < c){
			result.push_back(j);
			n = n-1;
			if (n == 0)
				break;
			c = c * n / (m-1);
		}
		else{
			i = i-c;
			c = c * (m-n) / (m-1);
		}
		m = m-1;
	}
	//assert i == 0
	return result;
}

vector< vector<int> > iterate(int M, int N){
	if(! M >= N and N >= 1){
		//raise ValueError("m >= n >= 1 required: " + `M, N`)
	}
	long ncombs = long(comb(M, N));
	vector< vector<int> > results;
	for (int x = 0 ; x < ncombs ; x++){
		int i = x; int n = N; int m = M;
		double c = ncombs * n / m;
		vector<int> result;
		for (int element = 0; element < M; element++){
			//cout << element << endl;
			if (i < c){
				//take this element, and n-1 from the remaining
				result.push_back(element);
				n = n-1;
				if (n == 0){
					break;
				}
				//have c == comb(m-1,n), want comb(m-2,n-1)
				c = c * n / (m-1);
			}else{
				//skip this element, and take all from the remaining
				i = i-c;
				//have c == comb(m-1,n-1), want comb(m-2,n-1)
				c = c * (m-n) / (m-1);
			}
			m = m-1;
		}
		//assert i == 0;
		results.push_back(result);
	}
	return results;
}



vector< vector<int> > dists_by_maxsize(int nareas, int maxsize){
	vector<int> v(nareas,0);
	vector < vector<int> > results;
	for (int i=0; i< nareas; i++){
		vector<int> result(v);
		result[i] = 1;
		results.push_back(result);
	}
	int n = 2;
	while (n <= maxsize){
		vector< vector<int> > indices = iterate(nareas,n);
		for (unsigned int i=0;i<indices.size();i++){
			vector<int> result(v);
			for (unsigned int j =0 ; j < indices[i].size();j++){
				result[indices[i][j]] = 1;
			}
			results.push_back(result);
		}
		n += 1;
	}
	return results;
}


vector<int> idx2bitvect(vector <int> indices, int M){
	vector <int> v (M,0);
	for (unsigned int i =0; i< indices.size(); i++){
		v[indices[i]] = 1;
	}
	return v;
}

vector< vector<int> >  iterate_all(int m){
	vector< vector<int> > results;
	for (int n = 1; n < m+1; n++){
		vector< vector<int> > it = iterate(m, n);
		for (unsigned int i = 0; i < it.size(); i++)
			results.push_back(it[i]);
	}
	return results;
}

map< int, vector<int> > iterate_all_bv(int m){
	vector< vector<int> > it = iterate_all(m);
	map<int,vector<int> > rangemap;
	for (unsigned int i=0;i<it.size();i++){
		rangemap[i] =  idx2bitvect(it.at(i),m);
	}
	return rangemap;
}

map< int, vector<int> > iterate_all_bv2(int m){
	vector< vector<int> > it = iterate_all(m);
	map<int,vector<int> > rangemap;
	vector<int> zeros;
	for (int i=0;i<m;i++){zeros.push_back(0);}
	rangemap[0] = zeros;
	for (unsigned int i=0;i<it.size();i++){
		rangemap[i+1] =  idx2bitvect(it.at(i),m);
	}
	return rangemap;
}

vector< vector<int> >  iterate_all_from_num_max_areas(int m, int n){
	vector< vector<int> > results;
	for (int areaSize = 1; areaSize <= n; areaSize++){
		vector< vector<int> > it = iterate(m, areaSize);
		for (unsigned int i = 0; i < it.size(); i++)
			results.push_back(it[i]);
	}
	return results;
}

map< int, vector<int> > iterate_all_bv_from_num_max_areas_with_adjacency(int m, int n, vector <vector<bool> > adjMat, bool defaultAdjMat, map<int,string> areanamemaprev){
	vector< vector<int> > it = iterate_all_from_num_max_areas(m,n);
	map<int,vector<int> > rangemap;
	if (defaultAdjMat) {
		for (unsigned int i = 0; i < it.size(); i++)
			rangemap[i] =  idx2bitvect(it.at(i),m);
	}
	else {
		unsigned int rangemapIndex = 0;
		for (unsigned int i = 0; i < it.size(); i++) {

#ifdef CBR
			cout << i << " ";
			for (unsigned int x=0;x<it.at(i).size();x++){
				cout << areanamemaprev[it.at(i)[x]];
				if (x < (it.at(i).size() - 1))
					cout << "_";
			}
			if (adjacency_compliant(it.at(i), adjMat))
				cout << " Pass";
			else
				cout << " Fail";
			cout << endl;
#endif

			if (adjacency_compliant(it.at(i), adjMat)) {
				rangemap[rangemapIndex] = idx2bitvect(it.at(i),m);
				++rangemapIndex;
			}
		}
	}
	return rangemap;
}

bool adjacency_compliant(vector <int> indices, vector <vector<bool> > adjMat) {
	if (indices.size() == 1)
		return true;

	if (indices.size() == 2)
		return adjMat[indices[0]][indices[1]];
	else if (indices.size() > 2) {
		int minimalAdjacency = indices.size() - 1, numAdjLinks = 0;
		for (unsigned int i = 0; i < (indices.size() - 1); i++) {
			for (unsigned int j = i+1; j < indices.size(); j++) {
				if (adjMat[indices[i]][indices[j]])
					++numAdjLinks;
			}
		}

		if (numAdjLinks < minimalAdjacency)
			return false;
		else {
			bool foundDisjointArea = false;
			for (unsigned int i = 0; i < indices.size(); i++) {
				int adjacencyCount = 0;
				for (unsigned int j = 0; j < indices.size(); j++) {
					if ((j != i) && (adjMat[indices[i]][indices[j]]))
						++adjacencyCount;
				}
				if (adjacencyCount == 0) {
					foundDisjointArea = true;
					break;
				}
			}

			if (foundDisjointArea)
				return false;
			else
				return true;
		}
	}

	return false;
}


int main2 (){
	std::cout << comb(2,1) << std::endl;
	std::cout << iterate(7,2).at(20).at(0) << std::endl;
	std::cout << dists_by_maxsize(7,2).at(27).at(6) << std::endl;
	std::cout << iterate_all(4).at(11).at(1) << std::endl;
	std::cout << iterate_all_bv(15).size() <<std::endl;
	cout << comb_at_index(4,2,0).at(1) << endl;
	return 0;
}

