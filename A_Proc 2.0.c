/*
 * A_Proc 2.0.c
 *
 *  Created on: Jun 26, 2017
 *      Author: thomasjansen
 */

#include "arghelp/arghelp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef __APPLE__
#include <unistd.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#endif

#include <time.h>

#ifdef __APPLE__
int MAX_PATH;
#endif

int mkdir_t( char* );

void sleep_t( int );

struct tm get_time();

int write_err( char* , char* );

char* read_s( FILE* , char* , int* , int );

int search_s( FILE* fd , char* search );

int count_w( char* , char );

int find_rep( char* , char , char );

int main(int argc, char** argv) {
	char* src_d = getstring(argc, argv, 's');
	char* dest_d = getstring(argc, argv, 'd'); //May not exist

	int time_val = getint(argc, argv, 't'); //Refresh rate of process
	if (time_val == -1)
		time_val = 0;

	if (mkdir_t(dest_d) < 0 && errno != EEXIST) { //Attempts to make destination directory
		printf("Failed to mkdir\n");
		return -1;
	}

#ifdef __APPLE__
	if ((MAX_PATH = pathconf(dest_d, _PC_NAME_MAX)) < 0) { //Gets Max Path Name for dest_d
		printf("Bad MP\n");
		return MAX_PATH;
	}
#endif

	struct tm ttime;
	FILE *rfd, *wfd;
	char* err_c = getstring(argc, argv, 'e'); //Destination for error messages
	if ( err_c == ( char* )NULL )
		err_c = "err.txt";

	int flags, count_d;
	char* path_buff = (char*)malloc(MAX_PATH + 1);
	char* rbuff = ( char* )malloc( 1025 );
	char* last_w = ( char* )malloc( 1025 );
	char* format;

	last_w = strncpy( last_w , "[measurements]" , strlen( "[measurements]" ) + 1 );

	while (1) {
		ttime = get_time(); //Updates Date
		if (ttime.tm_hour == 1) { //If 1 o'clock, move onto new file and record variable names
			//Delete rfd
			last_w = strncpy( last_w , "[measurements]" , strlen( "[measurements]" ) + 1 );
			continue;
		}

		if (((ttime.tm_mon + 1 ) / 10 ) < 1 )
			format = "%s/%d-0%d-%d.log";
		else
			format = "%s/%d-%d-%d.log";
		flags = snprintf(path_buff, MAX_PATH, ( const char* )format , src_d, ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday); //Getting current raw file
		if ((rfd = fopen( path_buff , "r")) == (FILE*)NULL) { //Assumes that raw file has not been created
			sleep_t(time_val);
			continue;
		}

		flags = snprintf(path_buff, MAX_PATH, "%s/%d.%d.%d", dest_d, ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday); //Creating current day file
		if (mkdir_t(path_buff) < 0 && errno != EEXIST) { //If process cannot make the directory and it does not exist
			write_err(err_c, "Failed to make: ");
			write_err(err_c, path_buff);
			write_err(err_c, "\n");
			return -1;
		}

		if (( flags = search_s( rfd, "Name:")) == EOF ) { //Failed to find Name:
			fclose(rfd);
			sleep_t(time_val);
			continue;
		}

		if ( read_s( rfd , rbuff , &flags , 1024 ) == ( char* )NULL || flags == EOF ) {
			fclose(rfd);
			sleep_t(time_val);
			continue;
		}

		flags = snprintf(path_buff, MAX_PATH, "%s/%d.%d.%d/%s.txt", dest_d, ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday, rbuff);
		if ((wfd = fopen(path_buff, "a")) == (FILE*)NULL) { //If writer file fails to open
			fclose(rfd);
			printf("Failed to create/open wfd: %s\n", path_buff);
			write_err(err_c, "Failed to create/open wfd: ");
			write_err(err_c, path_buff);
			write_err(err_c, "\n");
			return -1;
		}

		/*
		* Setup Complete
		* Raw data has been located
		* Destination has been created
		*/

		if ( strncmp( last_w , "[measurements]" , strlen( last_w )) == 0 ) { //Checks to see if the variables are being read
			if ((flags = search_s(rfd, last_w)) == EOF) { //If search for measurements fails
				fclose(rfd);
				fclose(wfd);
				sleep_t(time_val);
				continue;
			}
			if ( read_s( rfd , rbuff , &flags , 1024 ) == ( char* )NULL || flags == EOF ) {
				fclose(rfd);
				fclose(wfd);
				sleep_t(time_val);
				continue;
			}

			count_d = count_w(rbuff, ';'); //Count number of variables to judge completeness of data point
			find_rep(rbuff , ';', ','); //Make data into .csv
			fwrite(rbuff, strlen( rbuff ), 1, wfd); //Write data to file
			fputc('\n', wfd);
			last_w = strncpy( last_w , "[start]" , strlen( "[start]" ) + 1);
		}

		if ( search_s( rfd , last_w ) == EOF ) {
			fclose(rfd);
			fclose(wfd);
			sleep_t(time_val);
			continue;
		}

		flags = 0;
		while (flags != EOF) {
			if (read_s( rfd , rbuff , &flags , 1024 ) == ( char* )NULL )
				continue;
			if ( count_d != count_w( rbuff , ';' ))
				continue;

			last_w = strncpy( last_w , rbuff , strlen( rbuff ) + 1 ); //Set last_w to most recent valid data point
			find_rep(rbuff, ';', ','); //Format into .csv
			fwrite(rbuff, strlen( rbuff ) , 1, wfd);
			fputc('\n', wfd);
		}

		fclose(rfd);
		fclose(wfd);
		sleep_t(time_val);
	}

	return 0;
}

