#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdint.h>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/types.h>


int files_counter(char *dir_name, char *subdir)
{
    DIR *dir = opendir(dir_name);
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
        if ( (strcmp(dirent_ptr->d_name, ".") != 0) && ( strcmp(dirent_ptr->d_name, "..") != 0))
        {   

            char* full_path_name = (char*) malloc( (strlen(dir_name)+ strlen(dirent_ptr->d_name)+2) *sizeof(char));
            if (full_path_name == NULL)
            {
                fprintf(stderr, "Error in malloc at sender_functions.c\n");
                return -5;
            }
            /*make the path absolute path of the file/subir*/
            strcpy(full_path_name, dir_name);               /* mirror */
            strcat(full_path_name, "/");                    /* mirror/ */
            strcat(full_path_name, dirent_ptr->d_name);     /* mirrror/name */
            
            struct stat st;
            stat(full_path_name, &st);
           // unsigned int file_size = (unsigned int)st.st_size; /* take the file size */

            int is_dir = 0;
            if ((st.st_mode & S_IFMT) == __S_IFDIR) /* check if it is a directory*/
            {
                is_dir = 1;
                //file_size = 0; /*if it is a directory make the file size = 0*/
            }
            else
            {   //printf("%s\n",full_path_name); 
        		num_of_files++;
            }
            //unsigned short int dir = 1, no_dir = 0;


            if (is_dir == 1) /* if it is a directory */
            {
                char *new_sub_dirname = NULL;
                if (subdir == NULL) /*if it is in input directory(level = 0) */
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
                    strcpy(new_sub_dirname,subdir);             /* /subdir */  
                    strcat(new_sub_dirname,"/");                /* /subdir/ */
                    strcat(new_sub_dirname,dirent_ptr->d_name); /* /subdir/next_subdir */
                }

                num_of_files = num_of_files + files_counter(full_path_name,new_sub_dirname);
                free(new_sub_dirname);
            }
            free(full_path_name);
        }
    }

    closedir(dir);
    return num_of_files;
}

int send_pathnames(char *dir_name, char *subdir,int socket)
{
	char buf[128];

    DIR *dir = opendir(dir_name);
    if (dir == NULL)
    {
        fprintf(stderr, "Error in opening input dir. \n");
        return -3;
    }
    int send_paths = 0;
    struct dirent *dirent_ptr;

    /*read all the files and subdirectories of the input directory which is the dir name*/
    while ((dirent_ptr = readdir(dir)) != NULL)
    {   
        if ( (strcmp(dirent_ptr->d_name, ".") != 0) && ( strcmp(dirent_ptr->d_name, "..") != 0))
        {
            unsigned short int name_size = 0;
            if (subdir == NULL)/*find the name-size*/
                name_size = strlen(dirent_ptr->d_name) + 1 ;
            else
                name_size = strlen(dirent_ptr->d_name) + strlen(subdir) + 2 ;

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
                strcpy(file_name,subdir);
                strcat(file_name,"/");
                strcat(file_name,dirent_ptr->d_name);
            }

            char* full_path_name = (char*) malloc( (strlen(dir_name)+ strlen(dirent_ptr->d_name)+2) *sizeof(char));
            if (full_path_name == NULL)
            {
                fprintf(stderr, "Error in malloc at sender_functions.c\n");
                return -5;
            }
            /*make the path absolute path of the file/subir*/
            strcpy(full_path_name, dir_name);               /* mirror */
            strcat(full_path_name, "/");                    /* mirror/ */
            strcat(full_path_name, dirent_ptr->d_name);     /* mirrror/name */
            
            struct stat st;
            stat(full_path_name, &st);

            int is_dir = 0;
            if ((st.st_mode & S_IFMT) == __S_IFDIR) /* check if it is a directory*/
            {
                is_dir = 1;
                //file_size = 0; /*if it is a directory make the file size = 0*/
            }
            else
            {   //printf("%s\n",full_path_name); 
        		send_paths++;
            }
            //unsigned short int dir = 1, no_dir = 0;


            if (is_dir == 1) /* if it is a directory */
            {
                char *new_sub_dirname = NULL;
                if (subdir == NULL) /*if it is in input directory(level = 0) */
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
                    strcpy(new_sub_dirname,subdir);             /* /subdir */  
                    strcat(new_sub_dirname,"/");                /* /subdir/ */
                    strcat(new_sub_dirname,dirent_ptr->d_name); /* /subdir/next_subdir */
                }

                send_paths = send_paths + send_pathnames(full_path_name,new_sub_dirname,socket);
                free(new_sub_dirname);
            }
            else
            {   sprintf(buf,"%d",name_size);
            	int bytes = write(socket, buf, strlen(buf)+1);
            	if (bytes < 0)
            	{
            		fprintf(stderr, "Error in write in send_pathnames.\n");
            		return -1;
            	}

            	if (subdir == NULL)/*if you are not on a sub directory*/
            	{
                /*write the name of the file/subdir*/
            		printf("PATH: %s \t",dirent_ptr->d_name );
                	bytes = write(socket, dirent_ptr->d_name, name_size);
                	if (bytes < 0)
                	{
                		fprintf(stderr, "Error in write in send_pathnames.\n");
            			return -1;
                	}
            	}
            	else/*if you are on a sub directory*/
           	 	{	printf("PATH: %s \t",file_name );
                	bytes = write(socket, file_name , name_size);
                	if (bytes < 0)
                	{
                		fprintf(stderr, "Error in write in send_pathnames.\n");
            			return -1;
                	}           	 		
           	 	}
           	 	printf("Version: %ld\n",st.st_mtim.tv_sec );
                sprintf(buf,"%ld",st.st_mtim.tv_sec);
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

    closedir(dir);
    return send_paths;
}
