/*
 * BioGeoTree.cpp
 *
 *  Created on: Aug 15, 2009
 *      Author: Stephen A. Smith
 */
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <functional>
#include <numeric>
#include <iostream>
#include <cmath>
using namespace std;

//#include <armadillo>
//using namespace arma;

#include "BioGeoTree.h"
#include "BioGeoTreeTools.h"
#include "BranchSegment.h"
#include "RateMatrixUtils.h"
#include "RateModel.h"
#include "AncSplit.h"
#include "Utils.h"

#include "tree.h"
#include "node.h"
#include "vector_node_object.h"

//#include "omp.h"
//octave usage
//#include <octave/oct.h>

//#define DEBUG

Superdouble MAX(const Superdouble &a, const Superdouble &b){
	return b > a ? b:a;
}

/*
 * sloppy beginning but best for now because of the complicated bits
 */

//BioGeoTree::BioGeoTree(Tree * tr, vector<double> ps):tree(tr),periods(ps),
//		age("age"),dc("dist_conditionals"),en("excluded_dists"),
//		andc("anc_dist_conditionals"),columns(NULL),whichcolumns(NULL),rootratemodel(NULL),
//		distmap(NULL),store_p_matrices(false),use_stored_matrices(false),revB("revB"),
//		rev(false),rev_exp_number("rev_exp_number"),rev_exp_time("rev_exp_time"),
//		stochastic(false),stored_EN_matrices(map<int,map<double, mat > >()),stored_EN_CX_matrices(map<int,map<double, cx_mat > >()),
//		stored_ER_matrices(map<int,map<double, mat > >()),ultrametric(false),sim(false),ran_seed(314159265),sim_D(0.1),sim_E(0.1),
//		readSimStates(false),true_D(0),true_E(0){
BioGeoTree::BioGeoTree(Tree * tr, vector<double> ps):tree(tr),periods(ps),
		age("age"),dc("dist_conditionals"),en("excluded_dists"),
		andc("anc_dist_conditionals"),columns(NULL),whichcolumns(NULL),rootratemodel(NULL),
		distmap(NULL),store_p_matrices(false),use_stored_matrices(false),revB("revB"),
		rev(false),rev_exp_number("rev_exp_number"),rev_exp_time("rev_exp_time"),
		stochastic(false),ultrametric(false),sim(false),ran_seed(314159265),sim_D(0.1),sim_E(0.1),
		readSimStates(false),true_D(0),true_E(0){

	/*
	 * initialize each node with segments
	 */
	cout << "initializing nodes..." << endl;
	for(int i=0;i<tree->getNodeCount();i++){
		if(tree->getNode(i)->getBL()<0.000001)
			tree->getNode(i)->setBL(0.000001);
		tree->getNode(i)->initSegVector();
		tree->getNode(i)->initExclDistVector();
	}
	/*
	 * initialize the actual branch segments for each node
	 */
	if (ultrametric)
		tree->setHeightFromTipToNodes();
	else
		tree->setHeightForChronograms();
	cout << "initializing branch segments..." << endl;
	for(int i=0;i<tree->getNodeCount();i++){
		if (tree->getNode(i)->hasParent()){
			vector<double> pers(periods);
			double anc = tree->getNode(i)->getParent()->getHeight();
			double des = tree->getNode(i)->getHeight();
			//assert anc > des:q
			double t = des;
			if (pers.size() > 0){
				for(unsigned int j=0;j<pers.size();j++){
					double s = 0;
					if(pers.size() == 1)
						s = pers[0];
					for (unsigned int k=0;k<j+1;k++){
						s += pers[k];
					}
					if (t < s){
						double duration = min(s-t,anc-t);
						if (duration > 0){
							BranchSegment tseg = BranchSegment(duration,j);
							tree->getNode(i)->getSegVector()->push_back(tseg);
						}
						t += duration; // TODO: make sure that this is all working
					}
					if (t > anc || pers[j] > t){
						break;
					}
				}
			}else{
				BranchSegment tseg = BranchSegment(tree->getNode(i)->getBL(),0);
				tree->getNode(i)->getSegVector()->push_back(tseg);
			}
		}
	}
	/*
	 * initialize the periods for each internal node
	 */
	vector<double> cumulPeriod(periods.size(),0.0);
	partial_sum(periods.begin(),periods.end(),cumulPeriod.begin());
	for (int nodeNum = 0; nodeNum < tree->getInternalNodeCount(); nodeNum++) {
		Node * currNode = tree->getInternalNode(nodeNum);
		for (unsigned int ts = 0; ts < cumulPeriod.size(); ts++) {
			if (currNode->getHeight() <= cumulPeriod[ts]) {
				currNode->setPeriod(ts);
				break;
			}
		}
	}
	/*
	 * Initialise the period for the ROOT node
	 * (needed for some simulated topologies using independent software)
	 */
	tree->getRoot()->setPeriod(periods.size() - 1);
	/*
	 * Initialise the GSL RNG's
	 */
	gsl_rng_env_setup();
	T = gsl_rng_default;
	r = gsl_rng_alloc (T);
}

void BioGeoTree::set_store_p_matrices(bool i){
	store_p_matrices = i;
}

void BioGeoTree::set_use_stored_matrices(bool i){
	use_stored_matrices = i;
}

#ifdef DEBUG
void BioGeoTree::print_segs(){
	for(int i = 0; i < tree->getNodeCount(); i++) {
		Node * tmpNode = tree->getNode(i);
		vector<BranchSegment> * tsegs = tmpNode->getSegVector();
		if (tmpNode->hasParent()) {
			if (tmpNode->isExternal())
				cout << "Node : " << tmpNode->getName() << "\tHeight : " <<  tmpNode->getParent()->getHeight() - tmpNode->getHeight()
					 << "\tSegSize : " <<  tsegs->size() << endl;
			else
				cout << "Node : " << tmpNode->getNumber() << "\tHeight : " <<  tmpNode->getParent()->getHeight() - tmpNode->getHeight()
				 	 << "\tSegSize : " <<  tsegs->size() << endl;
		}
		else
			cout << "Node : " << tmpNode->getNumber() << "(ROOT)\tHeight : " <<  tmpNode->getHeight()
			 	 << "\tSegSize : " <<  tsegs->size() << endl;

		for(unsigned int j = 0; j < tsegs->size(); j++){
			cout << "\tSeg" << j << "\tPeriod: " << tsegs->at(j).getPeriod() << "\tDuration: " << tsegs->at(j).getDuration() << endl;
		}
		if (!tmpNode->isRoot()) {
			for(unsigned int k = 0; k < tsegs->at(0).distconds->size(); k++) {
				if (double(tsegs->at(0).distconds->at(k)) != 0) {
					cout << k << "(" << double(tsegs->at(0).distconds->at(k)) << ")";
					cout << endl;
				}
			}
		}
//		if (tmpNode->isInternal()) {
//			cout << "Children:";
//			for (int j = 0; j < tmpNode->getChildCount(); j++)
//				cout << "\t" << tmpNode->getChild(j).getNumber();
//			cout << endl;
//		}
		cout << endl;
	}
//	exit(-1);
}
#endif

