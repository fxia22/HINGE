#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unordered_map>
#include "DB.h"
#include "align.h"
#include "LAInterface.h"
#include "OverlapGraph.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>
#include <omp.h>
#include "INIReader.h"

extern "C" {
#include "common.h"
}



#define LAST_READ_SYMBOL  '$'

typedef std::tuple<Node, Node, int> Edge_w;

static int ORDER(const void *l, const void *r) {
    int x = *((int32 *) l);
    int y = *((int32 *) r);
    return (x - y);
}


std::ostream& operator<<(std::ostream& out, const MatchType value){
    static std::map<MatchType, std::string> strings;
    if (strings.size() == 0){
#define INSERT_ELEMENT(p) strings[p] = #p
        INSERT_ELEMENT(FORWARD);
        INSERT_ELEMENT(BACKWARD);
        INSERT_ELEMENT(MISMATCH_LEFT);
        INSERT_ELEMENT(MISMATCH_RIGHT);
        INSERT_ELEMENT(COVERED);
        INSERT_ELEMENT(COVERING);
        INSERT_ELEMENT(UNDEFINED);
        INSERT_ELEMENT(MIDDLE);
#undef INSERT_ELEMENT
    }

    return out << strings[value];
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}


std::string reverse_complement(std::string seq) {
    static std::map<char, char> m = {{'a','t'}, {'c','g'}, {'g','c'}, {'t','a'}, {'A','T'}, {'C','G'}, {'T','A'}, {'G','C'}, {'n','n'}, {'N', 'N'}, {'-', '-'}};
    std::reverse(seq.begin(), seq.end());
    for (int i = 0; i < seq.size(); i++) {
        seq[i] = m[seq[i]];
    }
    return seq;
}


std::string get_aligned_seq(std::string aln_tag1, std::string aln_tag2, int offset) {
    int pos = 0;
    int count = 0;
    while (count < offset) {
        if (aln_tag1[pos] != '-') count++;
        pos ++;
    }

    std::string str = "";

    for (int i = 0; i < pos; i++) {
        if (aln_tag2[i] != '-') str+=(aln_tag2[i]);
    }

    return str;
}


std::string get_aligned_seq_end(std::string aln_tag1, std::string aln_tag2, int offset) {
    int pos = 0;
    int count = 0;
    int len = aln_tag1.size() - 1;
    while (count < offset) {
        if (aln_tag1[len-pos] != '-') count++;
        pos ++;
    }

    std::string str = "";

    for (int i = 0; i < pos; i++) {
        if (aln_tag2[len-i] != '-') str+=(aln_tag2[len-i]);
    }


    std::reverse(str.begin(),str.end());
    return str;
}

std::vector<int> get_alignment_pos_list(std::string aln_tag1, std::string aln_tag2) {

    // pass

}


std::string get_aligned_seq_middle(std::string aln_tag1, std::string aln_tag2, int begin, int end) {
    int pos = 0;
    int count = 0;
    while ( count < begin) {
        if (aln_tag1[pos] != '-') count ++;
        pos ++;
    }

    std::string bstr = "";

    while (count < end) {
        if (aln_tag1[pos] != '-') count ++;
        if (aln_tag2[pos] != '-') bstr += aln_tag2[pos];
        pos ++;
    }

    return bstr;

}


std::vector<int> get_mapping(std::string aln_tag1, std::string aln_tag2) {
    int pos = 0;
    int count = 0;
    int count2 = 0;

    std::vector<int> ret;
    while (pos < aln_tag1.size()) {
        if (aln_tag1[pos] != '-') {
            ret.push_back(count2);
            count ++;
        }
        if (aln_tag2[pos] != '-') {
            count2 ++;
        }
        pos++;
    }
    return ret;
}



bool compare_overlap(LOverlap * ovl1, LOverlap * ovl2) {
    return ((ovl1->read_A_match_end_ - ovl1->read_A_match_start_ + ovl1->read_B_match_end_ - ovl1->read_B_match_start_) > (ovl2->read_A_match_end_ - ovl2->read_A_match_start_ + ovl2->read_B_match_end_ - ovl2->read_B_match_start_));
}

bool compare_sum_overlaps(const std::vector<LOverlap * > * ovl1, const std::vector<LOverlap *> * ovl2) {
    int sum1 = 0;
    int sum2 = 0;
    for (int i = 0; i < ovl1->size(); i++) sum1 += (*ovl1)[i]->read_A_match_end_ - (*ovl1)[i]->read_A_match_start_ + (*ovl1)[i]->read_B_match_end_ - (*ovl1)[i]->read_B_match_start_;
    for (int i = 0; i < ovl2->size(); i++) sum2 += (*ovl2)[i]->read_A_match_end_ - (*ovl2)[i]->read_A_match_start_ + (*ovl2)[i]->read_B_match_end_ - (*ovl2)[i]->read_B_match_start_;
    return sum1 > sum2;
}

bool compare_pos(LOverlap * ovl1, LOverlap * ovl2) {
    return (ovl1->read_A_match_start_) > (ovl2->read_A_match_start_);
}

bool compare_overlap_abpos(LOverlap * ovl1, LOverlap * ovl2) {
    return ovl1->read_A_match_start_ < ovl2->read_A_match_start_;
}

bool compare_overlap_aepos(LOverlap * ovl1, LOverlap * ovl2) {
    return ovl1->read_A_match_start_ > ovl2->read_A_match_start_;
}

std::vector<std::pair<int,int>> Merge(std::vector<LOverlap *> & intervals, int cutoff)
{
    //std::cout<<"Merge"<<std::endl;
    std::vector<std::pair<int, int > > ret;
    int n = intervals.size();
    if (n == 0) return ret;

    if(n == 1) {
        ret.push_back(std::pair<int,int>(intervals[0]->read_A_match_start_, intervals[0]->read_A_match_end_));
        return ret;
    }

    sort(intervals.begin(),intervals.end(),compare_overlap_abpos); //sort according to left

    int left= intervals[0]->read_A_match_start_ + cutoff, right = intervals[0]->read_A_match_end_ - cutoff; //left, right means maximal possible interval now

    for(int i = 1; i < n; i++)
    {
        if(intervals[i]->read_A_match_start_ + cutoff <= right)
        {
            right=std::max(right, intervals[i]->read_A_match_end_ - cutoff);
        }
        else
        {
            ret.push_back(std::pair<int, int>(left,right));
            left = intervals[i]->read_A_match_start_ + cutoff;
            right = intervals[i]->read_A_match_end_ - cutoff;
        }
    }
    ret.push_back(std::pair<int, int>(left,right));
    return ret;
}

