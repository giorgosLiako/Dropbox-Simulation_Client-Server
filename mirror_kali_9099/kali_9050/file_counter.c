#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

/*function to implement the  communication protocol from sender's side*/
int communication_sender_protocol(char *dir_name, char *subdir)
{
    DIR *dir = opendir(dir_name);
    if (dir == NULL)
    {
        fprintf(stderr, "Error in opening input dir. \n");
        return -3;
    }
    int files_counter = 0;
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
            unsigned int file_size = (unsigned int)st.st_size; /* take the file size */

            int is_dir = 0;
            if ((st.st_mode & S_IFMT) == __S_IFDIR) /* check if it is a directory*/
            {
                is_dir = 1;
                file_size = 0; /*if it is a directory make the file size = 0*/
            }
            else
            {   printf("%s\n",full_path_name); 
                files_counter++;
            }
            unsigned short int dir = 1, no_dir = 0;


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

                files_counter = files_counter + communication_sender_protocol(full_path_name,new_sub_dirname);
                free(new_sub_dirname);
            }
        }
    }

    closedir(dir);
    return files_counter;
}

int main(void)
{
    printf("FILES: %d\n",communication_sender_protocol(".", NULL) );
}