void BioGeoTree::set_default_model(RateModel * mod){
	rootratemodel = mod;
	for(int i=0;i<tree->getNodeCount();i++){
		vector<BranchSegment> * tsegs = tree->getNode(i)->getSegVector();
		for(unsigned int j=0;j<tsegs->size();j++){
			tsegs->at(j).setModel(mod);
			vector<Superdouble> * distconds = new vector<Superdouble> (rootratemodel->getDists()->size(),0);
			tsegs->at(j).distconds = distconds;
			vector<Superdouble> * ancdistconds = new vector<Superdouble> (rootratemodel->getDists()->size(),0);
			tsegs->at(j).ancdistconds = ancdistconds;
		}
	}
	vector<Superdouble> * distconds = new vector<Superdouble> (rootratemodel->getDists()->size(),0);
	tree->getRoot()->assocDoubleVector(dc,*distconds);
	delete distconds;
	vector<Superdouble> * ancdistconds = new vector<Superdouble> (rootratemodel->getDists()->size(),0);
	tree->getRoot()->assocDoubleVector(andc,*ancdistconds);
	delete ancdistconds;
}

void BioGeoTree::update_default_model(RateModel * mod){
	rootratemodel = mod;

	for(int i=0;i<tree->getNodeCount();i++){
		vector<BranchSegment> * tsegs = tree->getNode(i)->getSegVector();
		for(unsigned int j=0;j<tsegs->size();j++){
			tsegs->at(j).setModel(mod);
		}
	}
}

void BioGeoTree::set_tip_conditionals(map<string,vector<int> > distrib_data){
	int numofleaves = tree->getExternalNodeCount();
	for(int i=0;i<numofleaves;i++){
		vector<BranchSegment> * tsegs = tree->getExternalNode(i)->getSegVector();
		RateModel * mod = tsegs->at(0).getModel();
		int ind1 = get_vector_int_index_from_multi_vector_int(
				&distrib_data[tree->getExternalNode(i)->getName()],mod->getDists());
		tsegs->at(0).distconds->at(ind1) = 1.0;
	}
}

void BioGeoTree::set_excluded_dist(vector<int> ind,Node * node){
	node->getExclDistVector()->push_back(ind);
}

void BioGeoTree::set_node_constraints(vector<vector<vector<int> > > exdists_per_period, map<int,string> areanamemaprev)
{
	vector<double> cumulPeriod(periods.size(),0.0);
	partial_sum(periods.begin(),periods.end(),cumulPeriod.begin());
	for (int nodeNum = 0; nodeNum < tree->getNodeCount(); nodeNum++) {
		Node * currNode = tree->getNode(nodeNum);
		for (unsigned int ts = 0; ts < cumulPeriod.size() && currNode->isInternal(); ts++) {
			if (currNode->getHeight() < cumulPeriod[ts]) {

#ifdef DEBUG
				if (currNode->isRoot())
					cout << "\nThese following dists will be unavailable for the ROOT (period : " << ts << ")" << endl;
				else
					cout << "\nThese following dists will be unavailable for the internal node : " << nodeNum << " (period : " << ts << ")" << endl;
#endif

				for (unsigned int dists = 0; dists < exdists_per_period[ts].size(); dists++) {
					set_excluded_dist(exdists_per_period[ts][dists],currNode);

#ifdef DEBUG
//					cout << dists << " " << print_area_vector(exdists_per_period[ts][dists],areanamemaprev) << endl;
#endif
				}
				break;
			}
		}
	}
}

Superdouble BioGeoTree::eval_likelihood(bool marginal){
	if( rootratemodel->sparse == true){
		columns = new vector<int>(rootratemodel->getDists()->size());
		whichcolumns = new vector<int>();
	}
	ancdist_conditional_lh(*tree->getRoot(),marginal);
	if( rootratemodel->sparse == true){
		delete columns;
		delete whichcolumns;
	}
//	return (-(log(calculate_vector_double_sum(*(vector<double>*) tree->getRoot()->getDoubleVector(dc)))));
	return -(calculate_vector_Superdouble_sum(*(vector<Superdouble>*) tree->getRoot()->getDoubleVector(dc))).getLn();
}


