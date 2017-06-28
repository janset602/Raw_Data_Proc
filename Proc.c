#include "arghelp\arghelp.h"
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <time.h>
#include <direct.h>

#define ERR_PATH "C:\\Users\\Thomas\\OneDrive\\Inverter_Data\\err.txt"

char* err_buff;

struct tm parse_t(char*);

struct tm gettime();

int tmcmp(struct tm, struct tm);

char* read_s(FILE*, char*, int*, int);

int search_s(FILE* , char* );

int fcopy(FILE*, char*, char*);

int count_w(char*, char);

int find_rep(char*, char, char);

int fproc_a(char*, char*, struct tm, int);

int write_err( char*);

int main(int argc, char** argv) {
	char* src_d, *dest_d, *rsave_d, *sPath, *buff;
	int timeval, flags;
	timeval = getint(argc, argv, 't'); //Usually 15 minutes, fproc_a will gather realtime data, then main will sleep
	src_d = getstring(argc, argv, 's'); //Where raw data is stored
	dest_d = getstring(argc, argv, 'd'); //Proccessed data storage
	rsave_d = getstring(argc, argv, 'r');
	buff = (char*)malloc(1025);
	sPath = (char*)malloc(MAX_PATH + 1);
	err_buff = (char*)malloc(1025);

	struct tm tm;
	HANDLE hd;
	WIN32_FIND_DATAA fd;
	
	if (_mkdir(dest_d) < 0 && errno != EEXIST) { //Attempts to make destination directory
		snprintf(err_buff, 1024, "Failed to mkdir: %s\n", dest_d);
		write_err(err_buff);
		return -1;
	}

	if (_mkdir(rsave_d) < 0 && errno != EEXIST) { //Attempts to make raw_save directory
		snprintf(err_buff, 1024, "Failed to mkdir: %s\n", rsave_d);
		write_err(err_buff);
		return -1;
	}

	while (1) {
		if ((flags = snprintf(sPath, MAX_PATH, "%s\\*.*", src_d)) >= MAX_PATH) { //Attempts to write source directory
			snprintf(err_buff, 1024, "SNPRINTF Failed: %s\n", sPath);
			write_err(err_buff);
			return -1;
		}

		if ((hd = FindFirstFileA(sPath, &fd)) == INVALID_HANDLE_VALUE) { //Attempts to get first file, even if directory empty, this returns .
			snprintf(err_buff , 1024 , "Failed to find Path: %s\n", sPath );
			write_err(err_buff);
			return -1;
		}

		do {
			if (strncmp(fd.cFileName, ".", strlen(fd.cFileName)) == 0 || strncmp(fd.cFileName, "..", strlen(fd.cFileName)) == 0) 
				continue;
			tm = parse_t(fd.cFileName); //Gets raw_data filename into a date
			snprintf(sPath, MAX_PATH, "%s\\%s", src_d, fd.cFileName); //Getting path to raw_data file using src_d
			snprintf(buff, MAX_PATH, "%s\\%d.%02d.%02d", dest_d, tm.tm_year, tm.tm_mon, tm.tm_mday); //Creating name for destination directory using dat
			if (_mkdir(buff) < 0 && errno != EEXIST) { //Creates destination directory
				snprintf(err_buff, 1024, "Failed to mkdir: %s\n", buff);
				write_err(err_buff);
				return -1;
			}
			if (fproc_a(sPath, buff, tm, timeval) < 0) //fproc_a failure
				return -1;
			
			FILE* rcop = fopen(sPath, "r");
			if (fcopy(rcop, fd.cFileName, rsave_d) < 0)
				return -1;
			fclose(rcop);
			if (!DeleteFileA(sPath)) {
				snprintf(err_buff, 1024, "Failed to Delete: %s\n", sPath);
				write_err(err_buff);
			}
		} while (FindNextFileA(hd, &fd));

		Sleep(timeval * 1000);
	}

	return 0;
}

int write_err( char* buff) {
	FILE* fd = fopen(ERR_PATH, "a");
	fwrite(buff, strlen(buff), 1, fd);
	fclose(fd);
	return 0;
}

