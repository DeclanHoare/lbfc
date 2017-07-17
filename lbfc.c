/*
	Copyright 2017 Declan Hoare

	This file is part of lbfc.
	lbfc is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	lbfc is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with lbfc.  If not, see <http://www.gnu.org/licenses/>.

	lbfc.c - the compiler
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define COMPILER "cc"
#define CMDSTART COMPILER " -xc - -o "

#define DIE(...) { fprintf(stderr, __VA_ARGS__); free(srcname); if (src) fclose(src); if (csrc) free(csrc); if (cc) pclose(cc); if (cc_command) free(cc_command); return EXIT_FAILURE; }

const char usage[] = "usage: %s SOURCEFILE [SOURCEFILE] [...]\n";
const char error_open[] = "failed to open %s\n";
const char error_out_of_memory[] = "out of memory compiling %s\n";
const char error_target_exists[] = "output file %s already exists\n";
const char error_cc_not_open[] = "failed to launch " COMPILER "\n";
const char error_cc_failed[] = COMPILER " exited with code %d for %s\n";
const char complete[] = "%s -> %s\n";

const char templatestart[] = "#include<stdio.h>\n"
                             "#include<termios.h>\n"
                             "#include<unistd.h>\n"
                             "#include<stdint.h>\n"
                             "#include<string.h>\n"
                             "#include<stdlib.h>\n"
                             "#define b m[i]\n"
                             "int main(int argc,char**argv)"
                             "{"
                             "struct termios o,n;"
                             "tcgetattr(STDIN_FILENO,&o);"
                             "n=o;"
                             "n.c_lflag&=~(ICANON|ECHO);"
                             "tcsetattr(STDIN_FILENO,TCSANOW,&n);"
                             "uint16_t i=0;"
                             "char*m=malloc(UINT16_MAX);"
                             "memset(m,0,UINT16_MAX);";

const char templateend[] = "tcsetattr(STDIN_FILENO,TCSANOW,&o);"
                           "return EXIT_SUCCESS;"
                           "}\n";

const char templateright[] = "i++;";
const char templateleft[] = "i--;";
const char templateplus[] = "b++;";
const char templateminus[] = "b--;";
const char templatedot[] = "putchar(b);";
const char templatecomma[] = "b=getchar();";
const char templateopen[] = "while(b){";
const char templateclose[] = "}";

#define FAKEDICT(c, t) case c: *outlen = sizeof(t) - 1; return t;

const char* mapcmd(char cmd, size_t* outlen)
{
	switch (cmd)
	{
		FAKEDICT('>', templateright)
		FAKEDICT('<', templateleft)
		FAKEDICT('+', templateplus)
		FAKEDICT('-', templateminus)
		FAKEDICT('.', templatedot)
		FAKEDICT(',', templatecomma)
		FAKEDICT('[', templateopen)
		FAKEDICT(']', templateclose)
		default: return NULL;
	}
}

#undef FAKEDICT

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, usage, argv[0]);
		return EXIT_FAILURE;
	}
	for (int prog = 1; prog < argc; prog++)
	{
		char* csrc = NULL;
		char* cc_command = NULL;
		FILE* src = NULL;
		FILE* cc = NULL;
		size_t namelen = strlen(argv[prog]) + 1;
		char* srcname = malloc(namelen);
		memcpy(srcname, argv[prog], namelen);
		char* outname = strtok(argv[prog], ".");
		if (access(outname, F_OK) != -1)
			DIE(error_target_exists, outname);
		
		src = fopen(srcname, "r");
		if (!src)
			DIE(error_open, srcname);
		
		size_t len = sizeof(templatestart);
		csrc = malloc(len);
		if (!csrc)
			DIE(error_out_of_memory, srcname);
		memcpy(csrc, templatestart, sizeof(templatestart));
		
		int c;
		while ((c = fgetc(src)) != EOF)
		{
			size_t newlen;
			const char* templatecmd = mapcmd(c, &newlen);
			if (templatecmd)
			{
				len += newlen;
				csrc = realloc(csrc, len);
				if (!csrc)
					DIE(error_out_of_memory, srcname);
				strcat(csrc, templatecmd);
			}
		}
		
		fclose(src);
		src = NULL;
		
		len += sizeof(templateend) - 1;
		csrc = realloc(csrc, len);
		if (!csrc)
			DIE(error_out_of_memory, srcname);
		strcat(csrc, templateend);
		
		cc_command = malloc(sizeof(CMDSTART) + strlen(outname));
		if (!cc_command)
			DIE(error_out_of_memory, srcname);
		memcpy(cc_command, CMDSTART, sizeof(CMDSTART));
		strcat(cc_command, outname);
		
		cc = popen(cc_command, "w");
		free(cc_command);
		cc_command = NULL;
		if (!cc)
			DIE(error_cc_not_open);
		
		fwrite(csrc, 1, len - 1, cc);
		free(csrc);
		csrc = NULL;
		
		int excode = WEXITSTATUS(pclose(cc));
		cc = NULL;
		if (excode != EXIT_SUCCESS)
			DIE(error_cc_failed, excode, srcname);
		
		fprintf(stderr, complete, srcname, outname);
		free(srcname);
	}
	return EXIT_SUCCESS;
}
