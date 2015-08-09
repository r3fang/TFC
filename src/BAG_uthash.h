/*--------------------------------------------------------------------*/
/* Created Date: 15JULY2015                                           */
/* Author: Rongxin Fang                                               */
/* Contact: r3fang@ucsd.edu                                           */
/* Library for Breakend Associated Graph (BAG).                       */
/*--------------------------------------------------------------------*/

#ifndef _BAG_UTHASH_H
#define _BAG_UTHASH_H

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>
#include "utils.h"
#include "kseq.h"
#include "kmer_uthash.h"
/* error code */
#define BA_ERR_NONE		     0 // no error

/* defination of junction_t */
typedef struct {
	char* idx;          /* the key of this junction, determined by exon1.start.exon2.end, must be unique */
	char* exon1;       
	char* exon2;
	char* s;            /* short junction string that flanks the junction sites*/
	char *transcript;   /* constructed transcript */ 
	int junc_pos;       /* junction position on transcript */ 
	int *S1;            /* stores exon jump position of gene1 */
	int *S2;            /* stores exon jump position of gene2 */
	int S1_num;         /* number of exon jump positions of gene1 */
	int S2_num;         /* number of exon jump positions of gene2 */
	size_t hits;        /* number of times the junction is hitted by reads */
	double likehood;    /* likelihood of the junction */
    UT_hash_handle hh;
} junction_t;

/*
 * the BAG_uthash structure
 */
typedef struct{
	char *edge;
	size_t weight;
	char **read_names;  /* stores the name of read pair that support this edge*/
	char **evidence;    /* stores the read pair that support this edge*/
	junction_t *junc;   /* stores the junctions of the edge, NULL if no junction identified */
    UT_hash_handle hh;  /* makes this structure hashable */
} bag_t;

static inline bag_t 
*bag_init() {
	bag_t *t = mycalloc(1, bag_t);
	t->edge = NULL;
	t->weight = 0;
	t->evidence = mycalloc(1, char*);
	t->read_names = mycalloc(1, char*);
	t->junc = NULL;
	return t;
}

static inline bag_t
*find_edge(bag_t *bag, char* quary) {
	if(quary == NULL) return NULL;
	bag_t* edge = NULL;	
    HASH_FIND_STR(bag, quary, edge);  /* s: output pointer */
	return edge;
}

static inline int 
bag_distory(bag_t **bag) {
	if(*bag == NULL) return 0;
	/*free the kmer_hash table*/
	register bag_t *bag_cur, *bag_tmp;
	junction_t *junc_cur, *junc_tmp;
	HASH_ITER(hh, *bag, bag_cur, bag_tmp) {
		if(bag_cur->junc != NULL){ // if the edge has junctions
			HASH_ITER(hh, bag_cur->junc, junc_cur, junc_tmp){
				HASH_DEL(bag_cur->junc, junc_cur); 			
				free(junc_cur);	
			}			
		}
		HASH_DEL(*bag, bag_cur); 
		free(bag_cur);   
    }
	return 0;
}

/*
 * display BAG_uthash table on the screen
 *
 * PARAMETERS:	bag_t *
 * RETURN:	error code
 */
static inline int bag_display(bag_t *bag) {
	if(bag == NULL) return -1;
	/*free the kmer_hash table*/
	register bag_t *bag_cur, *bag_tmp;
	HASH_ITER(hh, bag, bag_cur, bag_tmp) {
		int i; for(i=0; i < bag_cur->weight; i++){
			printf(">%s\t%s\n%s\n", bag_cur->edge, bag_cur->read_names[i], bag_cur->evidence[i]);
		}
	}
	return 0;
}

static inline junction_t 
*junction_init(int seed_len){
	junction_t *junc = mycalloc(1, junction_t);
	junc->idx = NULL;
	junc->exon1 = NULL;
	junc->exon2 = NULL;
	junc->transcript = NULL;
	junc->junc_pos = 0;
	junc->S1 = NULL;
	junc->S2 = NULL;
	junc->s = mycalloc(seed_len+1, char);	
	junc->S1_num = 0;
	junc->S2_num = 0;	
	junc->hits = 0;	
	junc->likehood = 0.0;	
	return junc;
}

