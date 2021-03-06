
/*
 * =====================================================================================
 *
 *       Filename:  common.h
 *
 *    Description:  Common delclaration for the code base 
 *
 *        Version:  0.1
 *        Created:  07/16/2013 07:46:23 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Jason Chin, 
 *        Company:  
 *
 * =====================================================================================

 #################################################################################$$
 # Copyright (c) 2011-2014, Pacific Biosciences of California, Inc.
 #
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted (subject to the limitations in the
 # disclaimer below) provided that the following conditions are met:
 #
 #  * Redistributions of source code must retain the above copyright
 #  notice, this list of conditions and the following disclaimer.
 #
 #  * Redistributions in binary form must reproduce the above
 #  copyright notice, this list of conditions and the following
 #  disclaimer in the documentation and/or other materials provided
 #  with the distribution.
 #
 #  * Neither the name of Pacific Biosciences nor the names of its
 #  contributors may be used to endorse or promote products derived
 #  from this software without specific prior written permission.
 #
 # NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 # GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY PACIFIC
 # BIOSCIENCES AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 # WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 # OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 # DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR ITS
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 # SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 # LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 # USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 # ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 # OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 # OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 # SUCH DAMAGE.
 #################################################################################$$
 */

#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>


typedef int seq_coor_t; 

typedef struct {    
    seq_coor_t aln_str_size ;
    seq_coor_t dist ;
    seq_coor_t aln_q_s;
    seq_coor_t aln_q_e;
    seq_coor_t aln_t_s;
    seq_coor_t aln_t_e;
    char * q_aln_str;
    char * t_aln_str;

} alignment;


typedef struct {
    seq_coor_t pre_k;
    seq_coor_t x1;
    seq_coor_t y1;
    seq_coor_t x2;
    seq_coor_t y2;
} d_path_data;

typedef struct {
    seq_coor_t d;
    seq_coor_t k;
    seq_coor_t pre_k;
    seq_coor_t x1;
    seq_coor_t y1;
    seq_coor_t x2;
    seq_coor_t y2;
} d_path_data2;

typedef struct {
    seq_coor_t x;
    seq_coor_t y;
} path_point;

typedef struct {    
    seq_coor_t start;
    seq_coor_t last;
    seq_coor_t count;
} kmer_lookup;

typedef unsigned char base;
typedef base * seq_array;
typedef seq_coor_t seq_addr;
typedef seq_addr * seq_addr_array;


typedef struct {
    seq_coor_t count;
    seq_coor_t * query_pos;
    seq_coor_t * target_pos;
} kmer_match;


typedef struct {
    seq_coor_t s1;
    seq_coor_t e1;
    seq_coor_t s2;
    seq_coor_t e2;
    long int score;
} aln_range;


typedef struct {
    char * sequence;
    int * eqv;
} consensus_data;

kmer_lookup * allocate_kmer_lookup (seq_coor_t);
void init_kmer_lookup ( kmer_lookup *,  seq_coor_t );
void free_kmer_lookup(kmer_lookup *);

seq_array allocate_seq(seq_coor_t);
void init_seq_array( seq_array, seq_coor_t);
void free_seq_array(seq_array);

seq_addr_array allocate_seq_addr(seq_coor_t size); 

void free_seq_addr_array(seq_addr_array);


aln_range *  find_best_aln_range(kmer_match *, 
                              seq_coor_t, 
                              seq_coor_t, 
                              seq_coor_t); 

void free_aln_range( aln_range *);

kmer_match * find_kmer_pos_for_seq( char *, 
                                    seq_coor_t, 
                                    unsigned int K, 
                                    seq_addr_array, 
                                    kmer_lookup * );

void free_kmer_match( kmer_match * ptr);
void free_kmer_lookup(kmer_lookup * );



void add_sequence ( seq_coor_t, 
                    unsigned int, 
                    char *, 
                    seq_coor_t,
                    seq_addr_array, 
                    seq_array, 
                    kmer_lookup *); 

void mask_k_mer(seq_coor_t, kmer_lookup *, seq_coor_t);


alignment *_align(char *aseq, seq_coor_t aseq_pos,
                   char *bseq, seq_coor_t bseq_pos,
                   seq_coor_t t,
                   int t2);



void free_alignment(alignment *);


void free_consensus_data(consensus_data *);


void print_d_path(  d_path_data2 * base, unsigned long max_idx);

void d_path_sort( d_path_data2 * base, unsigned long max_idx);

int compare_d_path(const void * a, const void * b);


typedef struct {
    seq_coor_t t_pos;
    uint8_t delta;
    char q_base;
    seq_coor_t p_t_pos;   // the tag position of the previous base
    uint8_t p_delta; // the tag delta of the previous base
    char p_q_base;        // the previous base
    unsigned q_id;
} align_tag_t;


typedef struct {
    seq_coor_t len;
    align_tag_t * align_tags;
} align_tags_t;


typedef struct {
    uint16_t size;
    uint16_t n_link;
    seq_coor_t * p_t_pos;   // the tag position of the previous base
    uint8_t * p_delta; // the tag delta of the previous base
    char * p_q_base;        // the previous base
    uint16_t * link_count;
    uint16_t count;
    seq_coor_t best_p_t_pos;
    uint8_t best_p_delta;
    uint8_t best_p_q_base; // encoded base
    double score;
} align_tag_col_t;


typedef struct {
    align_tag_col_t * base;
} msa_base_group_t;


typedef struct {
    uint8_t size;
    uint8_t max_delta;
    msa_base_group_t * delta;
} msa_delta_group_t;

typedef msa_delta_group_t * msa_pos_t;


align_tags_t * get_align_tags( char * aln_q_seq, 
                               char * aln_t_seq, 
                               seq_coor_t aln_seq_len,
                               aln_range * range,
                               unsigned q_id,
                               seq_coor_t t_offset);

align_tags_t * get_align_tags2( char * aln_q_seq,
                               char * aln_t_seq,
                               seq_coor_t aln_seq_len,
                               aln_range * range,
                               unsigned q_id,
                               seq_coor_t t_offset);
							   
							   
void free_align_tags( align_tags_t * tags);


void allocate_aln_col( align_tag_col_t * col);

void realloc_aln_col( align_tag_col_t * col );

void free_aln_col( align_tag_col_t * col);

void allocate_delta_group( msa_delta_group_t * g);

void realloc_delta_group( msa_delta_group_t * g, uint16_t new_size );

void free_delta_group( msa_delta_group_t * g);

void update_col( align_tag_col_t * col, seq_coor_t p_t_pos, uint8_t p_delta, char p_q_base);

msa_pos_t * get_msa_working_sapce(unsigned int max_t_len);

void clean_msa_working_space( msa_pos_t * msa_array, unsigned int max_t_len);

consensus_data * get_cns_from_align_tags( align_tags_t ** tag_seqs, 
                                          unsigned n_tag_seqs, 
                                          unsigned t_len, 
                                          unsigned min_cov );
										  
consensus_data * get_cns_from_align_tags_large( align_tags_t ** tag_seqs, 
                                          unsigned n_tag_seqs, 
                                          unsigned t_len, 
                                          unsigned min_cov );

consensus_data * generate_consensus( char ** input_seq,
                           unsigned int n_seq,
                           unsigned min_cov,
                           unsigned K,
                           double min_idt);


consensus_data * generate_utg_consensus( char ** input_seq,
                           seq_coor_t *offset,
                           unsigned int n_seq,
                           unsigned min_cov,
                           unsigned K,
                           double min_idt);
														  
						   
void free_consensus_data( consensus_data * consensus );
	
#endif
							   
							   
					