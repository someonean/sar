/* Read LICENSE for information on the license */
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct dirinfo
{
	char name[20];
	int files_num;
};
struct fileinfo
{
	char name[20];
	int size;
};

void packdir(FILE *fp, char *dir_path)
{
	int i;
	struct dirent *dirent_ptr;
	struct stat st;
	struct dirinfo dir_info;
	struct fileinfo file_info;
	DIR *dirp, *dirp2;
	FILE *fp2;
	char ch, oldcwd[100];

	dirp = opendir(dir_path);
	if(!dirp)
	{
		perror(dir_path);
		exit(EXIT_FAILURE);
	}
	if(!getcwd(oldcwd, 100))
	{
		perror("Get current working directory error");
		exit(EXIT_FAILURE);
	}
	if(chdir(dir_path))
	{
		fprintf(stderr, "Cannot change current directory to ");
		perror(dir_path);
		exit(EXIT_FAILURE);
	}
	for(i = 0; dirent_ptr = readdir(dirp); i++) if(!strcmp(dirent_ptr->d_name, ".") || !strcmp(dirent_ptr->d_name, "..")) i--; /* Count all files in the directory except . and .. */
	rewinddir(dirp); /* Go back to the fist file */
	dir_info.files_num = i;
	strcpy(dir_info.name, dir_path);
	putc('d', fp); /* Write the indicator character for a directory before the dirinfo structure */
	fwrite(&dir_info, sizeof(struct dirinfo), 1, fp);
	while(dirent_ptr = readdir(dirp))
	{
		if(!strcmp(dirent_ptr->d_name, ".") || !strcmp(dirent_ptr->d_name, "..")) continue; /* Skip . and .. */

		dirp2 = opendir(dirent_ptr->d_name); /* Just to see if this is a directory. */
		if(!dirp2) /* If it's not, check if it is a file instead */
		{
			fp2 = fopen(dirent_ptr->d_name, "rb");
			if(!fp2) /* This is neither a file or a directory */
			{
				perror(dirent_ptr->d_name);
				exit(EXIT_FAILURE);
			}
			if(stat(dirent_ptr->d_name, &st))
			{
				perror(dirent_ptr->d_name);
				exit(EXIT_FAILURE);
			}
			strcpy(file_info.name, dirent_ptr->d_name);
			file_info.size = st.st_size;
			putc('f', fp); /* Write the file indicator character before the fileinfo structure */
			fwrite(&file_info, sizeof(struct fileinfo), 1, fp);
			for(i = 1; i <= st.st_size; i++)
			{
				ch = getc(fp2);
				putc(ch, fp);
			}
		}
		else /* If this really is a directory */
		{
			closedir(dirp2);
			packdir(fp, dirent_ptr->d_name); /* Use packdir recursively on any directory */
		}
	}
	closedir(dirp); /* After we are done with packing this directory, close it, */
	if(chdir(oldcwd)) /* and change directory out of it. */
	{
		perror(oldcwd);
		exit(EXIT_FAILURE);
	}
}

void unpackdir(FILE *fp, char *dir_path, int files_num)
{
	struct fileinfo file_info;
	struct dirinfo dir_info;
	int ch, i, j;
	FILE *fp2;
	char oldpwd[100];

	mkdir(dir_path, 0755); /* If mkdir returned error because this directory already exists, then good, if because of
				another reason, we will get an error later anyway. No need to check the return value of mkdir.
				;) */
	if(!getcwd(oldpwd, 100))
	{
		perror("Get present working directory error");
		exit(EXIT_FAILURE);
	}
	if(chdir(dir_path))
	{
		perror(dir_path);
		exit(EXIT_FAILURE);
	}
	for(i = 1; i <= files_num; i++)
	{
		ch = getc(fp);
		switch(ch)
		{
			case 'f':
				fread(&file_info, sizeof(struct fileinfo), 1, fp);
				fp2 = fopen(file_info.name, "wb");
				if(!fp2)
				{
					perror(file_info.name);
					exit(EXIT_FAILURE);
				}
				for(j = 1; j <= file_info.size; j++)
				{
					ch = getc(fp);
					if(ch == EOF)
					{
						fprintf(stderr, "The archive file ended too early.\n");
						exit(EXIT_FAILURE);
					}
					putc(ch, fp2);
				}
				fclose(fp2);
				break;
			case 'd':
				fread(&dir_info, sizeof(struct dirinfo), 1, fp);
				unpackdir(fp, dir_info.name, dir_info.files_num);
				break;
			case EOF:
				fprintf(stderr, "The archive file ended too early.\n");
				exit(EXIT_FAILURE);
				break;
			default:
				fprintf(stderr, "Archive file is corrupted\n");
				exit(EXIT_FAILURE);
		}
	}
	if(chdir(oldpwd)) /* After we are done with unpackging this directory we need to change back to the old current directory */
	{
		perror(oldpwd);
		exit(EXIT_FAILURE);
	}
}
int main(int argc, char **argv)
{
	FILE *fptr;
	DIR *dirptr;
	struct dirinfo dir_info;
	int ch;

	if(argc != 4)
	{
		fprintf(stderr, "Usage: sar <c|x> <archive file> <directory>\n");
		return EXIT_FAILURE;
	}
	if(argv[1][0] == 'c') fptr = fopen(argv[2], "wb");
	else if(argv[1][0] == 'x') fptr = fopen(argv[2], "rb");
	if(!fptr)
	{
		perror(argv[2]);
		return EXIT_FAILURE;
	}
	if(argv[1][0] == 'c') packdir(fptr, argv[3]);
	else if(argv[1][0] == 'x')
	{
		ch = getc(fptr);
		if(ch != 'd')
		{
			fprintf(stderr, "Directory character indicator expected (expected first byte of the file to be 'd')\n");
			exit(EXIT_FAILURE);
		}
		fread(&dir_info, sizeof(struct dirinfo), 1, fptr);
		unpackdir(fptr, argv[3], dir_info.files_num);
	}
	else
	{
		fprintf(stderr, "%s: Invalid argument\n", argv[1]);
		return EXIT_FAILURE;
	}
	fclose(fptr);
	return EXIT_SUCCESS;
}