vector<Superdouble> BioGeoTree::conditionals(Node & node, bool marginal,bool sparse){
	vector<Superdouble> distconds;
	vector<BranchSegment> * tsegs = node.getSegVector();

	distconds = *tsegs->at(0).distconds;
	for(unsigned int i=0;i<tsegs->size();i++){
		for(unsigned int j=0;j<distconds.size();j++){
			tsegs->at(i).distconds->at(j) = distconds.at(j);
		}
		RateModel * rm = tsegs->at(i).getModel();
		vector<Superdouble> * v = new vector<Superdouble> (rootratemodel->getDists()->size(), 0);
//		vector<int> distrange;
//		if(tsegs->at(i).get_start_dist_int() != -666){
//			int ind1 = tsegs->at(i).get_start_dist_int();
//			distrange.push_back(ind1);
//		}else if(tsegs->at(i).getFossilAreas().size()>0){
//			for(unsigned int j=0;j<rootratemodel->getDists()->size();j++){
//				distrange.push_back(j);
//			}
//			for(unsigned int k=0;k<distrange.size();k++){
//				bool flag = true;
//				for(unsigned int x = 0;x<tsegs->at(i).getFossilAreas().size();x++){
//					if (tsegs->at(i).getFossilAreas()[x] == 1 && distrange.at(x) == 0){
//						flag = false;
//					}
//				}
//				if(flag == true){
//					distrange.erase(distrange.begin()+k);
//				}
//			}
//		}else{
//			for(unsigned int j=0;j<rootratemodel->getDists()->size();j++){
//				distrange.push_back(j);
//			}
//		}
		vector<int> * distrange = rootratemodel->get_incldistsint_per_period(tsegs->at(i).getPeriod());
		/*
		 * marginal
		 */
		if(marginal == true){
			if(sparse == false){
				vector<vector<double > > p;
				if(use_stored_matrices == false){
					p= rm->setup_fortran_P(tsegs->at(i).getPeriod(),tsegs->at(i).getDuration(),store_p_matrices);
				}else{
					p = rm->stored_p_matrices[tsegs->at(i).getPeriod()][tsegs->at(i).getDuration()];
				}
//				for(unsigned int j=0;j<distrange.size();j++){
//					for(unsigned int k=0;k<distconds.size();k++){
//						v->at(distrange[j]) += (distconds.at(k)*p[distrange[j]][k]);
//					}
//				}
				for(unsigned int j=0;j<distrange->size();j++){
					for(unsigned int k=0;k<distrange->size();k++){
						v->at(distrange->at(j)) += (distconds.at(distrange->at(k))*p[j][k]);
					}
				}
			}
//			else{//sparse
//				/*
//		  testing pthread version
//				 */
//				if(rm->get_nthreads() > 0){
//					vector<vector<double > > p = rm->setup_pthread_sparse_P(tsegs->at(i).getPeriod(),tsegs->at(i).getDuration(),*whichcolumns);
//					for(unsigned int j=0;j<distrange.size();j++){
//						for(unsigned int k=0;k<distconds.size();k++){
//							v->at(distrange[j]) += (distconds.at(k)*p[distrange[j]][k]);
//						}
//					}
//				}else{
//					for(unsigned int j=0;j<distrange.size();j++){
//						bool inthere = false;
//						if(columns->at(distrange[j]) == 1)
//							inthere = true;
//						vector<double > p;
//						if(inthere == true){
//							p = rm->setup_sparse_single_column_P(tsegs->at(i).getPeriod(),tsegs->at(i).getDuration(),distrange[j]);
//						}else{
//							p = vector<double>(distconds.size(),0);
//						}
//						for(unsigned int k=0;k<distconds.size();k++){
//							v->at(distrange[j]) += (distconds.at(k)*p[k]);
//						}
//					}
//				}
//			}
		}
		/*
		 * joint reconstruction
		 * NOT FINISHED YET -- DONT USE
		 */
//		else{
//			if(sparse == false){
//				vector<vector<double > > p = rm->setup_fortran_P(tsegs->at(i).getPeriod(),tsegs->at(i).getDuration(),store_p_matrices);
//				for(unsigned int j=0;j<distrange.size();j++){
//					Superdouble maxnum = 0;
//					for(unsigned int k=0;k<distconds.size();k++){
//						Superdouble tx = (distconds.at(k)*p[distrange[j]][k]);
//						maxnum = MAX(tx,maxnum);
//					}
//					v->at(distrange[j]) = maxnum;
//				}
//			}else{//sparse
//
//			}
//		}
		for(unsigned int j=0;j<distconds.size();j++){
			distconds[j] = v->at(j);
		}
		if(store_p_matrices == true){
			tsegs->at(i).seg_sp_alphas = distconds;
		}
		delete v;
	}
	/*
	 * if store is true we want to store the conditionals for each node
	 * for possible use in ancestral state reconstruction
	 */
	if(store_p_matrices == true){
		tsegs->at(0).alphas = distconds;
	}
	return distconds;
}

#ifdef DEBUG
//void LR_print(vector<int> leftdists, vector<int> rightdists) {
//	cout << "leftdists: ";
//	for (unsigned int j = 0; j < leftdists.size(); j++)
//		cout << leftdists[j] << " ";
//	cout << endl;
//	cout << "rightdists: ";
//	for (unsigned int j = 0; j < rightdists.size(); j++)
//		cout << rightdists[j] << " ";
//	cout << endl;
//}
#endif

void BioGeoTree::ancdist_conditional_lh(Node & node, bool marginal){
	vector<Superdouble> distconds(rootratemodel->getDists()->size(), 0);
	if (node.isExternal()==false){//is not a tip
		Node * c1 = &node.getChild(0);
		Node * c2 = &node.getChild(1);
		RateModel * model;
		if(node.hasParent()==true){
			vector<BranchSegment> * tsegs = node.getSegVector();
			model = tsegs->at(0).getModel();
		}else{
			model = rootratemodel;
		}
		ancdist_conditional_lh(*c1,marginal);
		ancdist_conditional_lh(*c2,marginal);

#ifdef DEBUG
		if (node.isRoot())
			cout << "ROOT : ";
		cout << "Analyzing internal node #" << node.getNumber() << endl;
#endif

		bool sparse = rootratemodel->sparse;
		vector<Superdouble> v1;
		vector<Superdouble> v2;
		if(sparse == true){
			//getcolumns
			vector<BranchSegment> * c1tsegs = c1->getSegVector();
			vector<BranchSegment> * c2tsegs = c2->getSegVector();
			vector<int> lcols = get_columns_for_sparse(*c1tsegs->at(0).distconds,rootratemodel);
			vector<int> rcols = get_columns_for_sparse(*c2tsegs->at(0).distconds,rootratemodel);
			whichcolumns->clear();
			for(unsigned int i=0;i<lcols.size();i++){
				if(lcols[i]==1 || rcols[i] ==1){
					columns->at(i)=1;
					if(i!=0 && count(whichcolumns->begin(),whichcolumns->end(),i) == 0)
						whichcolumns->push_back(i);
				}else{
					columns->at(i)=0;
				}
			}
			if(calculate_vector_int_sum(columns)==0){
				for(unsigned int i=0;i<lcols.size();i++){
					columns->at(i)=1;
				}
			}
			columns->at(0) = 0;
		}

		v1 =conditionals(*c1,marginal,sparse);
		v2 =conditionals(*c2,marginal,sparse);

#ifdef DEBUG
//		cout << "At internal node #" << node.getNumber() << endl
//			 << "sum(v1) = " << calculate_vector_Superdouble_sum(v1) << endl
//			 << "sum(v2) = " << calculate_vector_Superdouble_sum(v2) << endl << endl;

//		if (((sumV1 == 0) || (sumV2 == 0)) && !node.isRoot()) {
//			cout << endl;
//			for (unsigned int j = 0; j < v1.size(); j++)
//				cout << v1[j] << "\t" << v2[j] << endl;
//			cout << endl;
//			exit(-1);
//		}
//		if (node.isRoot()) {
//			cout << endl;
//			for (unsigned int j = 0; j < v1.size(); j++)
//				cout << v1[j] << "\t" << v2[j] << endl;
//			cout << endl;
//		}
#endif

		vector<vector<int> > * dists = rootratemodel->getDists();
		vector<int> leftdists;
		vector<int> rightdists;
		double weight;
		//cl1 = clock();

		for (unsigned int i=0;i<dists->size();i++){

			if(accumulate(dists->at(i).begin(),dists->at(i).end(),0) > 0){
				Superdouble lh = 0.0;
				vector<vector<int> >* exdist = node.getExclDistVector();
				int cou = count(exdist->begin(),exdist->end(),dists->at(i));
				if(cou == 0){
					iter_ancsplits_just_int(rootratemodel,dists->at(i),leftdists,rightdists,weight,node.getPeriod());
					for (unsigned int j=0;j<leftdists.size();j++){
						int ind1 = leftdists[j];
						int ind2 = rightdists[j];
						Superdouble lh_part = v1.at(ind1)*v2.at(ind2);
						lh += (lh_part * weight);
					}
				}
				distconds.at(i)= lh;
			}
#ifdef DEBUG
//			if (node.isRoot()) {
//				cout << "i: " << i << "; dist[i]: ";
//				for (unsigned int j = 0; j < (*dists)[i].size(); j++)
//					cout << (*dists)[i][j];
//				cout << "; weight: " << weight << endl;
//
//				LR_print(leftdists, rightdists);
//				cout << endl;
//			}
#endif
		}
		///cl2 = clock();
		//ti += cl2-cl1;
	}else{
#ifdef DEBUG
		cout << "Analyzing external node : " << node.getName() << endl;
#endif
		vector<BranchSegment> * tsegs = node.getSegVector();
		distconds = *tsegs->at(0).distconds;
	}
	//testing scale
	//if (run_with_scale){
	//	scale_node(&distconds);
	//}
	//testing scale
	if(node.hasParent() == true){
		vector<BranchSegment> * tsegs = node.getSegVector();
		for(unsigned int i=0;i<distconds.size();i++){
			tsegs->at(0).distconds->at(i) = distconds.at(i);
		}
#ifdef DEBUG
		cout << "Fractional likelihood sum at this node : "
			 << calculate_vector_Superdouble_sum(*(tsegs->at(0).distconds)) << endl << endl;
#endif
	}
	else{
		for(unsigned int i=0;i<distconds.size();i++){
			node.getDoubleVector(dc)->at(i) = distconds.at(i);
			//cout << distconds.at(i) << endl;
		}
#ifdef DEBUG
		cout << "Global likelihood at the ROOT : "
			 << calculate_vector_Superdouble_sum(*(node.getDoubleVector(dc))) << endl << endl;
#endif
	}
}

