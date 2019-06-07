#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdint.h>
#include <dirent.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

extern int errno;
extern pthread_mutex_t str_mtx;

/*function to make the directories which are exited in the pathname of each file */
int directory_maker(char* path)
{
	int slash_counter = 0;
	for (int i = 0 ; i < strlen(path); i++) /*count the slashes of the pathname*/
		if (path[i] == '/')
			slash_counter++;
	

	char buf[128];
	int start = 0;
	int end;
	while ( slash_counter >= 1)/*for each slash make the directory,until the last one because it is the file*/
	{
		int j =0;
		for(int i = start ; i < strlen(path); i++)
		{	
			if (path[i] == '/')
			{	slash_counter--;
				end = i;		/*mark the end */
				start = i+1;	/*mark the start for the next loop */
				break;
			}
			j++;
		}
		for (int i = 0 ; i < end ; i++) /*take the directory until you reach end*/
			buf[i] = path[i];
		buf[end] = '\0';

		DIR* dir = opendir(buf); /*check if the directory exist*/
		if (dir) 
		{	
    		closedir(dir); /*if exist close it and return*/
		} 
		else if (ENOENT == errno) /*if it does not exist make it and return*/
		{
			if (mkdir(buf, 0777)!= 0)
				return -1;
		}

	}
	return 0;
}

/*function to count the files of a directory and all the subdirectories with recursion*/
int files_counter(char *dir_name, char *subdir)
{
    DIR *dir = opendir(dir_name); /*open the directory*/
    if (dir == NULL)
    {
        fprintf(stderr, "Error in opening input dir. \n");
        return -3;
    }
    int num_of_files = 0;
    struct dirent *dirent_ptr;

    /*read all the files and subdirectories of the input directory which is the dir name*/
    while ((dirent_ptr = readdir(dir)) != NULL)
    {   
    	/*ignore the . and .. */
        if ( (strcmp(dirent_ptr->d_name, ".") != 0) && ( strcmp(dirent_ptr->d_name, "..") != 0))
        {   
            char* full_path_name = (char*) malloc( (strlen(dir_name)+ strlen(dirent_ptr->d_name)+2) *sizeof(char));
            if (full_path_name == NULL)
            {
                fprintf(stderr, "Error in malloc at sender_functions.c\n");
                return -5;
            }
            /*make the path absolute path of the file/subir*/
            pthread_mutex_lock(&str_mtx);
            strcpy(full_path_name, dir_name);               /* mirror */
            strcat(full_path_name, "/");                    /* mirror/ */
            strcat(full_path_name, dirent_ptr->d_name);     /* mirrror/name */
            pthread_mutex_unlock(&str_mtx);

            struct stat st;
            stat(full_path_name, &st);

            int is_dir = 0;
            if ((st.st_mode & S_IFMT) == __S_IFDIR) /* check if it is a directory*/
            {
                is_dir = 1; /*mark that it is a directory*/
            }
            else /*else count the file in the sum*/
            {   
        		num_of_files++;
            }


            if (is_dir == 1) /* if it is a directory */
            {
                char *new_sub_dirname = NULL;
                if (subdir == NULL) /*if it is in the first directory(level = 0) */
                {
                    new_sub_dirname = (char *)malloc((strlen(dirent_ptr->d_name) + 1) * sizeof(char));
                    if (new_sub_dirname == NULL)
                    {
                        free(full_path_name);
                        fprintf(stderr, "Error in malloc at sender_functions.c\n");
                        return -5;
                    }
                    strcpy(new_sub_dirname,dirent_ptr->d_name); /* just take the name */
                }
                else /*if it is not in level = 0 */
                {
                    new_sub_dirname = (char *)malloc((strlen(dirent_ptr->d_name) + strlen(subdir) + 2) * sizeof(char));
                    if (new_sub_dirname == NULL)
                    {
                        fprintf(stderr, "Error in malloc at sender_functions.c\n");
                        return -1;
                    }
                    /*make the path*/
                    pthread_mutex_lock(&str_mtx);
                    strcpy(new_sub_dirname,subdir);             /* /subdir */  
                    strcat(new_sub_dirname,"/");                /* /subdir/ */
                    strcat(new_sub_dirname,dirent_ptr->d_name); /* /subdir/next_subdir */
                	pthread_mutex_unlock(&str_mtx);
                }
                /*call recursively the function for the subdir*/
                num_of_files = num_of_files + files_counter(full_path_name,new_sub_dirname);
                free(new_sub_dirname);
            }
            free(full_path_name);
        }
    }

    closedir(dir); /*close the directory*/
    return num_of_files; /*return the counter */
}