int fproc_a(char* raw_f, char* proc, struct tm ftime, int time_val) { //raw_f is path to raw_data, proc is destination, ftime is the date data was recorded
	struct tm curt; //Gets current time to check if raw_f is current data or past data
	FILE* rfd, *wfd;
	int flags, count_d;
	char* buff = (char*)malloc(1025);
	char* last_w = (char*)malloc(1025);
	char* path_buff = (char*)malloc(MAX_PATH + 1);
	last_w = strncpy(last_w, "[measurements]", strlen("[measurements]") + 1);

	do {
		curt = gettime(); //Gets current date
		if ((rfd = fopen(raw_f, "r")) == (FILE*)NULL) { //Opens raw_data for reading, main will never hand fproc_a a non_exist file
			snprintf(err_buff, 1024, "Failed to open: %s\n", raw_f);
			write_err(err_buff);
			free(buff);
			free(last_w);
			free(path_buff);
			return -1;
		}

		if (( flags = search_s( rfd , "Name:" )) == EOF ) { // Searches for inverter name
			Sleep(time_val * 1000);
			continue;
		}

		if (read_s(rfd, buff, &flags, 1024) == (char*)NULL || flags == EOF) { //If reading inverter name fails or hits EOF
			Sleep(time_val * 1000);
			continue;
		}

		if ((flags = snprintf(path_buff, MAX_PATH, "%s\\%s.txt", proc, buff)) >= MAX_PATH) { //Attempts to create writing file name
			snprintf(err_buff, 1024, "SNPRINTF failed: %s\n", path_buff);
			write_err(err_buff);
			fclose(rfd);
			free(buff);
			free(path_buff);
			free(last_w);
			return -1;
		}

		if ((wfd = fopen(path_buff, "a")) == (FILE*)NULL) { //Attempts to open wrting file
			fclose(rfd);
			snprintf(err_buff, 1024, "Failed to open Writer: %s\n", path_buff);
			write_err(err_buff);
			free(buff);
			free(last_w);
			free(path_buff);
			return -1;
		}

		if (strncmp(last_w, "[measurements]", strlen(last_w)) == 0) { //Checks to see if the variables are being read
			if ((flags = search_s(rfd, last_w)) == EOF) { //If search for measurements fails
				fclose(rfd);
				fclose(wfd);
				Sleep(time_val * 1000);
				continue;
			}
			if (read_s(rfd, buff, &flags, 1024) == (char*)NULL || flags == EOF) { //Attempts to read variable names
				fclose(rfd);
				fclose(wfd);
				Sleep(time_val * 1000);
				continue;
			}

			count_d = count_w(buff, ';'); //Count number of variables to judge completeness of data point
			find_rep(buff, ';', ','); //Make data into .csv
			fwrite(buff, strlen(buff), 1, wfd); //Write data to file
			fputc('\n', wfd);
			last_w = strncpy(last_w, "[start]", strlen("[start]") + 1); //Sets new search to start of data
		}

		if (search_s(rfd, last_w) == EOF) { //Searches for last saved data, if EOF, new data has not been saved
			fclose(rfd);
			fclose(wfd);
			Sleep(time_val * 1000);
			continue;
		}

		flags = 0;
		while (flags != EOF) {
			if (read_s(rfd, buff, &flags, 1024) == (char*)NULL) //Reads data
				continue;
			if (count_d != count_w(buff, ';')) //Checks if data is complete and real data point
				continue;

			last_w = strncpy(last_w, buff, strlen(buff) + 1); //Set last_w to most recent valid data point
			find_rep(buff, ';', ','); //Format into .csv
			fwrite(buff, strlen(buff), 1, wfd);
			fputc('\n', wfd);
		}

		fclose(rfd);
		fclose(wfd);

		if ( tmcmp( curt , ftime )) //If data was past data, no reason to sleep
			Sleep(time_val * 1000);

	} while (tmcmp(curt, ftime)); //Checks to see if data is current


	free(buff);
	free(last_w);
	free(path_buff);
	return 0;
}

int find_rep(char* buff, char f, char r) {
	for (size_t i = 0; i < strlen(buff); i++) {
		if (buff[i] == f)
			buff[i] = r;
	}
	return 0;
}

int count_w(char* buff, char c) {
	int cnt = 0;
	for (size_t i = 0; i < strlen(buff); i++) {
		if (buff[i] == c) {
			cnt++;
		}
	}
	return cnt;
}

char* read_s(FILE* fd, char* buff, int* flags, int sz) {
	int i = 0;
	*flags = 0;
	while ((buff[i] = fgetc(fd)) != '\n' && buff[i] != EOF && buff[i] != ' ' && i < sz)
		i++;

	if (i == 0) { //Case that no real word was read
		if (buff[i] == EOF) //End of file reached
			*flags = EOF;
		return (char*)NULL;
	}

#ifdef __APPLE__
	if (buff[i] == '\n') //Mac only
		i--;
#endif

	if (buff[i] == EOF) { //if word read was the last word in the file
		*flags = EOF;
	}

	buff[i] = '\0';
	return buff;
}

int search_s(FILE* fd, char* search) {
	int flags = 0;
	char* buff = (char*)malloc(1024);
	while (flags != EOF) {
		if (read_s(fd, buff, &flags, 1024) == (char*)NULL)
			continue;

		if (strncmp(buff, search, strlen(search)) == 0) {
			free(buff);
			return flags;
		}
	}
	free(buff);
	return EOF;
}

int fcopy(FILE* rfd, char* fname, char* dest_d) {
	if (rfd == (FILE*)NULL)
		return -1;
	char* sPath = (char*)malloc(MAX_PATH + 1);
	int flags;
	if ((flags = snprintf(sPath, MAX_PATH, "%s\\%s", dest_d, fname)) >= MAX_PATH) {
		snprintf(err_buff, 1024, "SNPRINTF failed: %s\n", sPath);
		write_err(err_buff);
		free(sPath);
		return -1;
	}

	FILE* wfd = fopen(sPath, "w");
	if (wfd == (FILE*)NULL) {
		snprintf(err_buff, 1024, "Failed to open writer: %s\n", sPath);
		write_err(err_buff);
		free(sPath);
		return -1;
	}
	int i;
	while ((i = fgetc(rfd)) != EOF)
		fputc(i, wfd);

	fclose(wfd);
	free(sPath);
	return 0;
}

struct tm parse_t(char* buff) {
	struct tm tm;

	tm.tm_year = 0;
	for (int i = 0; i < 4; i++) {
		tm.tm_year *= 10;
		tm.tm_year += (buff[i] - '0');
	}

	tm.tm_mon = 0;
	for (int i = 5; i < 7; i++) {
		tm.tm_mon *= 10;
		tm.tm_mon += (buff[i] - '0');
	}

	tm.tm_mday = 0;
	for (int i = 8; i < 10; i++) {
		tm.tm_mday *= 10;
		tm.tm_mday += (buff[i] - '0');
	}

	return tm;
}

struct tm gettime() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	tm.tm_year += 1900;
	tm.tm_mon += 1;
	return tm;
}

int tmcmp(struct tm t1, struct tm t2) {
	return (t1.tm_year == t2.tm_year && t1.tm_mon == t2.tm_mon && t1.tm_mday == t2.tm_mday);
}