void BioGeoTree::set_ultrametric(bool ultMet)
{
	ultrametric = ultMet;
}

/*
 * ********************************************
 *
 * adds fossils either at the node or along a branch
 *
 * ********************************************
 */
void BioGeoTree::setFossilatNodeByMRCA(vector<string> nodeNames, int fossilarea){
	Node * mrca = tree->getMRCA(nodeNames);
	vector<vector<int> > * dists = rootratemodel->getDists();
	for(unsigned int i=0;i<dists->size();i++){
		if(dists->at(i).at(fossilarea) == 0){
			vector<vector<int> > * exd = mrca->getExclDistVector();
			exd->push_back(dists->at(i));
		}
	}
}
void BioGeoTree::setFossilatNodeByMRCA_id(Node * id, int fossilarea){
	vector<vector<int> > * dists = rootratemodel->getDists();
	for(unsigned int i=0;i<dists->size();i++){
		if(dists->at(i).at(fossilarea) == 0){
			vector<vector<int> > * exd = id->getExclDistVector();
			exd->push_back(dists->at(i));
		}
	}
}
void BioGeoTree::setFossilatBranchByMRCA(vector<string> nodeNames, int fossilarea, double age){
	Node * mrca = tree->getMRCA(nodeNames);
	vector<BranchSegment> * tsegs = mrca->getSegVector();
	double startage = mrca->getHeight();
	for(unsigned int i=0;i<tsegs->size();i++){
		if(age > startage && age < (startage+tsegs->at(i).getDuration())){
			tsegs->at(i).setFossilArea(fossilarea);
		}
		startage += tsegs->at(i).getDuration();
	}
}
void BioGeoTree::setFossilatInternalBranchByMRCA_id(Node * id, int fossilarea, double age){
	vector<BranchSegment> * tsegs = id->getSegVector();
	double startage = id->getHeight();

	for(unsigned int i=0;i<tsegs->size();i++){
		//	CBR (23.02.2014) accounting for fossil age = start/end of time period
		if(age > startage && age <= (startage+tsegs->at(i).getDuration())){
			tsegs->at(i).setFossilArea(fossilarea);
		}
		startage += tsegs->at(i).getDuration();
	}
}
void BioGeoTree::setFossilatExternalBranchByMRCA_id(Node * id, int fossilarea, double age){
	vector<BranchSegment> * tsegs = id->getSegVector();
	double startage = id->getHeight();

	for(unsigned int i=0;i<tsegs->size();i++){
		//	CBR (23.02.2014) accounting for fossil age = start/end of time period
		if(age > startage && age <= (startage+tsegs->at(i).getDuration())){
			tsegs->at(i).setFossilArea(fossilarea);
		}
		startage += tsegs->at(i).getDuration();
	}
}


/************************************************************
 forward and reverse stuff for ancestral states
 ************************************************************/
//add joint
void BioGeoTree::prepare_ancstate_reverse(){
	reverse(*tree->getRoot());
}

/*
 * called from prepare_ancstate_reverse and that is all
 */
