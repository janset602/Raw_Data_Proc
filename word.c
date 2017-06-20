/*
 * word.c
 *
 *  Created on: Jun 19, 2017
 *      Author: thomasjansen
 */

#include "word.h"

word* create_word(char* buff, int sz) {
	if (buff == (char*)NULL)
		return (word*)NULL;

	word* w = (word*)malloc(sizeof(word));

	if (w == (word*)NULL)
		return w;

	w->buff = (char*)malloc(sz + 1);

	if (w->buff == (char*)NULL) {
		free(w);
		return (word*)NULL;
	}

	if ((w->buff = strncpy(w->buff, buff, sz)) == (char*)NULL) {
		free(w);
		return (word*)NULL;
	}

	w->buff[sz] = '\0';
	w->len = sz;
	return w;
}

void destroy_word( word* w ) {
	free( w->buff );
	free( w );
}

int word_cmp(word* w1, word* w2) {
	if (w1->len != w2->len)
		return FALSE;
	if ( strncmp(w1->buff, w2->buff, w1->len) != 0 )
		return FALSE;
	return TRUE;
}

word* read_t( FILE* fd , int* flags , int sz ) { //Always pass empty word* to prevent mem loss
	int i;
	char* buff;
	i = 0;
	*flags = 0;
	buff = ( char* )malloc( sz );

	while ((buff[i] = fgetc(fd)) != EOF && buff[i] != '\n' && buff[i] != ' ' && i < sz ) {
		i++;
	}
#ifdef __APPLE__
	if ( buff[i] == '\n' ) //Mac only
		i--;
#endif

	if ( i == 0 ) { //Case that no real word was read
		if ( buff[i] == EOF ) //End of file reached
			*flags = EOF;
		free( buff );
		return ( word* )NULL;
	}

	if ( buff[i] == EOF ) { //if word read was the last word in the file
		*flags = EOF;
	}

	word* w = create_word(buff, i);
	free(buff);
	return w;
}

int search_w( FILE* fd , char* buff , int sz ) {
	word* search = create_word( buff , sz );
	word* w;
	int flags = 0;

	while( flags != EOF ) {
		if (( w = read_t( fd , &flags , 1024 )) == ( word* )NULL )
			continue;

		if ( word_cmp( search , w ) == TRUE ) {
			destroy_word( search );
			destroy_word( w );
			return flags;
		}
		else {
			destroy_word( w );
		}
	}
	destroy_word(search);
	return EOF;
}
