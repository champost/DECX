/*
 * InputReader.cpp
 *
 *  Created on: Aug 21, 2009
 *      Author: smitty
 */

#include <string>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include "InputReader.h"
#include "BioGeoTreeTools.h"
#include "Utils.h"
#include "RateMatrixUtils.h"

#include "tree_reader.h"
#include "tree.h"
#include "node.h"

InputReader::InputReader():nareas(0),nspecies(0){
}

void InputReader::readMultipleTreeFile(string filename, vector<Tree *> & ret){
	TreeReader tr;
	ifstream ifs( filename.c_str() );
	string temp;
	int count = 1;
	while( getline( ifs, temp ) ){
		if(temp.size() > 1){
			Tree * intree = tr.readTree(temp);
			intree->setNewickStr(temp);
			cout << "Tree "<< count <<" has " << intree->getExternalNodeCount() << " leaves." << endl;
			ret.push_back(intree);
			count++;
		}
	}
}

void InputReader::checkData(map<string,vector<int> > data ,vector<Tree *> trees){
	vector<string> dataspecies;
	map<string,vector<int> >::const_iterator itr;
	for(itr = data.begin(); itr != data.end(); ++itr){
		dataspecies.push_back(itr->first);
	}
	vector<string> treespecies;
	for(int j=0;j<trees[0]->getExternalNodeCount();j++){
		treespecies.push_back(trees[0]->getExternalNode(j)->getName());
		int count = 0;
		for (unsigned int k=0;k<dataspecies.size();k++){
			if (trees[0]->getExternalNode(j)->getName() == dataspecies[k])
				count += 1;
		}
		if(count != 1){
			cout << "Error: " << trees[0]->getExternalNode(j)->getName() << " found "<<count<<" times in data file." << endl;
			exit(0);
		}
	}
	for (int j=0;j<dataspecies.size();j++){
		int count = 0;
		for (int k=0;k<treespecies.size();k++){
			if (dataspecies[j] == treespecies[k]){
				count += 1;
			}
		}
		if (count != 1){
			cerr << "Error: "<<dataspecies[j]<<" found "<<count<<" times in tree file."<<endl;
			exit(0);
		}
	}
}