void BioGeoTree::reverse(Node & node){
	rev = true;
	vector<Superdouble> * revconds = new vector<Superdouble> (rootratemodel->getDists()->size(), 0);//need to delete this at some point
	if (&node == tree->getRoot()) {
		vector<vector<int> > * inc_dists = rootratemodel->get_incldists_per_period(node.getPeriod());
		map<vector<int>,int> * distsmap = rootratemodel->get_dists_int_map();
		vector<vector<int> > * exdist = node.getExclDistVector();
		for(unsigned int i=0;i<inc_dists->size();i++){
			int cou = count(exdist->begin(), exdist->end(), inc_dists->at(i));
			if (cou == 0)
				revconds->at((*distsmap)[inc_dists->at(i)]) = 1.0;//prior
			else
				revconds->at((*distsmap)[inc_dists->at(i)]) = 0.0;//prior
		}

		node.assocDoubleVector(revB,*revconds);
		delete revconds;
		for(int i = 0;i<node.getChildCount();i++){
			reverse(node.getChild(i));
		}
	}
	else if(node.isExternal() == false){
		//calculate A i
		//sum over all alpha k of sister node of the parent times the priors of the speciations
		//(weights) times B of parent j
		vector<Superdouble> * parrev = node.getParent()->getDoubleVector(revB);
		vector<Superdouble> sisdistconds;
		if(&node.getParent()->getChild(0) != &node){
			vector<BranchSegment> * tsegs = node.getParent()->getChild(0).getSegVector();
			sisdistconds = tsegs->at(0).alphas;
		}else{
			vector<BranchSegment> * tsegs = node.getParent()->getChild(1).getSegVector();
			sisdistconds = tsegs->at(0).alphas;
		}
		vector<vector<int> > * dists = rootratemodel->getDists();
		vector<int> leftdists;
		vector<int> rightdists;
		double weight;
		//cl1 = clock();
		vector<Superdouble> tempA (rootratemodel->getDists()->size(),0);
		for (unsigned int i = 0; i < dists->size(); i++) {
			if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
				vector<vector<int> > * exdist = node.getExclDistVector();
				int cou = count(exdist->begin(), exdist->end(), dists->at(i));
				if (cou == 0) {
					iter_ancsplits_just_int(rootratemodel, dists->at(i), leftdists, rightdists, weight, node.getPeriod());
					//root has i, curnode has left, sister of cur has right
					for (unsigned int j = 0; j < leftdists.size(); j++) {
						int ind1 = leftdists[j];
						int ind2 = rightdists[j];
						tempA[ind1] += (sisdistconds.at(ind2)*weight*parrev->at(i));
					}
				}
			}
		}

		//now calculate node B
		vector<BranchSegment>* tsegs = node.getSegVector();
		vector<Superdouble> tempmoveA(tempA);
		//for(unsigned int ts=0;ts<tsegs->size();ts++){
		for(int ts = tsegs->size()-1;ts != -1;ts--){
			for(unsigned int j=0;j<dists->size();j++){revconds->at(j) = 0;}
			RateModel * rm = tsegs->at(ts).getModel();
			vector<vector<double > > * p = &rm->stored_p_matrices[tsegs->at(ts).getPeriod()][tsegs->at(ts).getDuration()];
//			mat * EN = NULL;
//			mat * ER = NULL;
			vector<Superdouble> tempmoveAer(tempA);
			vector<Superdouble> tempmoveAen(tempA);
			if(stochastic == true){
				//initialize the segment B's
				for(unsigned int j=0;j<dists->size();j++){tempmoveAer[j] = 0;}
				for(unsigned int j=0;j<dists->size();j++){tempmoveAen[j] = 0;}
//				EN = &stored_EN_matrices[tsegs->at(ts).getPeriod()][tsegs->at(ts).getDuration()];
//				ER = &stored_ER_matrices[tsegs->at(ts).getPeriod()][tsegs->at(ts).getDuration()];
				//cout << (*EN) << endl;
//				cx_mat * EN_CX = NULL;
//				EN_CX = &stored_EN_CX_matrices[tsegs->at(ts).getPeriod()][tsegs->at(ts).getDuration()];
				//cout << (*EN_CX) << endl;
				//exit(0);
			}
//			for(unsigned int j=0;j < dists->size();j++){
//				if(accumulate(dists->at(j).begin(), dists->at(j).end(), 0) > 0){
//					for (unsigned int i = 0; i < dists->size(); i++) {
//						if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
//							//cout << "here " << j << " " << i<< " " << node.getBL() << " " << ts <<" " << tsegs->size() << endl;
//							revconds->at(j) += tempmoveA[i]*((*p)[i][j]);//tempA needs to change each time
//							//cout << "and" << endl;
//							if(stochastic == true){
//								tempmoveAer[j] += tempmoveA[i]*(((*ER)(i,j)));
//								tempmoveAen[j] += tempmoveA[i]*(((*EN)(i,j)));
//							}
//						}
//					}
//				}
//			}
			//	the adjacency per time period version below
			vector<int> * validists = rootratemodel->get_incldistsint_per_period(tsegs->at(ts).getPeriod());
			for(unsigned int j=0;j < validists->size();j++)
				if(accumulate(dists->at(validists->at(j)).begin(), dists->at(validists->at(j)).end(), 0) > 0)
					for (unsigned int i = 0; i < validists->size(); i++)
						if (accumulate(dists->at(validists->at(i)).begin(), dists->at(validists->at(i)).end(), 0) > 0) {
//							revconds->at(validists->at(j)) += tempmoveA[i]*((*p)[i][j]);//tempA needs to change each time
							revconds->at(validists->at(j)) += tempmoveA[validists->at(i)]*((*p)[i][j]);//tempA needs to change each time
						}

			for(unsigned int j=0;j<dists->size();j++)
				tempmoveA[j] = revconds->at(j);

			if(stochastic == true){
				tsegs->at(ts).seg_sp_stoch_map_revB_time = tempmoveAer;
				tsegs->at(ts).seg_sp_stoch_map_revB_number = tempmoveAen;
			}
		}
		node.assocDoubleVector(revB,*revconds);
		delete revconds;
		for(int i = 0;i<node.getChildCount();i++){
			reverse(node.getChild(i));
		}
	}
}

/*
 * calculates the most likely split (not state) -- the traditional result for lagrange
 */

map<vector<int>,vector<AncSplit> > BioGeoTree::calculate_ancsplit_reverse(Node & node,bool marg){
	vector<Superdouble> * Bs = node.getDoubleVector(revB);
	map<vector<int>,vector<AncSplit> > ret;
	for(unsigned int j=0;j<rootratemodel->getDists()->size();j++){
		vector<int> dist = rootratemodel->getDists()->at(j);
		vector<AncSplit> ans = iter_ancsplits(rootratemodel,dist,node.getPeriod());
		if (node.isExternal()==false){//is not a tip
			Node * c1 = &node.getChild(0);
			Node * c2 = &node.getChild(1);
			vector<BranchSegment> * tsegs1 = c1->getSegVector();
			vector<BranchSegment> * tsegs2 = c2->getSegVector();
			for (unsigned int i=0;i<ans.size();i++){
				vector<vector<int> > * exdist = node.getExclDistVector();
				int cou = count(exdist->begin(), exdist->end(), (*rootratemodel->get_int_dists_map())[ans[i].ancdistint]);
				if (cou == 0) {
					vector<Superdouble> v1  =tsegs1->at(0).alphas;
					vector<Superdouble> v2 = tsegs2->at(0).alphas;
					Superdouble lh = (v1[ans[i].ldescdistint]*v2[ans[i].rdescdistint]*Bs->at(j)*ans[i].getWeight());
					ans[i].setLikelihood(lh);
					//cout << lh << endl;
				}
			}
		}
		ret[dist] = ans;
	}
	return ret;
}

/*
 * calculates the ancestral area over all the possible states
 */
