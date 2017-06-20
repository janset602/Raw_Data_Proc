/*
 * word.h
 *
 *  Created on: Jun 19, 2017
 *      Author: thomasjansen
 */

#ifndef WORD_H_
#define WORD_H_

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef struct {
	char* buff;
	int len;
} word;

word* create_word( char* , int );

void destroy_word( word* );

int word_cmp( word* , word* );

word* read_t( FILE* , int* , int );

int search_w( FILE* , char* , int );

#endif /* WORD_H_ */
