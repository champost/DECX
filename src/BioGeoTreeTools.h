/*
 * PhyloTree.h
 *
 *  Created on: Aug 15, 2009
 *      Author: smitty
 */

#ifndef PHYLOTREE_H_
#define PHYLOTREE_H_

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include "AncSplit.h"
using namespace std;
#include "superdouble.h"
#include "tree.h"
#include "node.h"

#ifdef XYZ
#include "gmpfrxx/gmpfrxx.h"
#endif

class BioGeoTreeTools {
public :
	Tree * getTreeFromString(string treestring);
	vector<Node *> getAncestors(Tree & tree, Node & node);

	void summarizeSplits(Node * node,map<vector<int>,vector<AncSplit> > & ans,map<int,string> &areanamemaprev, RateModel * rm);
	void summarizeAncState(Node * node,vector<Superdouble> & ans,map<int,string> &areanamemaprev, RateModel * rm, bool NodeLHOODS, ofstream &NodeLHOODFile);
	string get_string_from_dist_int(int dist,map<int,string> &areanamemaprev, RateModel * rm);
	void summarizeSimState(Node & node,vector<Superdouble> & ans,RateModel * rm);

	friend class BioGeoTree;
};

#endif /* PHYLOTREE_H_ */