vector<Superdouble> BioGeoTree::calculate_ancstate_reverse(Node & node,bool marg){
	if (node.isExternal()==false){//is not a tip
		vector<Superdouble> * Bs = node.getDoubleVector(revB);
		vector<vector<int> > * dists = rootratemodel->getDists();
		vector<int> leftdists;
		vector<int> rightdists;
		double weight;
		Node * c1 = &node.getChild(0);
		Node * c2 = &node.getChild(1);
		vector<BranchSegment>* tsegs1 = c1->getSegVector();
		vector<BranchSegment>* tsegs2 = c2->getSegVector();
		vector<Superdouble> v1  =tsegs1->at(0).alphas;
		vector<Superdouble> v2 = tsegs2->at(0).alphas;
		vector<Superdouble> LHOODS (dists->size(),0);
		for (unsigned int i = 0; i < dists->size(); i++) {
			if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
				vector<vector<int> > * exdist = node.getExclDistVector();
				int cou = count(exdist->begin(), exdist->end(), dists->at(i));
				if (cou == 0) {
					iter_ancsplits_just_int(rootratemodel, dists->at(i),leftdists, rightdists, weight, node.getPeriod());
					for (unsigned int j=0;j<leftdists.size();j++){
						int ind1 = leftdists[j];
						int ind2 = rightdists[j];
						LHOODS[i] += (v1.at(ind1)*v2.at(ind2)*weight);
					}
					LHOODS[i] *= Bs->at(i);
				}
			}
		}
		return LHOODS;
	}
	return vector<Superdouble> ();
}


/************************************************************************
 simulate ancestral states on BioGeoTree following a pre-order traversal
 ************************************************************************/
void BioGeoTree::prepare_simulation(unsigned long int seed, bool ranPar)
{
	sim = true;

	ran_seed = seed;
	gsl_rng_set(r, ran_seed);
	if (ranPar) {
		do {
			sim_D = gsl_ran_flat(r, 0, 1) * 0.2;
			sim_E = gsl_ran_flat(r, 0, 1) * 0.2;
		} while ((sim_D != 0) && (sim_E != 0) && (sim_D <= sim_E));
	}
	else {
		if (sim_D < sim_E)
			cout << "\nNote that manually setting the rate of geographic dispersal to be less than that of extinction"
					"\nwill lead to highly trivial biogeographic histories. Continuing anyway..." << endl;
	}

	rootratemodel->setup_D(sim_D);
	rootratemodel->setup_E(sim_E);
	rootratemodel->setup_Q_with_adjacency();
	update_default_model(rootratemodel);

	cout << "dispersal : " << sim_D << "\textinction : " << sim_E << "\tseed : " << ran_seed;
	simulate(*tree->getRoot());
}

/*
 * called from prepare_simulation and that is all
 */
void BioGeoTree::simulate(Node & node)
{
	if (&node == tree->getRoot()) {
		vector<Superdouble> simconds = vector<Superdouble> (rootratemodel->getDists()->size(), 0);
		vector<vector<int> > * inc_dists = rootratemodel->get_incldists_per_period(node.getPeriod());
		map<vector<int>,int> * distsintmap = rootratemodel->get_dists_int_map();
		vector<vector<int> > * exdist = node.getExclDistVector();

		//	randomly choose the ROOT dist
		size_t root_dist;
		do {
			root_dist = size_t(floor(gsl_ran_flat(r, 1, inc_dists->size())));
		} while (count(exdist->begin(), exdist->end(), inc_dists->at(root_dist)) != 0);

		//	set the ROOT prior for the forward simulation
		simconds.at((*distsintmap)[inc_dists->at(root_dist)]) = 1.0;
		tt.summarizeSimState(node,simconds,rootratemodel);

		//	randomly choose the speciation model (i.e. the dist split)
		vector<vector<vector<int> > > * splits = rootratemodel->get_iter_dist_splits_per_period(inc_dists->at(root_dist),node.getPeriod());
		int splitIdx = int(floor(gsl_ran_flat(r, 0, splits->at(0).size())));
		node.setIntObject("simsplit",splitIdx);

#ifdef DEBUG
		cout << "considered dist : " << print_area_vector(inc_dists->at(root_dist),*rootratemodel->get_areanamemaprev())
			 << "\tno. of splits : " << splits->at(0).size()
			 << "\tchosen split : " << splitIdx << " (" << print_area_vector(splits->at(0)[splitIdx],*rootratemodel->get_areanamemaprev()) << ")" << endl;
		cout << "Left split\tRight split" << endl;
		for (size_t spl = 0; spl < splits->at(0).size(); spl++)
			cout << print_area_vector(splits->at(0)[spl],*rootratemodel->get_areanamemaprev())
				 << "\t" << print_area_vector(splits->at(1)[spl],*rootratemodel->get_areanamemaprev()) << endl;
#endif

	}
	else {
		vector<vector<int> > * dists = rootratemodel->getDists();
		map<vector<int>,int> * distsintmap = rootratemodel->get_dists_int_map();
		map<int,vector<int> > * intdistsmap = rootratemodel->get_int_dists_map();
		vector<vector<vector<int> > > * ancSplits = rootratemodel->get_iter_dist_splits_per_period((*intdistsmap)[*(node.getParent()->getIntObject("simdistidx"))],node.getParent()->getPeriod());
		vector<Superdouble> simconds = vector<Superdouble> (dists->size(), 0);

		//	the rule here for assigning the randomly chosen parent dist split
		//	child(0) gets the corresponding "left" split
		//	child(1) gets the corresponding "right" split
		int ancSplitIdx = (*node.getParent()->getIntObject("simsplit"));

#ifdef DEBUG
		cout << "\nStart dist : " << print_area_vector(ancSplits->at(0)[ancSplitIdx],*rootratemodel->get_areanamemaprev()) << endl;
#endif

		//	set the node prior for the forward simulation
		if(&node.getParent()->getChild(0) == &node)
			simconds.at((*distsintmap)[ancSplits->at(0)[ancSplitIdx]]) = 1.0;
		else
			simconds.at((*distsintmap)[ancSplits->at(1)[ancSplitIdx]]) = 1.0;

		vector<BranchSegment>* tsegs = node.getSegVector();
#ifdef DEBUG
		cout << "Seg size : " << tsegs->size() << endl;
		cout << "Prior sum : " << calculate_vector_Superdouble_sum(simconds) << endl;
#endif

		for(int ts = tsegs->size() - 1; ts != -1; ts--) {
			vector<Superdouble> * segconds = new vector<Superdouble> (dists->size(), 0);
			RateModel * rm = tsegs->at(ts).getModel();
			vector<vector<double > > p = rm->setup_fortran_P(tsegs->at(ts).getPeriod(),tsegs->at(ts).getDuration(),false);
			vector<int> * validists = rm->get_incldistsint_per_period(tsegs->at(ts).getPeriod());

			for(unsigned int j=0;j < validists->size();j++)
				for (unsigned int i = 0; i < validists->size(); i++)
					segconds->at(validists->at(j)) += simconds[validists->at(i)] * p[i][j];

			for(unsigned int j=0;j<dists->size();j++)
				simconds[j] = segconds->at(j);

			delete segconds;
		}

		tt.summarizeSimState(node,simconds,rootratemodel);

		//	randomly choose the speciation model (i.e. the dist split)
		if (node.isInternal()) {
			vector<vector<vector<int> > > * nodeSplits = rootratemodel->get_iter_dist_splits_per_period((*intdistsmap)[*(node.getIntObject("simdistidx"))],node.getPeriod());
			int splitIdx = int(floor(gsl_ran_flat(r, 0, nodeSplits->at(0).size())));
			node.setIntObject("simsplit",splitIdx);

#ifdef DEBUG
			cout << "considered dist : " << print_area_vector((*intdistsmap)[*(node.getIntObject("simdistidx"))],*rootratemodel->get_areanamemaprev())
				 << "\tno. of splits : " << nodeSplits->at(0).size()
				 << "\tchosen split : " << splitIdx << " (" << print_area_vector(nodeSplits->at(0)[splitIdx],*rootratemodel->get_areanamemaprev()) << ")" << endl;
			cout << "Left split\tRight split" << endl;
			for (size_t spl = 0; spl < nodeSplits->at(0).size(); spl++)
				cout << print_area_vector(nodeSplits->at(0)[spl],*rootratemodel->get_areanamemaprev())
					 << "\t" << print_area_vector(nodeSplits->at(1)[spl],*rootratemodel->get_areanamemaprev()) << endl;
#endif

		}
	}

	for(int i = 0;i<node.getChildCount();i++)
		simulate(node.getChild(i));
}