Interval Effective_length(std::vector<LOverlap *> & intervals, int min_cov) {
    Interval ret;
    sort(intervals.begin(),intervals.end(),compare_overlap_abpos); //sort according to left

    if (intervals.size() > min_cov) {
        ret.first = intervals[min_cov]->read_A_match_start_;
    } else
        ret.first = 0;
    sort(intervals.begin(),intervals.end(),compare_overlap_aepos); //sort according to left
    if (intervals.size() > min_cov) {
        ret.second = intervals[min_cov]->read_A_match_end_;
    } else
        ret.second = 0;
    return ret;
}


int main(int argc, char *argv[]) {

    LAInterface la;
	char * name_db = argv[1];
	char * name_las = argv[2];
    char * name_input = argv[3];
	char * name_output = argv[4];
	char * name_config = argv[5];

	printf("name of db: %s, name of .las file %s\n", name_db, name_las);
    la.openDB(name_db);
	//la.openDB2(name_db); // changed this on Oct 12, may case problem, xf1280@gmail.com
    std::cout<<"# Reads:" << la.getReadNumber() << std::endl;
    la.openAlignmentFile(name_las);
    std::cout<<"# Alignments:" << la.getAlignmentNumber() << std::endl;
	//la.resetAlignment();
	//la.showOverlap(0,1);

	int n_aln = la.getAlignmentNumber();
	int n_read = la.getReadNumber();
    std::vector<LOverlap *> aln;
	la.resetAlignment();
    la.getOverlap(aln,0,n_aln);




    std::vector<Read *> reads;
    la.getRead(reads,0,n_read);

    std::cout << "input data finished" <<std::endl;
	/**
	filter reads
	**/

	/**
	 * Remove reads shorter than length threshold
	 */

    /*
     * load config
     */

    INIReader reader(name_config);

    if (reader.ParseError() < 0) {
        std::cout << "Can't load "<<name_config<<std::endl;
        return 1;
    }

    int MIN_COV2 = reader.GetInteger("draft", "min_cov", -1);
    int EDGE_TRIM = reader.GetInteger("draft", "trim", -1);
    int EDGE_SAFE = reader.GetInteger("draft", "edge_safe", -1);
    int TSPACE = reader.GetInteger("draft", "tspace", -1);
    int STEP = reader.GetInteger("draft", "step", -1);


    int LENGTH_THRESHOLD = reader.GetInteger("filter", "length_threshold", -1);
    double QUALITY_THRESHOLD = reader.GetReal("filter", "quality_threshold", 0.0);
    //int CHI_THRESHOLD = 500; // threshold for chimeric/adaptor at the begining
    int N_ITER = reader.GetInteger("filter", "n_iter", -1);
    int ALN_THRESHOLD = reader.GetInteger("filter", "aln_threshold", -1);
    int MIN_COV = reader.GetInteger("filter", "min_cov", -1);
    int CUT_OFF = reader.GetInteger("filter", "cut_off", -1);
    int THETA = reader.GetInteger("filter", "theta", -1);

	int N_PROC = reader.GetInteger("running", "n_proc", 4);

    omp_set_num_threads(N_PROC);

    //std::unordered_map<std::pair<int,int>, std::vector<LOverlap *> > idx; //unordered_map from (aid, bid) to alignments in a vector
    std::vector< std::vector<std::vector<LOverlap*>* > > idx2(n_read); //unordered_map from (aid) to alignments in a vector
    std::vector< Edge_w > edgelist; // save output to edgelist
    std::unordered_map<int, std::vector <LOverlap * > >idx3; // this is the pileup
    std::vector<std::set<int> > has_overlap(n_read);
    std::unordered_map<int, std::unordered_map<int, std::vector<LOverlap *> > > idx;



    for (int i = 0; i< n_read; i++) {
        //has_overlap[i] = std::set<int>();
        idx3[i] = std::vector<LOverlap *>();
    }

    //for (int i = 0; i < aln.size(); i++)
    //    if (aln[i]->active)
    //        idx[std::pair<int, int>(aln[i]->aid, aln[i]->bid)] = std::vector<LOverlap *>();
    for (int i = 0; i < aln.size(); i++) {
        if (aln[i]->active) {
            idx[aln[i]->read_A_id_][aln[i]->read_B_id_] = std::vector<LOverlap *>();
        }
    }


    for (int i = 0; i < aln.size(); i++) {
    if (aln[i]->active) {
	    has_overlap[aln[i]->read_A_id_].insert(aln[i]->read_B_id_);
        }
    }

    for (int i = 0; i < aln.size(); i++) {
        if (aln[i]->active) {
            idx3[aln[i]->read_A_id_].push_back(aln[i]);
        }
    }


    std::cout<<"add data"<<std::endl;
    for (int i = 0; i < aln.size(); i++) {
        if (aln[i]->active) {
            idx[aln[i]->read_A_id_][aln[i]->read_B_id_].push_back(aln[i]);
        }
    }
    std::cout<<"add data"<<std::endl;


    //sort each a,b pair according to read_A_match_start_:
    /*for (int i = 0; i < n_read; i++)
        for (std::set<int>::iterator j = has_overlap[i].begin(); j != has_overlap[i].end(); j++) {
            std::sort(idx[std::pair<int,int>(i, *j)].begin(), idx[std::pair<int,int>(i, *j)].end(), compare_pos);
        }
    */
    for (int i = 0; i < n_read; i++)
        for (std::set<int>::iterator j = has_overlap[i].begin(); j != has_overlap[i].end(); j++) {
            idx2[i].push_back(&(idx[i][*j]));
    }

    std::cout<<"add data"<<std::endl;
    std::unordered_map<int,std::vector<Interval> > covered_region;


    //for (int i = 0; i < n_read; i++) {
    //    printf("read %d [%d %d]/%d\n", i, reads[i]->effective_start, reads[i]->effective_end, reads[i]->len);
    //}

    /**Filtering
     *
     **/

    for (int n_iter = 0; n_iter < N_ITER; n_iter ++) {

#pragma omp parallel for
        for (int i = 0; i < n_read; i++) {
            if (reads[i]->active) {
                //covered_region[i] = Merge(idx3[i]);
                reads[i]->intervals = Merge(idx3[i], CUT_OFF);
                /*if (reads[i]->intervals.empty()) {
                    reads[i]->effective_start = 0;
                    reads[i]->effective_end = 0;
                } else {
                    reads[i]->effective_start = reads[i]->intervals.front().first;
                    reads[i]->effective_end = reads[i]->intervals.back().second;
                }*/
                Interval cov = Effective_length(idx3[i], MIN_COV);
                reads[i]->effective_start = cov.first;
                reads[i]->effective_end = cov.second;

            }
        } // find all covered regions, could help remove adaptors

        std::cout<<"covered region"<<std::endl;
# pragma omp parallel for
        for (int i = 0; i < n_read; i++) {
            if (reads[i]->active)
            if ((reads[i]->effective_end - reads[i]->effective_start <
                 LENGTH_THRESHOLD) /*or (reads[i]->len - reads[i]->effective_end > CHI_THRESHOLD) or (reads[i]->effective_start > CHI_THRESHOLD)*/
                or (reads[i]->intervals.size() != 1))
                reads[i]->active = false;
        } // filter according to effective length, and interval size

        std::cout<<"filter data"<<std::endl;

        int num_active = 0;

        for (int i = 0; i < n_read; i++) {
            if (reads[i]->active)
                num_active++;
        }
        std::cout << "num active reads " << num_active << std::endl;
# pragma omp parallel for
        for (int i = 0; i < n_aln; i++) {
            if (aln[i]->active)
            if ((not reads[aln[i]->read_A_id_]->active) or (not reads[aln[i]->read_B_id_]->active) or
                (aln[i]->diffs * 2 / float(aln[i]->read_B_match_end_ - aln[i]->read_B_match_start_ + aln[i]->read_A_match_end_ - aln[i]->read_A_match_start_) >
                 QUALITY_THRESHOLD) or (aln[i]->read_A_match_end_ - aln[i]->read_A_match_start_ < ALN_THRESHOLD))
                aln[i]->active = false;
        }

# pragma omp parallel for
        for (int i = 0; i < n_aln; i++) {
            if (aln[i]->active) {
                aln[i]->eff_read_A_start_ = reads[aln[i]->read_A_id_]->effective_start;
                aln[i]->eff_read_A_end_ = reads[aln[i]->read_A_id_]->effective_end;

				if (aln[i]->reverse_complement_match_ == 0) {
					aln[i]->eff_read_B_start_ = reads[aln[i]->read_B_id_]->effective_start;
                	aln[i]->eff_read_B_end_ = reads[aln[i]->read_B_id_]->effective_end;
				}
				else {
					aln[i]->eff_read_B_start_ = aln[i]->blen - reads[aln[i]->read_B_id_]->effective_end;
                	aln[i]->eff_read_B_end_ = aln[i]->blen - reads[aln[i]->read_B_id_]->effective_start;
				}
            }
        }

        int num_active_aln = 0;
# pragma omp parallel for reduction(+:num_active_aln)
        for (int i = 0; i < n_aln; i++) {
            if (aln[i]->active) {
                num_active_aln++;
                aln[i]->addtype(THETA);
            }
        }
        std::cout << "num active alignments " << num_active_aln << std::endl;

        idx3.clear();
        for (int i = 0; i < aln.size(); i++) {
            if (aln[i]->active) {
                idx3[aln[i]->read_A_id_].push_back(aln[i]);
            }
        }
    }


# pragma omp parallel for
    for (int i = 0; i < n_read; i++ ) {
        std::sort( idx2[i].begin(), idx2[i].end(), compare_sum_overlaps );
    }


    for (int n_iter = 0; n_iter < 3; n_iter ++) {
        int no_edge = 0;
        edgelist.clear();
        for (int i = 0; i < n_read; i++) {
            int cf = 0;
            int cb = 0;
            /*
             * For each read, if there is exact right match (type FORWARD), choose the one with longest alignment with read A
             * same for BACKWARD,
             */

            if (reads[i]->active)
                for (int j = 0; j < idx2[i].size(); j++) {
                    //idx2[i][j]->show();
                    if ((*idx2[i][j])[0]->active) {

						//std::sort( idx2[i][j]->begin(), idx2[i][j]->end(), compare_overlap );

                        for (int kk = 0; kk < idx2[i][j]->size(); kk++) {
                            if (reads[(*idx2[i][j])[kk]->read_B_id_]->active)
                            if (((*idx2[i][j])[kk]->match_type_ == FORWARD) and (cf < 1)) {
                                cf += 1;
                                //add edge
                                /*if ((*idx2[i][j])[kk]->reverse_complement_match_ == 1) { // n = 0, c = 1
                                    edgelist.push_back(
                                            std::pair<Node, Node>(Node((*idx2[i][j])[kk]->aid, 0),
                                                                  Node((*idx2[i][j])[kk]->bid, 1)));
                                } else {
                                    edgelist.push_back(
                                            std::pair<Node, Node>(Node((*idx2[i][j])[kk]->aid, 0),
                                                                  Node((*idx2[i][j])[kk]->bid, 0)));
                                }*/
                            }
                            if (reads[(*idx2[i][j])[kk]->read_B_id_]->active)
                            if (((*idx2[i][j])[kk]->match_type_ == BACKWARD) and (cb < 1)) {
                                cb += 1;
                                //add edge
                                /*if ((*idx2[i][j])[kk]->reverse_complement_match_ == 1) {
                                    edgelist.push_back(
                                            std::pair<Node, Node>(Node((*idx2[i][j])[kk]->aid, 1),
                                                                  Node((*idx2[i][j])[kk]->bid, 0)));
                                } else {
                                    edgelist.push_back(
                                            std::pair<Node, Node>(Node((*idx2[i][j])[kk]->aid, 1),
                                                                  Node((*idx2[i][j])[kk]->bid, 1)));
                                }*/
                            }
                            if ((cf >= 1) and (cb >= 1)) break;
                        }
                    }
                    if ((cf >= 1) and (cb >= 1)) break;
                }

                if (reads[i]->active) {
                    if (cf == 0) printf("%d has no out-going edges\n", i);
                    if (cb == 0) printf("%d has no in-coming edges\n", i);
                    if ((cf == 0) or (cb ==0)) {
                        no_edge++;
                        reads[i]->active = false;
                    }
                }

            }

            printf("no_edge nodes:%d\n",no_edge);
		}



# pragma omp parallel for
            for (int ii = 0; ii < n_aln; ii++) {
            if (aln[ii]->active)
            if ((not reads[aln[ii]->read_A_id_]->active) or (not reads[aln[ii]->read_B_id_]->active))
                aln[ii]->active = false;
            }

            int num_active_aln = 0;
# pragma omp parallel for reduction(+:num_active_aln)
	for (int i = 0; i < n_aln; i++) {
	    if (aln[i]->active) {
	        num_active_aln++;
	        aln[i]->addtype(THETA);
	    }
	}
	std::cout << "num active alignments " << num_active_aln << std::endl;


	edgelist.clear();
    std::string edge_line;
    std::ifstream edges_file(name_input);
    while (!edges_file.eof()) {
        std::getline(edges_file,edge_line);
        std::vector<std::string> tokens = split(edge_line, ' ');
        Node node0;
        Node node1;
        int w;
        if (tokens.size() == 3) {
            if (tokens[0].back() == '\'') {
                node0.id = std::stoi(tokens[0].substr(0, tokens[0].size() - 1));
                node0.strand = 1;
            } else {
                node0.id = std::stoi(tokens[0]);
                node0.strand = 0;
            }

            if (tokens[1].back() == '\'') {
                node1.id = std::stoi(tokens[1].substr(0, tokens[1].size() - 1));
                node1.strand = 1;
            } else {
                node1.id = std::stoi(tokens[1]);
                node1.strand = 0;


            }
            w = std::stoi(tokens[2]);
            edgelist.push_back(std::make_tuple(node0,node1,w));
        }
    }

    //edgelist.push_back(std::pair<Node, Node>(edgelist.back().second, edgelist.front().first));

	//for (int i = 0; i < edgelist.size(); i++) {
	//	printf("%d->%d\n", std::get<0>(edgelist[i]), std::get<1>(edgelist[i]));
	//}
/* test falcon consensus, now we don't need them

    for (int i = 0; i < n_read; i ++ ) {
        std::transform(reads[i]->bases.begin(), reads[i]->bases.end(),reads[i]->bases.begin(), ::toupper);
    }

    std::vector<std::string> sequence_list;


    const int top_k = 400;
    //choose top 400 for consensus

    for (int i = 0; i < edgelist.size(); i++){
        int nactive = 0;
        int total = 0;
        for (int kk = 0; kk < idx3[std::get<0>(edgelist[i]).id].size(); kk++) {
            total ++;
            if (idx3[std::get<0>(edgelist[i]).id][kk]->active)
                nactive ++;
        }
        std::sort(idx3[i].begin(), idx3[i].end(), compare_overlap);
        //printf("id: %d, num pileup: %d/%d\n", std::get<0>(edgelist[i]).id, nactive, total);

        int seq_count = nactive;
        char small_buffer[1024];
        char big_buffer[65536];
        char ** input_seq;
        //char ** seq_id;
        consensus_data * consensus;

        input_seq = (char **)calloc( 501, sizeof(char *));
        //seq_id = (char **)calloc( 501, sizeof(char *));

        input_seq[0] = (char *)calloc( 100000 , sizeof(char));

		int astart = reads[idx3[std::get<0>(edgelist[i]).id][0]->aid]->effective_start;
		int aend = reads[idx3[std::get<0>(edgelist[i]).id][0]->aid]->effective_end;

        //strcpy(input_seq[0], reads[idx3[std::get<0>(edgelist[i]).id][0]->aid]->bases.substr(astart, aend-astart).c_str());

		strcpy(input_seq[0], reads[idx3[std::get<0>(edgelist[i]).id][0]->aid]->bases.c_str());

        //feed input seq in
        int num_chosen = 1;
        int num_passed = 1;

        while ((num_chosen < top_k) and (num_passed < idx3[std::get<0>(edgelist[i]).id].size())) {

            if (idx3[std::get<0>(edgelist[i]).id][num_passed]->active) {
                //put it in
                int start = idx3[std::get<0>(edgelist[i]).id][num_passed]->read_B_match_start_;
                int end = idx3[std::get<0>(edgelist[i]).id][num_passed]->read_B_match_end_;
                std::string bsub = reads[idx3[std::get<0>(edgelist[i]).id][num_passed]->bid]->bases;
                input_seq[num_chosen] = (char *)calloc( 100000 , sizeof(char));
                if (idx3[std::get<0>(edgelist[i]).id][num_passed]->reverse_complement_match_ == 0)
                    strcpy(input_seq[num_chosen], bsub.substr(start, end-start).c_str());
                else
                    strcpy(input_seq[num_chosen], reverse_complement(bsub.substr(start, end-start)).c_str());
                num_chosen ++;
            }
            num_passed ++;
        }

        seq_count = num_chosen;

        //printf("%d\n",seq_count);
        consensus = generateConsensus(input_seq, seq_count, 8, 8, 12, 6, 0.56); // generate consensus for each read


        if (std::get<0>(edgelist[i]).strand == 0 ) {
			sequence_list.push_back(std::string(consensus->sequence));
			printf(">%d_%d_%d\n %s\n", std::get<0>(edgelist[i]).id, num_chosen, strlen(consensus->sequence), std::string(consensus->sequence).c_str());
		} else {
            sequence_list.push_back(reverse_complement(std::string(consensus->sequence)));
			printf(">%d_%d_%d\n %s\n", std::get<0>(edgelist[i]).id, num_chosen, strlen(consensus->sequence), reverse_complement(std::string(consensus->sequence)).c_str());
		}

        free_consensus_data(consensus);
        for (int jj=0; jj < seq_count; jj++) {
            //free(seq_id[jj]);
            free(input_seq[jj]);
        };

    }

*/



    std::vector<LAlignment *> full_alns;
    std::vector <LAlignment *> selected;
    std::unordered_map<int, std::vector<LAlignment *>> idx_aln;
    la.resetAlignment();
    std::vector<int> range;

    for (int i = 0; i < edgelist.size(); i++) {
        range.push_back(std::get<0>(edgelist[i]).id);
        idx_aln[std::get<0>(edgelist[i]).id] = std::vector<LAlignment *> ();
    }

    std::sort(range.begin(), range.end());


    la.getAlignment(full_alns, range);

    for (auto i:full_alns) {
        idx_aln[i->read_A_id_].push_back(i);
    }

    for (int i = 0; i < edgelist.size(); i++) {
        int aid = std::get<0>(edgelist[i]).id;
        int bid = std::get<1>(edgelist[i]).id;
        bool found = false;
        for (int j = 0; j < idx_aln[std::get<0>(edgelist[i]).id].size(); j++) {
            //printf("%d %d %d %d\n",bid, idx_aln[aid][j]->bid, idx_aln[aid][j]->read_A_match_end_ - idx_aln[aid][j]->read_A_match_start_, std::get<2>(edgelist[i]));
            if ((idx_aln[aid][j]->read_B_id_ == bid) and \
            (idx_aln[aid][j]->aepos - idx_aln[aid][j]->abpos == std::get<2>(edgelist[i]))) {
                selected.push_back(idx_aln[aid][j]);
                found = true;
                break;
            }
            if (found) continue;
        }
    }



    std::unordered_map<int, std::unordered_map<int, std::pair<std::string, std::string> > > aln_tags_map;
    std::vector<std::pair<std::string, std::string> > aln_tags_list;
    std::vector<std::pair<std::string, std::string> > aln_tags_list_true_strand;



    for (int i = 0; i < selected.size(); i++) {
        la.recoverAlignment(selected[i]);
        //printf("%d %d\n",selected[i]->tlen, selected[i]->trace_pts_len);
        std::pair<std::string, std::string> res = la.getAlignmentTags(selected[i]);
        aln_tags_map[selected[i]->read_A_id_][selected[i]->read_B_id_] = res;
        aln_tags_list.push_back(res);
    }



    std::ofstream out(name_output);
    std::string sequence = "";

    std::vector<LOverlap *> bedges;
    std::vector<std::string> breads;

    std::vector<std::vector<std::pair<int, int> > > pitfalls;


    range.clear();
    for (int i = 0; i < edgelist.size(); i++) {
        range.push_back(std::get<0>(edgelist[i]).id);
    }

    std::vector<std::vector<int> *> coverages;

    for (int i = 0; i < range.size(); i++) {
        int aread = range[i];
        if (idx3[aread].size() > 0) {
            std::vector<int> * res = la.getCoverage(idx3[aread]);
            std::vector<std::pair<int, int> > * res2 = la.lowCoverageRegions(*res, MIN_COV2);
            //delete res;
            coverages.push_back(res);
            //printf("%d %d: (%d %d) ", i, aread, 0, idx3[aread][0]->alen);
            //for (int j = 0; j < res2->size(); j++) {
            //    printf("[%d %d] ", res2->at(j).first, res2->at(j).second);
            //}
            //printf("\n");
            pitfalls.push_back(*res2);
            delete res2;
        }
    }


    /***
     * Prepare the data
     */

    for (int i = 0; i < edgelist.size(); i++){

		std::vector<LOverlap *> currentalns = idx[std::get<0>(edgelist[i]).id][std::get<1>(edgelist[i]).id];

    	LOverlap * currentaln = NULL;

		for (int j = 0; j < currentalns.size(); j++) {
    		//std::cout << std::get<0>(edgelist[i]).id << " " << std::get<1>(edgelist[i]).id << " " << currentalns[j]->match_type_ << std::endl;
    		if (currentalns[j]->read_A_match_end_ - currentalns[j]->read_A_match_start_ == std::get<2>(edgelist[i]) ) currentaln = currentalns[j];
		}

		if (currentaln == NULL) exit(1);
		//currentaln->show();

        std::string current_seq;
		std::string next_seq;

        std::string aln_tags1;
        std::string aln_tags2;


        if (std::get<0>(edgelist[i]).strand == 0)
            current_seq = reads[std::get<0>(edgelist[i]).id]->bases;
        else
            current_seq = reverse_complement(reads[std::get<0>(edgelist[i]).id]->bases);

        if (std::get<0>(edgelist[i]).strand == 0) {
            aln_tags1 = aln_tags_list[i].first;
            aln_tags2 = aln_tags_list[i].second;
        } else {
            aln_tags1 = reverse_complement(aln_tags_list[i].first);
            aln_tags2 = reverse_complement(aln_tags_list[i].second);
        }

        aln_tags_list_true_strand.push_back(std::pair<std::string, std::string>(aln_tags1, aln_tags2));

        if (std::get<1>(edgelist[i]).strand == 0)
            next_seq = reads[std::get<1>(edgelist[i]).id]->bases;
        else
            next_seq = reverse_complement(reads[std::get<1>(edgelist[i]).id]->bases);

		int abpos, aepos, alen, bbpos, bepos, blen, aes, aee, bes, bee;

		alen = currentaln->alen;
		blen = currentaln->blen;

		if (std::get<0>(edgelist[i]).strand == 0) {
			abpos = currentaln->read_A_match_start_;
			aepos = currentaln->read_A_match_end_;

			aes = currentaln->eff_read_A_start_;
			aee = currentaln->eff_read_A_end_;

		} else {
			abpos = alen - currentaln->read_A_match_end_;
			aepos = alen - currentaln->read_A_match_start_;

			aes = alen - currentaln->eff_read_A_end_;
			aee = alen - currentaln->eff_read_A_start_;
		}

		if (((std::get<1>(edgelist[i]).strand == 0) and (currentaln->reverse_complement_match_ == 0)) or ((std::get<1>(edgelist[i]).strand == 1) and (currentaln->reverse_complement_match_ == 1))) {
			bbpos = currentaln->read_B_match_start_;
			bepos = currentaln->read_B_match_end_;

			bes = currentaln->eff_read_B_start_;
			bee = currentaln->eff_read_B_end_;

		} else {
			bbpos = blen - currentaln->read_B_match_end_;
			bepos = blen - currentaln->read_B_match_start_;

			bes = blen - currentaln->eff_read_B_end_;
			bee = blen - currentaln->eff_read_B_start_;

		}

		//printf("[[%d %d] << [%d %d]] x [[%d %d] << [%d %d]]\n", read_A_match_start_, read_A_match_end_, aes, aee, read_B_match_start_, read_B_match_end_, bes, bee);

        LOverlap *new_ovl = new LOverlap();
        new_ovl->read_A_match_start_ = abpos;
        new_ovl->read_A_match_end_ = aepos;
        new_ovl->read_B_match_start_ = bbpos;
        new_ovl->read_B_match_end_ = bepos;
        new_ovl->eff_read_A_end_ = aee;
        new_ovl->eff_read_A_start_ = aes;
        new_ovl->eff_read_B_end_ = bee;
        new_ovl->eff_read_B_start_ = bes;
        new_ovl->alen = currentaln->alen;
        new_ovl->blen = currentaln->blen;
        new_ovl->read_A_id_ = std::get<0>(edgelist[i]).id;
        new_ovl->read_B_id_ = std::get<1>(edgelist[i]).id;


        bedges.push_back(new_ovl);
        breads.push_back(current_seq);


        /*for (int j = 0; j < pitfalls[i+1].size(); j++)
            if ((pitfalls[i+1][j].first > read_B_match_end_ ) and ( pitfalls[i+1][j].second<blen)) {
                //printf("read %d:", range[i+1]);
                //printf("[%d %d]\n", pitfalls[i + 1][j].first, pitfalls[i + 1][j].second);
                //fix the pit fall
                //next_trim = blen - pitfalls[i + 1][j].first;
                //printf("trim:%d\n", next_trim);
                }

        */

        //printf("%d,%s\n", str2.size(), str2.c_str());
        //printf("ref:%s\n", next_seq.substr(read_B_match_end_ - str2.size(), str2.size()).c_str());


        //filling the pit holes !!
        //show all the gaps in [read_B_match_end_ <--> blen]
        /*printf("read: %d ", range[i+1]);
        for (int j = 0; j < pitfalls[i+1].size(); j++)
            printf("[%d %d] ", pitfalls[i+1][j].first, pitfalls[i+1][j].second);
        printf("read a %d b %d\n",std::get<0>(edgelist[i]).id,std::get<1>(edgelist[i]).id);
        printf("alen %d read_B_match_end_: %d blen: %d\n", alen, read_B_match_end_, blen);
        printf("\n");*/




	}
    //need to trim the end


 /*   sequence = breads[0];
    int next_trim = 0;

    for (int i = 0; i < range.size()-1; i++) {
        std::string this_seq = breads[i];
        std::string next_seq = breads[i + 1];
        LOverlap * this_alignment = bedges[i];
        LOverlap * next_alignment = bedges[i+1];
        std::string aln_tag1 = aln_tags_list_true_strand[i].first;
        std::string aln_tag2 = aln_tags_list_true_strand[i].second;

        std::string next_aln_tag1 = aln_tags_list_true_strand[i+1].first;
        std::string next_aln_tag2 = aln_tags_list_true_strand[i+1].second;



        printf("%d %d %d %d %d %d %d\n",this_alignment->aid, this_alignment->bid, next_alignment->aid, this_alignment->alen, this_alignment->blen, this_seq.size(), next_seq.size());




        int trim = EDGE_TRIM;
        for (int j = 0; j < pitfalls[i+1].size(); j++)
            if ((pitfalls[i+1][j].first > this_alignment->read_B_match_end_ ) and ( pitfalls[i+1][j].second<this_alignment->blen)) {
                printf("read %d: the pitfall is:", range[i+1]);
                printf("[%d %d]\n", pitfalls[i + 1][j].first, pitfalls[i + 1][j].second);
                //fix the pit fall
                //next_trim = next_alignment->read_A_match_end_ - pitfalls[i + 1][j].first;
                //printf("longer next trim:%d\n", next_trim);

                if (pitfalls[i + 1][j].first > next_alignment->read_A_match_end_)
                    printf("fine, this will be removed in next alignment\n");
                else {
                    std::string fix = get_aligned_seq_middle(next_aln_tag1, next_aln_tag2,
                    pitfalls[i + 1][j].first-next_alignment->read_A_match_start_ - 20 , pitfalls[i + 1][j].second - next_alignment->read_A_match_start_ + 20);
                    printf("fix will be:%s\n",fix.c_str());

					if (fix.size() > 0) {
                    	next_seq.erase(pitfalls[i + 1][j].first-20, pitfalls[i + 1][j].second + 40 - pitfalls[i + 1][j].first);
                    	next_seq.insert(pitfalls[i + 1][j].first-20,fix);
					}
                     //fix this pitfall;
                }

            }


        std::string str2 = get_aligned_seq_end(aln_tag1, aln_tag2, trim);

        //printf("%d,%s\n", str2.size(), str2.c_str());
        //printf("ref:%s\n", next_seq.substr(this_alignment->read_B_match_end_ - str2.size(), str2.size()).c_str());

        sequence.erase(sequence.end() - (this_alignment->alen - this_alignment->read_A_match_end_), sequence.end());
        sequence.erase(sequence.end() - trim, sequence.end());
        next_seq.erase(next_seq.begin(), next_seq.begin() + this_alignment->read_B_match_end_ - str2.size());
        sequence += next_seq;



    }
*/

    std::vector<std::vector<int> > mappings;
    for (int i = 0; i < range.size(); i++) {
        mappings.push_back(get_mapping(aln_tags_list_true_strand[i].first, aln_tags_list_true_strand[i].second));
    }

    std::cout << bedges.size() << " " << breads.size() << " " << selected.size() << " "
    << aln_tags_list.size() << " " << pitfalls.size() << " " << aln_tags_list_true_strand.size()
    << " " << mappings.size() << " " << coverages.size() <<  std::endl;

    /*for (int i = 0; i < bedges.size() - 1; i++) {
        printf("%d %d %d %d %d\n", bedges[i]->read_B_match_start_, bedges[i]->read_B_match_end_, bedges[i+1]->read_A_match_start_, bedges[i+1]->read_A_match_end_, bedges[i]->read_B_match_end_ - bedges[i+1]->read_A_match_start_);
    }*/


    int tspace = TSPACE; // set lane length to be 500
    int nlane = 0;


    printf("%d %d\n", mappings[0][800], mappings[0][1000]); // debug output
    printf("%s\n%s\n", breads[0].substr(bedges[0]->read_A_match_start_ + 800, 50).c_str(), breads[1].substr(bedges[0]->read_B_match_start_ + mappings[0][800], 50).c_str() ); //debug output


    std::vector<std::vector<std::pair<int, int>>> lanes;

    std::string draft_assembly = "";


    int currentlane = 0;
    int current_starting_read = 0;
    int current_starting_space = 1;
    int current_starting_offset = 0;
    int n_bb_reads = range.size();
    std::vector<std::vector<int>> trace_pts(n_bb_reads);
    bool revert = false;


    int rmax = -1;
    /**
     * Move forward and put "trace points"
     */
    while (current_starting_read < n_bb_reads-1) {
        int currentread = current_starting_read;
        int additional_offset = 0;
        while (bedges[current_starting_read]->read_A_match_start_ + current_starting_space * tspace + current_starting_offset + additional_offset < bedges[current_starting_read]->read_A_match_end_ - EDGE_SAFE) {
            int waypoint = bedges[current_starting_read]->read_A_match_start_ + tspace * current_starting_space + current_starting_offset + additional_offset;
            //if ((waypoint - bedges[current_starting_read]->read_A_match_start_) < EDGE_SAFE)
            //    waypoint += EDGE_SAFE;

            //int next_waypoint = mappings[currentread][waypoint - bedges[current_starting_read]->read_A_match_start_] + bedges[current_starting_read]->read_B_match_start_;
            std::vector<std::pair<int,int> > lane;

            while ((waypoint > bedges[currentread]->read_A_match_start_) and (waypoint < bedges[ currentread ]->read_A_match_end_)) {

                printf("%d %d\n",currentread, waypoint);
                trace_pts[currentread].push_back(waypoint);


                /*if (waypoint > bedges[currentread]->read_A_match_end_ - EDGE_SAFE) {
                    printf("Reaching the end, neglect low coverage\n");
                }

                if ((coverages[currentread]->at(waypoint) < MIN_COV2) and (waypoint < bedges[currentread]->read_A_match_end_ - EDGE_SAFE)) {
                    revert = true;
                    printf("Low coverage, revert\n");
                    break;
                }*/


                lane.push_back(std::pair<int,int>(currentread, waypoint));
                if (currentread>rmax) rmax = currentread;
                //int previous_wp = waypoint;
                waypoint  = mappings[currentread][waypoint - bedges[currentread]->read_A_match_start_] + bedges[currentread]->read_B_match_start_;
                //printf("%s\n%s\n", breads[currentread].substr(previous_wp,50).c_str(), breads[currentread+1].substr(waypoint,50).c_str());
                currentread ++;
                if (currentread >= n_bb_reads) break;
            }
            if (currentread < n_bb_reads)
            if (waypoint <  bedges[currentread]->alen) {
                lane.push_back(std::pair<int, int>(currentread, waypoint));
                if (currentread>rmax) rmax = currentread;
            }
            /*if (revert) {
                printf("revert\n");
                revert = false;
                while (currentread >= current_starting_read) {
                    trace_pts[currentread].pop_back();
                    currentread --;
                    additional_offset += STEP;
                }
                currentread = current_starting_read;
            }
            else*/
            {
                if (currentread >= rmax)
                    lanes.push_back(lane);
                current_starting_space ++;
                currentread = current_starting_read;

            }

        }

        current_starting_read ++;
        current_starting_space = 1;//get next space;
        if (trace_pts[current_starting_read].size() == 0)
            current_starting_offset = 0;
        else
            current_starting_offset = trace_pts[current_starting_read].back() - bedges[current_starting_read]->read_A_match_start_;
    }


    /**
     * Show trace points on reads
     */
    for (int i = 0; i < n_bb_reads; i++) {
        printf("Read %d:", i);
        for (int j = 0; j < trace_pts[i].size(); j++) {
            printf("%d ", trace_pts[i][j]);
        }
        printf("\n");
    }

    /**
     * Show lanes
     */

    for (int i = 0; i < lanes.size(); i++) {

        printf("Lane %d\n",i);
        for (int j = 0; j < lanes[i].size(); j++) {
            printf("[%d %d] ", lanes[i][j].first, lanes[i][j].second);
        }
        printf("\n");
    }


    printf("In total %d lanes\n", lanes.size());


    /**
     * Consequtive lanes form a column (ladder)
     */

    std::vector<std::vector<std::tuple<int, int, int> > > ladders;

    for (int i = 0; i < lanes.size() - 1; i++) {
        std::vector<std::pair<int,int> > lane1 = lanes[i];
        std::vector<std::pair<int,int> > lane2 = lanes[i+1];
        std::vector<std::tuple<int, int, int> > ladder;
        int pos = 0;
        for (int j = 0; j < lane2.size(); j++) {
            while ((lane1[pos].first!=lane2[j].first) and (pos < lane1.size()-1)) pos ++;
            if ((lane1[pos].first == lane2[j].first)) ladder.push_back(std::make_tuple(lane2[j].first, lane1[pos].second, lane2[j].second));
        }
        ladders.push_back(ladder);
    }


    /**
     * show ladders
     */
    for (int i = 0; i < ladders.size(); i++) {
        printf("Ladder %d\n",i);
        for (int j = 0; j < ladders[i].size(); j++) {
            //printf("[%d %d-%d] ", std::get<0>(ladders[i][j]), std::get<1>(ladders[i][j]), std::get<2>(ladders[i][j]) );
            //printf("%s\n", breads[std::get<0>(ladders[i][j])].substr(std::get<1>(ladders[i][j]),std::get<2>(ladders[i][j])-std::get<1>(ladders[i][j])).c_str());

        }

        if (ladders[i].size() == 0) {
            printf("low coverage!\n");
            continue;
        }

        if (ladders[i].size() > 1) {


            int mx = 0;
            int maxcoverage = 0;
            for (int j = 0; j < ladders[i].size(); j++) {
                int mincoverage = 10000;
                int read = std::get<0>(ladders[i][j]);
                int start = std::get<1>(ladders[i][j]);
                int end = std::get<2>(ladders[i][j]);
                for (int pos = start; pos < end; pos ++) {
                    if (coverages[read]->at(pos) < mincoverage) mincoverage = coverages[read]->at(pos);
                }
                if (mincoverage > maxcoverage) {
                    maxcoverage = mincoverage;
                    mx = j;
                }
            }

           std::cout << "ladder " << i << " num reads "<< ladders[i].size() << " possibly error here " << maxcoverage << "\n!";


			if (ladders[i].size() == 2) {
				draft_assembly +=  breads[std::get<0>(ladders[i][mx])].substr(std::get<1>(ladders[i][mx]),std::get<2>(ladders[i][mx])-std::get<1>(ladders[i][mx]));
				continue;
			}


            std::string base = breads[std::get<0>(ladders[i][mx])].substr(std::get<1>(ladders[i][mx]),std::get<2>(ladders[i][mx])-std::get<1>(ladders[i][mx]));;
            int seq_count = ladders[i].size();
            printf("seq_count:%d, max %d\n",seq_count,mx);
            align_tags_t ** tags_list;
            tags_list = (align_tags_t **) calloc( seq_count, sizeof(align_tags_t *) );
            consensus_data * consensus;

            int alen = (std::get<2>(ladders[i][mx])-std::get<1>(ladders[i][mx]));
            for (int j = 0; j < ladders[i].size(); j++) {

                int blen = (std::get<2>(ladders[i][j])-std::get<1>(ladders[i][j]));
                char * aseq = (char *) malloc( (20+ (std::get<2>(ladders[i][mx])-std::get<1>(ladders[i][mx]))) * sizeof(char) );
                char * bseq = (char *) malloc( (20 + (std::get<2>(ladders[i][j])-std::get<1>(ladders[i][j]))) * sizeof(char) );
                strcpy(aseq, breads[std::get<0>(ladders[i][mx])].substr(std::get<1>(ladders[i][mx]),std::get<2>(ladders[i][mx])-std::get<1>(ladders[i][mx])).c_str());
                strcpy(bseq, breads[std::get<0>(ladders[i][j])].substr(std::get<1>(ladders[i][j]),std::get<2>(ladders[i][j])-std::get<1>(ladders[i][j])).c_str());


                aln_range * arange = (aln_range*) calloc(1 , sizeof(aln_range));
                arange->s1 = 0;
                arange->e1 = strlen(bseq);
                arange->s2 = 0;
                arange->e2 = strlen(aseq);
                arange->score = 5;

                //printf("blen %d alen%d\n",strlen(bseq), strlen(aseq));
                //printf("before get tags\n");

                alignment * alng = align(bseq, blen , aseq, alen , 150, 1);

                char * q_aln_str = (char * )malloc((5+strlen(alng->q_aln_str))*sizeof(char)) ;
                char * t_aln_str = (char * )malloc((5+strlen(alng->t_aln_str))*sizeof(char));


                strcpy(q_aln_str + 1, alng->q_aln_str);
                strcpy(t_aln_str + 1, alng->t_aln_str);
                q_aln_str[0] = 'T';
                t_aln_str[0] = 'T';


                for (int pos = 0; pos < strlen(q_aln_str); pos++) q_aln_str[pos] = toupper(q_aln_str[pos]);
                for (int pos = 0; pos < strlen(t_aln_str); pos++) t_aln_str[pos] = toupper(t_aln_str[pos]);

                //printf("Q:%s\nT:%s\n", q_aln_str, t_aln_str);

                tags_list[j] = get_align_tags(  q_aln_str,
                                                t_aln_str,
                                                 strlen(alng->q_aln_str) + 1,
                                                 arange, (unsigned int)j, 0 );
                //free(aseq);
                //free(bseq);

                /*for (int k = 0; k < tags_list[j]->len; k++) {
                    printf("%d %d %ld %d %c %c\n",j, k, tags_list[j]->align_tags[k].t_pos,
                           tags_list[j]->align_tags[k].delta,
                            //tags_list[j]->align_tags[k].p_q_base,
                           aseq[tags_list[j]->align_tags[k].t_pos],
                           tags_list[j]->align_tags[k].q_base);
                }*/
                free(q_aln_str);
                free(t_aln_str);
                free(aseq);
                free(bseq);
                free_alignment(alng);

            }

            //printf("%d %d\n%s\n",seq_count, strlen(seq), seq);

            consensus = get_cns_from_align_tags( tags_list, seq_count, alen+1, 1 );
            //printf("Consensus:%s\n",consensus->sequence);
            draft_assembly += std::string(consensus->sequence);

            free_consensus_data(consensus);
            for (int j = 0; j < seq_count; j++)
                free_align_tags(tags_list[j]);

        }  else {
            draft_assembly +=  breads[std::get<0>(ladders[i][0])].substr(std::get<1>(ladders[i][0]),std::get<2>(ladders[i][0])-std::get<1>(ladders[i][0]));
        }

        printf("\n");
    }



    /*for (int i = 0; i < mapping.size(); i++)
        printf("%d %d\n", i, mapping[i]);
    printf("[%d %d], [%d %d]\n", bedges[0]->read_A_match_start_, bedges[0]->read_A_match_end_, bedges[0]->read_B_match_start_, bedges[0]->read_B_match_end_);*/

    std::cout<<sequence.size()<<std::endl;
    std::cout<<draft_assembly.size()<<std::endl;


	out << ">Draft_assembly\n";
	out << draft_assembly << std::endl;

    la.closeDB(); //close database
    return 0;
}
