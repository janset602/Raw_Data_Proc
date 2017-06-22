/*
* A_Proc.c
*
*  Created on: Jun 19, 2017
*      Author: thomasjansen
*/


#include "arghelp/arghelp.h"
#include "strl/strl.h"
#include "word/word.h"
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

int search_mw( FILE* , char* );

word* create_mword( char* );

int word_mcmp( word* , char* );

int count_w( word* , char );

void find_rep( word* , char , char );

struct tm get_time();

int write_err( char* , char* );

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
	char* buff = (char*)malloc(MAX_PATH + 1);

	word* last_w = create_mword("[measurements]"); //When process begins, it gets variable names and count
	word* f_name;

	while (1) {
		ttime = get_time(); //Updates Date
		if (ttime.tm_hour == 1) { //If 1 o'clock, move onto new file and record variable names
			//Delete rfd
			printf("New File\n");
			destroy_word(last_w);
			last_w = create_mword("[measurements]");
		}
		flags = snprintf(buff, MAX_PATH, "%s/%d-0%d-%d.log", src_d, ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday); //Getting current raw file
		if ((rfd = fopen(buff, "r")) == (FILE*)NULL) { //Assumes that raw file has not been created
			sleep_t(time_val);
			continue;
		}

		flags = snprintf(buff, MAX_PATH, "%s/%d.%d.%d", dest_d, ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday); //Creating current day file
		if (mkdir_t(buff) < 0 && errno != EEXIST) { //If process cannot make the directory and it does not exist
			/*
			* Other issues
			*/
			write_err(err_c, "Failed to make: ");
			write_err(err_c, buff);
			write_err(err_c, "\n");
			return -1;
		}

		if (( flags = search_mw(rfd, "Name:")) == EOF ) { //Failed to find Name:
			fclose(rfd);
			sleep_t(time_val);
			continue;
		}

		f_name = read_t(rfd, &flags, 1024); //Attempts to read device name
		if (flags == EOF) { //If dev_name is at the end of the file, try again later to avoid errors
			fclose(rfd);
			if (f_name != (word*)NULL)
				destroy_word(f_name);
			sleep_t(time_val);
			continue;
		}

		flags = snprintf(buff, MAX_PATH, "%s/%d.%d.%d/%s.txt", dest_d, ttime.tm_year + 1900, ttime.tm_mon + 1, ttime.tm_mday, f_name->buff);
		if ((wfd = fopen(buff, "a")) == (FILE*)NULL) { //If writer file fails to open
			fclose(rfd);
			printf("Failed to create/open wfd: %s\n", buff);
			write_err(err_c, "Failed to create/open wfd: ");
			write_err(err_c, buff);
			write_err(err_c, "\n");
			return -1;
		}
		destroy_word(f_name); //Frees memoray allocated for device name

		/*
		* Setup Complete
		* Raw data has been located
		* Destination has been created
		*/

		if (word_mcmp(last_w, "[measurements]")) { //Checks to see if the variables are being read
			if ((flags = search_w(rfd, last_w->buff, last_w->len)) == EOF) { //If search for measurements fails
				fclose(rfd);
				fclose(wfd);
				sleep_t(time_val);
				continue;
			}
			word* header = read_t(rfd, &flags, 1024);
			if (flags == EOF) { //If variable names are at EOF
				fclose(rfd);
				fclose(wfd);
				if (header != (word*)NULL)
					destroy_word(header);
				sleep_t(time_val);
				continue;
			}

			count_d = count_w(header, ';'); //Count number of variables to judge completeness of data point
			find_rep(header, ';', ','); //Make data into .csv
			fwrite(header->buff, header->len, 1, wfd); //Write data to file
			fputc('\n', wfd);
			destroy_word(header);
			destroy_word(last_w);
			last_w = create_mword("[start]"); //Now process can begin recording data
		}

		if (search_w(rfd, last_w->buff, last_w->len) == EOF) { //Search for last data point
			fclose(rfd);
			fclose(wfd);
			sleep_t(time_val);
			continue;
		}

		flags = 0;
		word* w;
		while (flags != EOF) {
			if ((w = read_t(rfd, &flags, 1024)) == (word*)NULL) //If there is a space or have reached EOF without a word found
				continue;
			if (count_d != count_w(w, ';')) { //Incomplete data point or unnecessary information in raw data
				destroy_word(w);
				continue;
			}
			/*
			 * w is a valid data point
			 */
			destroy_word(last_w);
			last_w = create_word(w->buff, w->len); //Set last_w to most recent valid data point
			find_rep(w, ';', ','); //Format into .csv
			fwrite(w->buff, w->len, 1, wfd);
			fputc('\n', wfd);
			destroy_word(w);
		}

		fclose(rfd);
		fclose(wfd);
		sleep_t(time_val);
	}

	return 0;
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

int search_mw(FILE* fd, char* buff) {
	return search_w(fd, buff, strlength(buff, '\0'));
}

word* create_mword(char* buff) {
	return create_word(buff, strlength(buff, '\0'));
}

int word_mcmp(word* w, char* buff) {
	int ans;
	word* w1 = create_mword(buff);
	ans = word_cmp(w, w1);
	destroy_word(w1);
	return ans;
}

int count_w(word* w, char c) {
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
	time_t t = time(NULL);
	return *localtime(&t);
}

int write_err(char* err_c, char* buff) {
	FILE* errfd = fopen(err_c, "a");
	if (errfd == (FILE*)NULL)
		return -1;
	fwrite(buff, strlength(buff, '\0'), 1, errfd);
	fclose(errfd);
	return 0;
}