double BioGeoTree::getSim_D()
{
	return sim_D;
}

double BioGeoTree::getSim_E()
{
	return sim_E;
}

void BioGeoTree::setSim_D(const double disp)
{
	sim_D = disp;
}

void BioGeoTree::setSim_E(const double ext)
{
	sim_E = ext;
}

void BioGeoTree::read_true_states(string truestatesfile)
{
	ifstream ifs(truestatesfile.c_str());
	bool first = false, second = false;
	int nareas = rootratemodel->get_num_areas();
	map<string,vector<int> > data;
	string line;
	vector<string> tokens;
	string del("\t ");
	while(getline(ifs,line)){
		tokens.clear();
		Tokenize(line, tokens, del);
		for(unsigned int j=0;j<tokens.size();j++){
			TrimSpaces(tokens[j]);
		}
		if (first == false){
			first = true;
			true_D = atof(tokens[0].c_str());
			cout << "True dispersal rate : " << tokens[0] << endl;
		}
		else if (second == false){
			second = true;
			true_E = atof(tokens[0].c_str());
			cout << "True extinction rate : " << tokens[0] << endl;
		}else{
			cout << "True state for node: " << tokens[0] << " ";
			vector<int> speciesdata(nareas,0);
			for(int i=0;i<nareas;i++){
				char spot;
				spot = tokens[1][i];
				if (spot == '1')
					speciesdata[i] = 1;
				cout << spot - '0';
			}
			cout << " (" << print_area_vector(speciesdata,*rootratemodel->get_areanamemaprev()) << ")" << endl;
			trueStates[atoi(tokens[0].c_str())] = speciesdata;
		}
	}
	ifs.close();
}

vector<int> * BioGeoTree::get_true_state(int num)
{
	return &trueStates[num];
}

double BioGeoTree::getTrue_D()
{
	return true_D;
}

double BioGeoTree::getTrue_E()
{
	return true_E;
}

/**********************************************************
 * forward and reverse stuff for stochastic mapping
 **********************************************************/

//void BioGeoTree::prepare_stochmap_reverse_all_nodes(int from , int to){
//	stochastic = true;
//	int ndists = rootratemodel->getDists()->size();
//
//	//calculate and store local expectation matrix for each branch length
//	//#pragma omp parallel for ordered num_threads(8)
//	for(int k = 0; k < tree->getNodeCount(); k++){
//		//cout << k << " " << tree->getNodeCount() << endl;
//		vector<BranchSegment>* tsegs = tree->getNode(k)->getSegVector();
//		for (unsigned int l = 0;l<tsegs->size();l++){
//			int per = (*tsegs)[l].getPeriod();
//			double dur =  (*tsegs)[l].getDuration();
//			cx_mat eigvec(ndists,ndists);eigvec.fill(cx_double(0.0, 0.0));
//			cx_mat eigval(ndists,ndists);eigval.fill(cx_double(0.0, 0.0));
//			bool isImag = rootratemodel->get_eigenvec_eigenval_from_Q(&eigval, &eigvec,per);
//			mat Ql(ndists,ndists);Ql.fill(0);Ql(from,to) = rootratemodel->get_Q()[per][from][to];
//			mat W(ndists,ndists);W.fill(0);W(from,from) = 1;
//			cx_mat summed(ndists,ndists);summed.fill(cx_double(0.0, 0.0));
//			cx_mat summedR(ndists,ndists);summedR.fill(cx_double(0.0, 0.0));
//			for(int i=0;i<ndists;i++){
//				mat Ei(ndists,ndists);Ei.fill(0);Ei(i,i)=1;
//				cx_mat Si(ndists,ndists);
//				Si = eigvec * Ei * inv(eigvec);
//				for(int j=0;j<ndists;j++){
//					cx_double dij = (eigval(i,i)-eigval(j,j)) * dur;
//					mat Ej(ndists,ndists);Ej.fill(0);Ej(j,j)=1;
//					cx_mat Sj(ndists,ndists);
//					Sj = eigvec * Ej * inv(eigvec);
//					cx_double Iijt = 0;
//					if (abs(dij) > 10){
//						Iijt = (exp(eigval(i,i)*dur)-exp(eigval(j,j)*dur))/(eigval(i,i)-eigval(j,j));
//					}else if(abs(dij) < 10e-20){
//						Iijt = dur*exp(eigval(j,j)*dur)*(1.+dij/2.+pow(dij,2.)/6.+pow(dij,3.)/24.);
//					}else{
//						if(eigval(i,i) == eigval(j,j)){
//							//WAS Iijt = dur*exp(eigval(j,j)*dur)*expm1(dij)/dij;
//							if (isImag)
//								Iijt = dur*exp(eigval(j,j)*dur)*(exp(dij)-1.)/dij;
//							else
//								Iijt = dur*exp(eigval(j,j)*dur)*(expm1(real(dij)))/dij;
//						}else{
//							//WAS Iijt = -dur*exp(eigval(i,i)*dur)*expm1(-dij)/dij;
//							if (isImag)
//								Iijt = -dur*exp(eigval(i,i)*dur)*(exp(-dij)-1.)/dij;
//							else
//								Iijt = -dur*exp(eigval(i,i)*dur)*(expm1(real(-dij)))/dij;
//						}
//					}
//					summed += (Si  * Ql * Sj * Iijt);
//					summedR += (Si * W * Sj * Iijt);
//				}
//			}
//			stored_EN_matrices[per][dur] = (real(summed));
//			stored_EN_CX_matrices[per][dur] = summed;
//			stored_ER_matrices[per][dur] = (real(summedR));
//			//for(int i=0;i<ndists;i++){
//			//	for (int j=0;j<ndists;j++){
//			//		if (real(summed(i,j)) < 0){
//			//			cout <<"N:" <<  summed << endl;
//			//			cout << endl;
//			//			exit(0);
//			//		}
//			//		if (real(summedR(i,j)) < 0){
//			//			cout <<"R:" << summedR << endl;
//			//			cout << endl;
//			//			exit(0);
//			//		}
//			//	}
//			//}
//		}
//	}
//}