static inline junction_t 
*find_junction(junction_t *junc, char* quary) {
	if(quary == NULL) return NULL;
	junction_t *s;
    HASH_FIND_STR(junc, quary, s);  /* s: output pointer */
	return s;
}

static inline int 
junction_destory(junction_t **junc){
	if(*junc==NULL) return -1;
	junction_t *junc_cur, *junc_tmp;
	HASH_ITER(hh, *junc, junc_cur, junc_tmp) {
		HASH_DEL(*junc, junc_cur);
		free(junc_cur);
	}
	return 0;
}

static inline int min_mismatch(char* str, char* pattern){
	if(str == NULL || pattern == NULL) die("[%s] input error"); 
	register int i, j, n;
	int min_mis_match = strlen(pattern)+1;
	char substring[strlen(pattern)+1];
	for(i=0;i<strlen(str)-strlen(pattern);i++){
		n = 0;
		memset(substring, '\0', sizeof(substring));
		strncpy(substring, &str[i], strlen(pattern));
		for(j=0; j<strlen(pattern); j++){if(toupper(pattern[j]) != toupper(substring[j])){n++;}}
		if (n < min_mis_match){min_mis_match = n;} // update min_mis_match
	}
	return min_mis_match;
} 

/*
 * add one edge to graph
 */
static inline int 
BAG_uthash_add(bag_t** bag, char* edge_name, char* read_name, char* evidence){
	if(edge_name == NULL || evidence == NULL) return -1;
	bag_t *bag_cur;
	register int n;
	if((bag_cur = find_edge(*bag, edge_name)) == NULL){ /* if edge does not exist */
		bag_cur = bag_init();
		bag_cur->edge = strdup(edge_name);
		bag_cur->weight = 1;
		bag_cur->read_names[0] = strdup(read_name);
		bag_cur->evidence[0] = strdup(evidence);       /* first and only 1 element*/
		HASH_ADD_STR(*bag, edge, bag_cur);								
	}else{
		bag_cur->weight++;
		char **tmp1 = mycalloc(bag_cur->weight, char*);
		char **tmp2 = mycalloc(bag_cur->weight, char*);
		
	 	for (n = 0; n < bag_cur->weight-1; n++){
	 		tmp1[n] = strdup(bag_cur->evidence[n]);
	 	}
	 	tmp1[n] = strdup(evidence);
	 	free(bag_cur->evidence);
	 	bag_cur->evidence = tmp1;
		
	 	for (n = 0; n < bag_cur->weight-1; n++){
	 		tmp2[n] = strdup(bag_cur->read_names[n]);
	 	}
	 	tmp2[n] = strdup(read_name);
	 	free(bag_cur->read_names);
	 	bag_cur->read_names = tmp2;
	}
	return 0;
}

/*
 * delete edges in bag with evidence less than min_weight
 */
static inline bag_t
*bag_trim(bag_t* bag, int min_weight){
	if(bag == NULL) return NULL;	
	
	register bag_t *bag_cur, *bag_tmp, *bag_s, *bag_res=NULL;
	HASH_ITER(hh, bag, bag_cur, bag_tmp) {
		if(bag_cur->weight >= min_weight){
			if((bag_s = find_edge(bag_res, bag_cur->edge))==NULL){ /* this edge not exist */
				HASH_ADD_STR(bag_res, edge, bag_cur);
			}
		}
    }
	return bag_res;
}

/*
 * rm duplicate evidence for graph edge
 */