/*function that sends the pathname-version of each file in directory and subdirectories using recursion */
int send_pathnames(char *dir_name, char *subdir,int socket)
{
	char buf[128];

    DIR *dir = opendir(dir_name); /*open the directory */
    if (dir == NULL)
    {
        fprintf(stderr, "Error in opening input dir. \n");
        return -3;
    }
    int send_paths = 0;
    struct dirent *dirent_ptr;

    /*read all the files and subdirectories of the input directory which is the dir name*/
    while ((dirent_ptr = readdir(dir)) != NULL)
    {   /*ignore . and .. */
        if ( (strcmp(dirent_ptr->d_name, ".") != 0) && ( strcmp(dirent_ptr->d_name, "..") != 0))
        {
            unsigned short int name_size = 0;
            if (subdir == NULL)/*find the name-size*/
                name_size = strlen(dirent_ptr->d_name) + 1 ; /*the file is in the first directory */
            else
                name_size = strlen(dirent_ptr->d_name) + strlen(subdir) + 2 ;/*the file is in a subdir*/

            char *file_name = NULL;
            
            if (subdir != NULL)/*if you are on a sub directory*/
            {
                file_name = (char*) malloc( (strlen(subdir)+ strlen(dirent_ptr->d_name)+ 2 ) * sizeof(char));
                if (file_name == NULL)
                {
                    fprintf(stderr,"Error in malloc at sender_functions.c\n");
                    return -9;
                }
                /*send the path as file_name (path starts after mirror directory)*/
                pthread_mutex_lock(&str_mtx);
                strcpy(file_name,subdir);
                strcat(file_name,"/");
                strcat(file_name,dirent_ptr->d_name);
                pthread_mutex_unlock(&str_mtx);
            }

            char* full_path_name = (char*) malloc( (strlen(dir_name)+ strlen(dirent_ptr->d_name)+2) *sizeof(char));
            if (full_path_name == NULL)
            {
                fprintf(stderr, "Error in malloc at sender_functions.c\n");
                return -5;
            }
            /*make the path absolute path of the file/subdir*/
            pthread_mutex_lock(&str_mtx);
            strcpy(full_path_name, dir_name);               /* mirror */
            strcat(full_path_name, "/");                    /* mirror/ */
            strcat(full_path_name, dirent_ptr->d_name);     /* mirrror/name */
            pthread_mutex_unlock(&str_mtx);

            struct stat st;
            stat(full_path_name, &st);

            int is_dir = 0;
            if ((st.st_mode & S_IFMT) == __S_IFDIR) /* check if it is a directory*/
            {
                is_dir = 1; /*mark that it is a directory*/
            }
            else
            {    /*count the file */
        		send_paths++;
            }

            if (is_dir == 1) /* if it is a directory */
            {
                char *new_sub_dirname = NULL;
                if (subdir == NULL) /*if it is in first directory(level = 0) */
                {
                    new_sub_dirname = (char *)malloc((strlen(dirent_ptr->d_name) + 1) * sizeof(char));
                    if (new_sub_dirname == NULL)
                    {
                        free(full_path_name);
                        fprintf(stderr, "Error in malloc at sender_functions.c\n");
                        return -5;
                    }
                    strcpy(new_sub_dirname,dirent_ptr->d_name); /* just take the name */
                }
                else /*if it is not in level = 0 */
                {
                    new_sub_dirname = (char *)malloc((strlen(dirent_ptr->d_name) + strlen(subdir) + 2) * sizeof(char));
                    if (new_sub_dirname == NULL)
                    {
                        fprintf(stderr, "Error in malloc at sender_functions.c\n");
                        return -1;
                    }
                    /*make the path*/
                    pthread_mutex_lock(&str_mtx);
                    strcpy(new_sub_dirname,subdir);             /* /subdir */  
                    strcat(new_sub_dirname,"/");                /* /subdir/ */
                    strcat(new_sub_dirname,dirent_ptr->d_name); /* /subdir/next_subdir */
                	pthread_mutex_unlock(&str_mtx);
                }
                /*call recursion for this subdirectory */
                send_paths = send_paths + send_pathnames(full_path_name,new_sub_dirname,socket);
                free(new_sub_dirname);
            }
            else
            {   pthread_mutex_lock(&str_mtx);
            	sprintf(buf,"%d",name_size);
            	pthread_mutex_unlock(&str_mtx);
            	/*send the size of the pathname */
            	int bytes = write(socket, buf, strlen(buf)+1);
            	if (bytes < 0)
            	{
            		fprintf(stderr, "Error in write in send_pathnames.\n");
            		return -1;
            	}

            	if (subdir == NULL)/*if you are not on a sub directory*/
            	{
                	/*send the path of the file*/
                	bytes = write(socket, dirent_ptr->d_name, name_size);
                	if (bytes < 0)
                	{
                		fprintf(stderr, "Error in write in send_pathnames.\n");
            			return -1;
                	}
            	}
            	else/*if you are on a sub directory*/
           	 	{	/*send the path strating after dir_name */
                	bytes = write(socket, file_name , name_size);
                	if (bytes < 0)
                	{
                		fprintf(stderr, "Error in write in send_pathnames.\n");
            			return -1;
                	}           	 		
           	 	}
           	 	pthread_mutex_lock(&str_mtx);
                sprintf(buf,"%ld",st.st_mtim.tv_sec);
                pthread_mutex_unlock(&str_mtx);
            	/*send the version of the file , the version is the last modification time*/
            	bytes = write(socket, buf, strlen(buf)+1);
            	if (bytes < 0)
            	{
            	   	fprintf(stderr, "Error in write in send_pathnames.\n");
            		return -1;
            	}

            }
            if (subdir != NULL)
               	free(file_name);

            free(full_path_name);
        }
    }

    closedir(dir); /*close directory*/
    return send_paths; /*send the count of files*/
}