/*
 * called directly after reverse_stochastic
 */

vector<Superdouble> BioGeoTree::calculate_reverse_stochmap(Node & node,bool time){
	if (node.isExternal()==false){//is not a tip
		vector<BranchSegment> * tsegs = node.getSegVector();
		vector<vector<int> > * dists = rootratemodel->getDists();
		vector<Superdouble> totalExp (dists->size(),0);
		for(int t = 0;t<tsegs->size();t++){
			if (t == 0){
				vector<Superdouble> Bs;
				if(time)
					Bs = tsegs->at(t).seg_sp_stoch_map_revB_time;
				else
					Bs =  tsegs->at(t).seg_sp_stoch_map_revB_number;
				vector<int> leftdists;
				vector<int> rightdists;
				double weight;
				Node * c1 = &node.getChild(0);
				Node * c2 = &node.getChild(1);
				vector<BranchSegment>* tsegs1 = c1->getSegVector();
				vector<BranchSegment>* tsegs2 = c2->getSegVector();
				vector<Superdouble> v1  =tsegs1->at(0).alphas;
				vector<Superdouble> v2 = tsegs2->at(0).alphas;
				vector<Superdouble> LHOODS (dists->size(),0);
				for (unsigned int i = 0; i < dists->size(); i++) {
					if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
						vector<vector<int> > * exdist = node.getExclDistVector();
						int cou = count(exdist->begin(), exdist->end(), dists->at(i));
						if (cou == 0) {
							iter_ancsplits_just_int(rootratemodel, dists->at(i),leftdists, rightdists, weight, node.getPeriod());
							for (unsigned int j=0;j<leftdists.size();j++){
								int ind1 = leftdists[j];
								int ind2 = rightdists[j];
								LHOODS[i] += (v1.at(ind1)*v2.at(ind2)*weight);
							}
							LHOODS[i] *= Bs.at(i);
						}
					}
				}
				for(int i=0;i<dists->size();i++){
					totalExp[i] = LHOODS[i];
				}
			}else{
				vector<Superdouble> alphs = tsegs->at(t-1).seg_sp_alphas;
				vector<Superdouble> Bs;
				if(time)
					Bs = tsegs->at(t).seg_sp_stoch_map_revB_time;
				else
					Bs =  tsegs->at(t).seg_sp_stoch_map_revB_number;
				vector<Superdouble> LHOODS (dists->size(),0);
				for (unsigned int i = 0; i < dists->size(); i++) {
					if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
						vector<vector<int> > * exdist = node.getExclDistVector();
						int cou = count(exdist->begin(), exdist->end(), dists->at(i));
						if (cou == 0) {
							LHOODS[i] = Bs.at(i) * (alphs[i] );//do i do this or do i do from i to j
						}
					}
				}
				for(int i=0;i<dists->size();i++){
					totalExp[i] += LHOODS[i];
				}
			}
		}
		//not sure if this should return a Superdouble or not when doing a bigtree
		return totalExp;
	}else{
		vector<BranchSegment> * tsegs = node.getSegVector();
		vector<vector<int> > * dists = rootratemodel->getDists();
		vector<Superdouble> totalExp (dists->size(),0);
		for(int t = 0;t<tsegs->size();t++){
			if(t == 0){
				vector<Superdouble> Bs;
				if(time)
					Bs = tsegs->at(t).seg_sp_stoch_map_revB_time;
				else
					Bs =  tsegs->at(t).seg_sp_stoch_map_revB_number;
				vector<Superdouble> LHOODS (dists->size(),0);
				for (unsigned int i = 0; i < dists->size(); i++) {
					if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
						vector<vector<int> > * exdist = node.getExclDistVector();
						int cou = count(exdist->begin(), exdist->end(), dists->at(i));
						if (cou == 0) {
							LHOODS[i] = Bs.at(i) * (tsegs->at(0).distconds->at(i) );
						}
					}
				}
				for(int i=0;i<dists->size();i++){
					totalExp[i] = LHOODS[i];
				}
			}else{
				vector<Superdouble> alphs = tsegs->at(t-1).seg_sp_alphas;
				vector<Superdouble> Bs;
				if(time)
					Bs = tsegs->at(t).seg_sp_stoch_map_revB_time;
				else
					Bs =  tsegs->at(t).seg_sp_stoch_map_revB_number;
				vector<Superdouble> LHOODS (dists->size(),0);
				for (unsigned int i = 0; i < dists->size(); i++) {
					if (accumulate(dists->at(i).begin(), dists->at(i).end(), 0) > 0) {
						vector<vector<int> > * exdist = node.getExclDistVector();
						int cou = count(exdist->begin(), exdist->end(), dists->at(i));
						if (cou == 0) {
							LHOODS[i] = Bs.at(i) * (alphs[i]);
						}
					}
				}
				for(int i=0;i<dists->size();i++){
					totalExp[i] += LHOODS[i];
				}
			}
		}
		return totalExp;
	}
}

/**********************************************************
 * trash collection
 **********************************************************/
BioGeoTree::~BioGeoTree(){
	for(int i=0;i<tree->getNodeCount();i++){
		vector<BranchSegment> * tsegs = tree->getNode(i)->getSegVector();
		for(unsigned int j=0;j<tsegs->size();j++){
			delete tsegs->at(j).distconds;
			delete tsegs->at(j).ancdistconds;
		}
		tree->getNode(i)->deleteExclDistVector();
		if(rev == true && tree->getNode(i)->isInternal()){
			tree->getNode(i)->deleteDoubleVector(revB);
		}
		if(sim == true){
			delete tree->getNode(i)->getObject("simstate");
		}
		tree->getNode(i)->deleteSegVector();
	}
	tree->getRoot()->deleteDoubleVector(dc);
	tree->getRoot()->deleteDoubleVector(andc);
	tree->getRoot()->deleteDoubleVector(revB);

	gsl_rng_free (r);
}