static inline int 
bag_uniq(bag_t **tb){
	if(*tb == NULL) die("BAG_uthash_uniq: input error");
	bag_t *cur, *tmp;
	int i, j;
	int before, del;
	HASH_ITER(hh, *tb, cur, tmp) {
		if(cur == NULL) die("kmer_uthash_uniq: HASH_ITER fails\n");
		qsort(cur->evidence, cur->weight, sizeof(char*), mystrcmp);
		before = cur->weight;
		del = 0;
		if(cur->weight > 1){
			for(i=1; i<cur->weight; i++){
				if(mystrcmp(cur->evidence[i], cur->evidence[i-1]) == 0){
					cur->evidence[i-1] = NULL;
					del++;
				}
			}
		}		
		if(del > 0){ // if any element has been deleted
			char** tmp = mycalloc(before - del , char*);
			j=0; for(i=0; i<cur->weight; i++){
				if(cur->evidence[i] != NULL){
					tmp[j++] = strdup(cur->evidence[i]);
				}
			}
			free(cur->evidence);
			cur->evidence = tmp;		
			cur->weight = before - del;	
		}
    }	
	return BA_ERR_NONE;
}

/* 
 * Find all genes uniquely matched with kmers on _read.          
 * hash     - a hash table count number of matches between _read and every gene
 * _read    - inqury read
 * _k       - kmer length
 */
static inline int
find_all_genes(str_ctr **hash, struct kmer_uthash *KMER_HT, char* _read, int _k){
/*--------------------------------------------------------------------*/
	/* check parameters */
	if(_read == NULL || _k < 0) die("find_all_MEKMs: parameter error\n");
/*--------------------------------------------------------------------*/
	/* declare vaiables */
	str_ctr *s;
	int _read_pos = 0;
	int num;
	char* gene = NULL;
	register struct kmer_uthash *s_kmer = NULL; 
	char buff[_k];
/*--------------------------------------------------------------------*/
	while(_read_pos<(strlen(_read)-_k+1)){
		/* copy a kmer of string */
		strncpy(buff, _read + _read_pos, _k); buff[_k] = '\0';	
		if(strlen(buff) != _k) die("find_next_match: buff strncpy fails\n");
		/*------------------------------------------------------------*/
		if((s_kmer=find_kmer(KMER_HT, buff)) == NULL){_read_pos++; continue;} // kmer not in table but not an error
		if(s_kmer->count == 1){ // only count the uniq match 
			//gene = strdup(s_kmer->seq_names[0]);
			gene = strsplit(s_kmer->seq_names[0], '.', &num)[0];
			if(gene == NULL) die("find_next_match: get_exon_name fails\n");
			if(str_ctr_add(hash, gene) != 0) die("find_all_MEKMs: str_ctr_add fails\n");
		}
		_read_pos++;
	}
	return 0;
}


/* 
 * Find all exons uniquely matched with kmers on _read.          
 * hash     - a hash table count number of matches between _read and every gene
 * _read    - inqury read
 * _k       - kmer length
 */
static inline int
find_all_exons(str_ctr **hash, struct kmer_uthash *KMER_HT, char* _read, int _k){
/*--------------------------------------------------------------------*/
	/* check parameters */
	if(_read == NULL || _k < 0) die("find_all_MEKMs: parameter error\n");
/*--------------------------------------------------------------------*/
	/* declare vaiables */
	str_ctr *s;
	int _read_pos = 0;
	char* exon = NULL;
	register struct kmer_uthash *s_kmer = NULL; 
	char buff[_k];
/*--------------------------------------------------------------------*/
	while(_read_pos<(strlen(_read)-_k+1)){
		/* copy a kmer of string */
		strncpy(buff, _read + _read_pos, _k); buff[_k] = '\0';	
		if(strlen(buff) != _k) die("find_next_match: buff strncpy fails\n");
		/*------------------------------------------------------------*/
		if((s_kmer=find_kmer(KMER_HT, buff)) == NULL){_read_pos++; continue;} // kmer not in table but not an error
		if(s_kmer->count == 1){ // only count the uniq match 
			exon = strdup(s_kmer->seq_names[0]);
			if(exon == NULL) die("find_next_match: get_exon_name fails\n");
			if(str_ctr_add(hash, exon) != 0) die("find_all_MEKMs: str_ctr_add fails\n");
		}
		_read_pos++;
	}
	return 0;
}

#endif