char* read_s( FILE* fd , char* buff , int* flags , int sz ) {
	int i = 0;
	*flags = 0;
	while (( buff[i] = fgetc( fd )) != '\n' && buff[i] != EOF && buff[i] != ' ' && i < sz )
		i++;

	if ( i == 0 ) { //Case that no real word was read
		if ( buff[i] == EOF ) //End of file reached
			*flags = EOF;
		return ( char* )NULL;
	}

#ifdef __APPLE__
	if ( buff[i] == '\n' ) //Mac only
		i--;
#endif

	if ( buff[i] == EOF ) { //if word read was the last word in the file
		*flags = EOF;
	}

	buff[i] = '\0';
	return buff;
}

int search_s( FILE* fd , char* search ) {
	int flags = 0;
	char* buff = ( char* )malloc( 1024 );
	while( flags != EOF ) {
		if ( read_s( fd , buff , &flags , 1024 ) == ( char* )NULL )
			continue;

		if ( strncmp( buff , search , strlen( search )) == 0 ) {
			free( buff );
			return flags;
		}
	}
	free( buff );
	return EOF;
}

int find_rep( char* buff , char f , char r ) {
	for ( size_t i = 0; i < strlen( buff ); i++ ) {
		if ( buff[i] == f )
			buff[i] = r;
	}
	return 0;
}

int count_w( char* buff , char c) {
	int cnt = 0;
	for ( size_t i = 0; i < strlen( buff ); i++ ) {
		if ( buff[i] == c ) {
			cnt++;
		}
	}
	return cnt;
}

int mkdir_t(char* filename) {
#ifdef __APPLE__
	return mkdir(filename, S_IRWXU);
#endif

#ifdef _WIN32
	return _mkdir(filename);
#endif

}

void sleep_t(int t_secs) {
#ifdef __APPLE__
	sleep(t_secs);
#endif

#ifdef _WIN32
	Sleep(t_secs * 100);
#endif
}

struct tm get_time() {
	time_t t = time(NULL);
	return *localtime(&t);
}

int write_err(char* err_c, char* buff) {
	FILE* errfd = fopen(err_c, "a");
	if (errfd == (FILE*)NULL)
		return -1;
	fwrite(buff, strlen( buff ), 1, errfd);
	fclose(errfd);
	return 0;
}

