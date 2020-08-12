#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define END 0
#define PIPE 1
#define BREAK 2

#define SIDE_IN 0
#define SIDE_OUT 1

typedef struct s_list 
{
	char **args;
	int type; //DONT FORGET
	int length;
	int pipes[2];
	struct s_list *previous;
	struct s_list *next;
}				t_list;

//utils

int ft_strlen(char *s)
{
	int i = 0;
	while (s[i])
		i++;
	return(i);
}

int show_error(char *s)
{
	if (s)
		write(STDERR, s, ft_strlen(s));
	return (EXIT_FAILURE); //DONT FORGET
}

int error_fatal()
{
	show_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}

void *error_fatal_ptr()
{
	error_fatal();
	exit(EXIT_FAILURE);
	return (NULL);
}

char *ft_strdup(char *s)
{
	char *dst;
	int i = 0;

	if(!(dst = (char *)malloc(sizeof(*dst) * (ft_strlen(s) + 1))))
		return(error_fatal_ptr());
	while (s[i])
	{
		dst[i] = s[i];
		i++;
	}
	dst[i] = 0;
	return(dst);
}

//list fts
int list_rewind(t_list **list)
{
	while (*list && (*list)->previous)
		*list = (*list)->previous;
	return(EXIT_SUCCESS);
}

int list_clear(t_list **list)
{
	t_list *tmp;
	int i;
	list_rewind(list);
	while (*list) //no check if next necessary
	{
		tmp = (*list)->next;
		i = 0;
		while ((*list)->args[i])
		{
			free((*list)->args[i]);
			i++;
		}
		free((*list)->args);
		free(*list); //DONT FORGET to free the node itself
		*list = tmp;
	}
	*list = NULL; //dont forget
	return (EXIT_SUCCESS);
}

int add_arg(t_list *cmd, char *arg)
{
	char **tmp = NULL;
	int i = 0;
	if (!(tmp = (char **)malloc(sizeof(*tmp) * (cmd->length + 2)))) //only one *tmp (we want a char * multipled, not chars) + parenthesis !!!!
		return(error_fatal());
	while (i < cmd->length) //dont forget the copy
	{
		tmp[i] = cmd->args[i];
		i++;
	}
	if (cmd->length > 0)
		free(cmd->args); //why no need for tabdel??
	cmd->args = tmp;
	cmd->args[i++] = ft_strdup(arg); //i++ before
	cmd->args[i] = 0;
	cmd->length++;
	return(EXIT_SUCCESS);
}

int push_list(t_list **list, char *arg)
{
	printf("1");
	t_list *new;
	if (!(new = (t_list *)malloc(sizeof(*new))))
		return(error_fatal());
	printf("2");
	new->args = NULL;
	new->type = END;
	new->length = 0;
	new->next = NULL; //dont forget
	new->previous = NULL;
	printf("3");
	if (*list)
	{
		(*list)->next = new;
		new->previous = *list;
	}
	*list = new;
	printf("4");
	return(add_arg(new, arg));
}


int exec(t_list *cmd, char **env)
{
	//4 var to declare
	pid_t pid;
	int ret = EXIT_FAILURE;
	int status;
	int pipe_open = 0;

	//pipe tube ATTENTION CONDITIONS
	if(cmd->type == PIPE || (cmd->previous && cmd->previous->type == PIPE)) //double conditionm
	{
		pipe_open = 1;
		if (pipe(cmd->pipes)) //we pipe here
			return(error_fatal());
	}
	//fork
	pid = fork();
	if (pid < 0)
		return(error_fatal());
	else if (pid == 0)
	{
		//dup2
		if (cmd->type == PIPE && dup2(cmd->pipes[SIDE_IN], STDOUT) < 0)
			return(error_fatal());
		if (cmd->previous && cmd->previous->type == PIPE && dup2(cmd->previous->pipes[SIDE_OUT], STDIN) < 0) //DONT FORGET DOUBLE CONDITION ENCORE
			return(error_fatal());
		//execve and put into ret
		if ((ret = execve(cmd->args[0], cmd->args, env)) < 0)
		{
			show_error("error: cannot execute ");
			show_error(cmd->args[0]);
			show_error("\n");
		}
		exit(ret); // dont forget because if failure it wont exit
	}
	else
	{
		//wait
		waitpid(pid, &status, 0);
		//close pipes for current node
		if (pipe_open)
		{
			close(cmd->pipes[SIDE_IN ]);
			if (!cmd->next || cmd->type == BREAK) //close STDOUT if no next cmd or if we change node
				close(cmd->pipes[SIDE_OUT]);
		}
		//close SIDE_OUT pipes for prev node for previous iteration since it was not closed with above segment
		if (cmd->previous && cmd->previous->type == PIPE) //dont forget double condition
			close(cmd->previous->pipes[SIDE_OUT]);
		//exit status - is it really necessary? yes because ret was only saved in the child process
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
	}
	return (ret);
	
}

int exec_cmds(t_list **cmds, char **env)
{
	t_list *cur; //use a current node
	int ret = EXIT_SUCCESS;

	while (*cmds)
	{
		cur = *cmds;
		if (strcmp("cd", cur->args[0]) == 0)
		{
			ret = EXIT_SUCCESS;
			if (cur->length < 2)
				ret = show_error("error: cd: bad arguments\n"); //NO RETURN HERE
			if (chdir(cur->args[1])) //if not 0
			{
				ret = show_error("error: cd: cannot change directory to ");
				show_error(cur->args[1]);
				show_error("\n");
			}
		}
		else
			ret = exec(cur, env);
		//to change we  loop on cmds and not cur
		if (!(*cmds)->next)
			break;
		*cmds = (*cmds)->next;
	}
	return (ret);
}

int parse_arg(t_list **cmd, char *arg)
{
	int is_break;

	is_break = (strcmp(";", arg) == 0);
	if (is_break && !*cmd)
		return(EXIT_SUCCESS);
	else if (!is_break && (!*cmd || (*cmd)->type > END))
		return(push_list(cmd, arg)); //add a new node if break or pipe
	else if (strcmp("|", arg) == 0)
		(*cmd)->type = PIPE;
	else if (is_break)
		(*cmd)->type = BREAK;
	else
		return(add_arg(*cmd, arg));
	return(EXIT_SUCCESS);
}

int main(int argc, char **argv, char **env)
{
	t_list *cmds = NULL;
	int i = 1;
	int ret = EXIT_SUCCESS;

	while (i < argc)
		parse_arg(&cmds, argv[i++]);
	if (cmds)
	{
		list_rewind(&cmds);
		ret = exec_cmds(&cmds, env);
	}
	list_clear(&cmds); //dont forget
	return(ret);
}
