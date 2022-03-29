#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>


const char *sysname = "shellfyre";

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}

void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	// FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;

	while (1)
	{
		c = getchar();
		//printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);
char filename[60];

int fileExists() {
	struct stat buffer;
	int exists = stat(filename,&buffer);
	if(exists == 0){
		return 1;
	} else {
		return 0;
	}
}

int main()
{
	char cwd[100];
	getcwd(cwd,sizeof(cwd));
	strcat(cwd,"/dest_hist.txt");
	strcpy(filename,cwd);
	int file_exists = fileExists();
	FILE *fptr = fopen(filename,"a");
	if(fptr == NULL) {
		printf("Error opening cdh file");
		exit(1);
	}
	if(file_exists == 0) {
		if(getcwd(cwd,sizeof(cwd)) != NULL) {
			fprintf(fptr,"%s\n",cwd);
		}
	}
	fclose(fptr);

	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

void recursive_file_search(char *path,const char *option, const char *file){
	
	DIR *d;
	struct dirent *dir;
	d = opendir(path);

	if(d){
		while((dir = readdir(d)) != NULL){
			if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0){
					
					char buffer[1024];
					strcpy(buffer, path);
					strcat(buffer, "/");
					strcat(buffer, dir->d_name);
					
					if(strstr(dir->d_name,file) != NULL){
						printf("%s \n",buffer);
						if(strcmp(option,"-o") == 0){

							//char s[1050];
							//snprintf(s,sizeof(s),"%s %s","xdg-open",buffer);
							//system(s);
							pid_t pid2 = fork();
							if(pid2 == 0){
								char *binaryPath = "/bin/xdg-open";
								execl(binaryPath,binaryPath,buffer,NULL);
							}

							else{
								wait(NULL);
							}
						}
					}
					recursive_file_search(buffer,option,file);
			}
		}
	}
	closedir(d);
}

int getNumLine() {
	int numLine = 0;
	FILE *fptr = fopen(filename,"r");
	if (fptr == NULL) {
		printf("Error opening file\n");
		return -1;
	}
	char currentline[100];
	while(fgets(currentline,sizeof(currentline),fptr) != NULL) {
		numLine++;
	}
	return numLine;
}



void take(char *path){
	
	const char *sep = "/";
	struct stat st = {0};
	char *string = strdup(path);
	char *token = strtok(string,sep);
	while(token != NULL){
		if(stat(token,&st) == -1){
			mkdir(token,0700);
			//printf("%s \n",token);
			chdir(token);
		}
		else{
			chdir(token);
		}
		token = strtok(NULL,sep);
	}
}

int joker(){
	
	//char *arg = "/bin/notify-send";
	//char *argv[] = {arg,"\"$(curl https://icanhazdadjoke.com)\"",NULL};
       	//execv(arg,argv);
	//system("notify-send \"$(curl https://icanhazdadjoke.com)\"");	
 	//system("crontab -l | { cat; echo \"* * * * *  XDG_RUNTIME_DIR=/run/user/$(id -u) notify-send \"\"$(curl https://icanhazdadjoke.com)\"\"\"; } | crontab -");	
	char buffer[1000] = "crontab -l | { cat; echo \"* * * * * XDG_RUNTIME_DIR=/run/user/$(id -u) notify-send Joke:";
	strcat(buffer," $(curl https://icanhazdadjoke.com)\"; } | crontab -");
	printf("%s \n",buffer);
	system(buffer);	
	return 0;
}

int directoryHasChanged() {
	FILE *fptr = fopen(filename,"r");
	if (fptr == NULL) {
		printf("Error opening file\n");
		return -1;
	}
	char currentline[100];
	while(fgets(currentline,sizeof(currentline),fptr) != NULL) {
		
	}
	fclose(fptr);
	char *a = strtok(currentline,"\n");
	char cwd[100];
	if(getcwd(cwd,sizeof(cwd)) != NULL) {
		if(strcmp(cwd,a) == 0) {
			return 0;
		} else {
			return 1;
		}
	}
	return -1;
}




int process_command(struct command_t *command)
{
	if (directoryHasChanged() == 1) {
		FILE *fptr = fopen(filename,"a");
		if (fptr == NULL) {
			printf("Error opening file\n");
		}
		char cwd[100];
		if(getcwd(cwd,sizeof(cwd)) != NULL) {
			fprintf(fptr,"%s\n",cwd);
		}
		fclose(fptr);
	}
	
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0)
		return EXIT;

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			r = chdir(command->args[0]);
			if (r == -1 ) {
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			}
			
			return SUCCESS;
		}
	}

	// TODO: Implement your custom commands here
	
	// Filesearch command:	
	if(strcmp(command->name, "filesearch") == 0 && command->arg_count == 1){
		DIR *d;
		struct dirent *dir;
		d = opendir(".");
		if (d){
			while((dir = readdir(d)) != NULL ){
				if(strstr(dir->d_name,command->args[0]) != NULL){
					char buffer[1024];
					strcpy(buffer,".");
					strcat(buffer,"/");
					strcat(buffer,dir->d_name);
					printf("%s \n",buffer);
				}
			}
			closedir(d);
		}
		return SUCCESS;
	}
	else if(strcmp(command->name, "filesearch") == 0 && strcmp("-r",command->args[0]) == 0 && command->arg_count == 2){

		recursive_file_search(".",command->args[0],command->args[1]);

	}
	else if(strcmp(command->name, "filesearch") == 0 && strcmp("-o",command->args[0]) == 0 && command->arg_count == 2){

		DIR *d;
		struct dirent *dir;
		d = opendir(".");
		if (d){
			while((dir = readdir(d)) != NULL){
				if(strstr(dir->d_name,command->args[1]) != NULL){	
					
					//char s[300];
					//snprintf(s,sizeof(s),"%s %s","xdg-open",dir->d_name);
					//system(s);
					char buffer[1024];
					strcpy(buffer,".");
					strcat(buffer,"/");
					strcat(buffer,dir->d_name);
					pid_t pid1 = fork();
					if(pid1 == 0){
					char *binaryPath = "/bin/xdg-open";
					char *args[] = {binaryPath, buffer,NULL}; 
					execv(binaryPath,args);
					}
					else{
						wait(NULL);
					}
					
							
					
				}
			}
		}
	}	
	
	else if(strcmp(command->name,"filesearch") == 0 && command->arg_count == 3 &&  ((strcmp("-o",command->args[0]) && strcmp("-r",command->args[1])) || (strcmp("-r",command->args[0]) && strcmp("-o",command->args[1])))){
		
		recursive_file_search(".","-o",command->args[2]);
	}
	// Cdh command: 
	else if(strcmp(command->name, "cdh") == 0) {
		int numLine = getNumLine();
		if (numLine <= 1) {
			printf("No directory history\n");
		} else {
		if (numLine > 10) {
			//Getting the last 10 cd in an array
			char lastTen[10][100];
			FILE *fptr = fopen(filename,"r");
			if (fptr ==NULL) {
				printf("Error opening file\n");
			}
			int count = 0;
			int count2 = -1;
			char currentline[100];
			while(fgets(currentline,sizeof(currentline),fptr) != NULL) {
				count++;
				if(count > numLine-10) {
					count2++;
					strcpy(lastTen[count2],currentline);
					//printf("%s",lastTen[count2]);
				}
			}
			fclose(fptr);
			//Overwriting the file to have only the last 10
			
			fptr = fopen(filename,"w");
			if (fptr == NULL) {
				printf("Error opening file");
			}
			int i;
			for (i = 0; i < 10; i++) {
				//printf("%d\n",i);
				//printf("%s",lastTen[i]);
				fprintf(fptr,"%s",lastTen[i]);
			}
			fclose(fptr);
		
		}
		numLine = getNumLine();
		//printf("%d\n",numLine);
		FILE *fptr = fopen(filename,"r");
		if(fptr == NULL) {
			printf("Error opening read file\n");
		}
		char currentline[100];
		int count = -1;
		char letterArray[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'};
		char directories[numLine][100];
		while (fgets(currentline,sizeof(currentline),fptr) != NULL) {
			count++;
			strcpy(directories[count],currentline);
			printf(" %c %d) ~%s",letterArray[count],count+1,currentline);
		}
		fclose(fptr);
		printf("Select directory by letter or number: \n");
		char chosenDirectory;
		int fd[2];
		pipe(fd);
		pid_t pid7 = fork();
		if (pid7 == 0) {
			close(fd[0]);
			scanf("%c", &chosenDirectory);
			write(fd[1], &chosenDirectory, sizeof(chosenDirectory));
			close(fd[1]);
			exit(0);
		} else {
			wait(NULL);
			close(fd[1]);
			read(fd[0], &chosenDirectory, sizeof(chosenDirectory));
			close(fd[0]);
		}
		int chosenDirectoryIndex = -1;
		if(chosenDirectory >= 97 && chosenDirectory <= 96+numLine) {
			chosenDirectoryIndex = chosenDirectory-97;
		} else if(chosenDirectory >= 49 && chosenDirectory < 48+numLine) {
			chosenDirectoryIndex = chosenDirectory-49;
		}
		char *a = strtok(directories[chosenDirectoryIndex],"\n");
		r = chdir(a);
		if (r == -1) {
			printf("-%s: %s: %s\n", sysname, a, strerror(errno));
		}
		}
	}	
	// Take command:
	if(strcmp(command->name,"take") == 0){
		
		take(command->args[0]);
	}

	if(strcmp(command->name, "joker") == 0){
		int result = joker();
	}

	pid_t pid = fork();

	if (pid == 0) // child
	{	
		//print_command(command);
		//printf("%d \n",command->arg_count);
		//int i;
		//for(i = 0; i < command->arg_count; i++){
		//	printf("%s \n",command->args[i]);
		//}
		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		/// TODO: do your own exec with path resolving using execv()	
		char binaryPath[50] = "/bin/";
                strcat(binaryPath,command->args[0]);
		
		execv(binaryPath,command->args);

		exit(0);
	}
	else
	{
		/// TODO: Wait for child to finish if command is not running in background
		if(!command->background){
			wait(NULL);
		}
		return SUCCESS;
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}


