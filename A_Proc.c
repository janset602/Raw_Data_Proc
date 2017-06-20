/*
 * A_Proc.c
 *
 *  Created on: Jun 19, 2017
 *      Author: thomasjansen
 */


#include "arghelp/arghelp.h"
#include "strl/strl.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include "word/word.h"

int MAX_PATH;

int search_mw( FILE* fd , char* buff ) {
	return search_w( fd , buff , strlength( buff , '\0' ));
}

word* create_mword( char* buff ) {
	return create_word( buff , strlength( buff , '\0' ));
}

int word_mcmp( word* w , char* buff ) {
	int ans;
	word* w1 = create_mword( buff );
	ans = word_cmp( w , w1 );
	destroy_word( w1 );
	return ans;
}

int count(word* w, char c) {
	int j = 0;
	for (int i = 0; i < w->len; i++)
		if (w->buff[i] == c)
			j++;
	return j;
}

void find_rep(word* w, char f, char r) {
	for (int i = 0; i < w->len; i++)
		if (w->buff[i] == f)
			w->buff[i] = r;
}

struct tm get_time() {
	time_t t = time( NULL );
	return *localtime( &t );
}

int write_err( char* err_c , char* buff ) {
	FILE* errfd = fopen( err_c , "a" );
	if ( errfd == ( FILE* )NULL )
		return -1;
	fwrite( buff , strlength( buff , '\0' ) , 1 , errfd );
	fclose( errfd );
	return 0;
}

int main( int argc , char** argv ) {
	char* src_d = getstring( argc , argv , 's' );
	char* dest_d = getstring( argc , argv , 'd' ); //May not exist

	int time_val = getint( argc , argv , 't' );
	if ( time_val == -1 )
		time_val = 0;

	if ( mkdir( dest_d , S_IRWXU ) < 0 && errno != EEXIST ) {
		printf( "Failed to mkdir\n" );
		return -1;
	}

	if (( MAX_PATH = pathconf( dest_d , _PC_NAME_MAX )) < 0 ) { //Gets Max Path Name for dest_d
		printf( "Bad MP\n" );
		return MAX_PATH;
	}

	struct tm ttime;
	FILE *rfd , *wfd;
	char* err_c = getstring( argc , argv , 'e' );
	int flags , count_d;
	char* buff = ( char* )malloc( MAX_PATH + 1 );

	word* last_w = create_mword( "[measurements]" );
	word* f_name;

	while ( 1 ) {
		ttime = get_time();
		flags = snprintf( buff , MAX_PATH , "%s/%d-0%d-%d.log" , src_d , ttime.tm_year + 1900 , ttime.tm_mon + 1 , ttime.tm_mday );
		if (( rfd = fopen( buff , "r" )) == ( FILE* )NULL ) {
			sleep( time_val );
			continue;
		}

		flags = snprintf( buff , MAX_PATH , "%s/%d.%d.%d" , dest_d , ttime.tm_year + 1900 , ttime.tm_mon + 1 , ttime. tm_mday );
		if ( mkdir( buff , S_IRWXU ) < 0 && errno != EEXIST ) {
			/*
			 * Other issues
			 */
			write_err( err_c , "Failed to make: " );
			write_err( err_c , buff );
			write_err( err_c , "\n" );
			return -1;
		}

		flags = search_mw( rfd , "Name:" );
		if ( flags == EOF ) {
			fclose( rfd );
			sleep( time_val );
			continue;
		}

		f_name = read_t( rfd , &flags , 1024 );
		if ( flags == EOF ) {
			fclose( rfd );
			if ( f_name != ( word* )NULL )
				destroy_word( f_name );
			sleep( time_val );
			continue;
		}

		flags = snprintf( buff , MAX_PATH , "%s/%d.%d.%d/%s.txt" , dest_d , ttime.tm_year + 1900 , ttime.tm_mon + 1 , ttime.tm_mday , f_name->buff );
		if (( wfd = fopen( buff , "w" )) == ( FILE* )NULL ) { //Switch back to a later on
			/*
			 * bigger issues
			 */
			fclose( rfd );
			printf( "Failed to create/open wfd: %s\n" , buff );
			write_err( err_c , "Failed to create/open wfd: " );
			write_err( err_c , buff );
			write_err( err_c , "\n" );
			return -1;
		}
		destroy_word( f_name );
		/*
		 * Setup Complete
		 * Raw data has been located
		 * Destination has been created
		 */

		if ( word_mcmp( last_w , "[measurements]" )) {
			if (( flags = search_w( rfd , last_w->buff , last_w->len )) == EOF ) {
				fclose( rfd );
				fclose( wfd );
				sleep( time_val );
				continue;
			}
			word* header = read_t( rfd , &flags , 1024 );
			if ( flags == EOF ) {
				fclose( rfd );
				fclose( wfd );
				if ( header != ( word* )NULL )
					destroy_word( header );
				sleep( time_val );
				continue;
			}

			count_d = count( header , ';' );
			find_rep( header , ';' , ',' );
			fwrite( header->buff , header->len , 1 , wfd );
			fputc( '\n' , wfd );
			destroy_word( header );
			destroy_word( last_w );
			last_w = create_mword( "[start]" );
		}

		if ( search_w( rfd , last_w->buff , last_w->len ) == EOF ) {
			fclose( rfd );
			fclose( wfd );
			sleep( time_val );
			continue;
		}

		flags = 0;
		word* w;
		while( flags != EOF ) {
			if (( w = read_t( rfd , &flags , 1024 )) == ( word* )NULL )
				continue;
			if ( count_d != count( w , ';' )) { //incomplete data point
				destroy_word( w );
				continue;
			}

			destroy_word( last_w );
			last_w = create_word( w->buff , w->len );
			find_rep( w , ';' , ',' );
			fwrite( w->buff , w->len , 1 , wfd );
			fputc( '\n' , wfd );
			destroy_word( w );
		}

		fclose( rfd );
		fclose( wfd );

		if ( ttime.tm_hour == 1 ) {
			printf( "One\n" );
		}

		sleep( time_val );
	}

	return 0;
}