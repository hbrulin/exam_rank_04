#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define SIDEIN 1 //INVERSE, IMPORTANT
#define SIDEOUT 0

#define STDIN 0
#define STDOUT 1
#define STDERR 2


#define END 0
#define PIPE 1
#define BREAK 2

//struct  with 6 elements
typedef struct s_list 
{
	char **args;
	int type;
	int length;
	int pipes[2]; //dont forget
	struct s_list *next;
	struct s_list *previous;
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
	if (s) // dont forget condition
		write(STDERR, s, ft_strlen(s));
	return (EXIT_FAILURE);
}

int error_fatal()
{
	show_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return(EXIT_FAILURE);
}

void *error_fatal_ptr(void)
{
	error_fatal();
	exit(EXIT_FAILURE);
	return(NULL);
}

char *ft_strdup(char *s)
{
	char *dst;
	int i = 0;
	if (!(dst = (char *)malloc(sizeof(*dst) * (ft_strlen(s) + 1))))
		return(error_fatal_ptr());
	while (s[i])
	{
		dst[i] = s[i];
		i++;
	}
	dst[i] = 0;
	return(dst);
}

//list functions
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
	while (*list)
	{
		tmp = (*list)->next;
		i = 0;
		while ((*list)->args[i])
		{
			free((*list)->args[i]);
			i++;
		}
		free((*list)->args);
		free(*list);
		*list = tmp;
	}
	*list = NULL;
	return(EXIT_SUCCESS);
}

//adds new word to current node if no break or pipe
int add_arg(t_list *cmd, char *arg)
{
	char **tmp = NULL; //double etoile
	int i = 0;

	if (!(tmp = (char **)malloc(sizeof(*tmp) * (cmd->length + 2)))) //only one *tmp + parenthesis
		return(error_fatal());
	//copy existing args
	while (i < cmd->length)
	{
		tmp[i] = cmd->args[i];
		i++;
	}
	if (cmd->length > 0)
		free(cmd->args); //CHECK FOR LEAKS IF TABDEL NEEDED
	
	//copy back and add new arg
	cmd->args = tmp;
	cmd->args[i++] = ft_strdup(arg); //STRDUP
	cmd->args[i] = 0; //DONT FORGET THE NULL
	cmd->length++;
	return (EXIT_SUCCESS);
}

//just add new nodem, init it and call add_arg to start filling up cmd->args for this node
int list_push(t_list **list, char *arg)
{
	t_list *new;
	if (!(new = (t_list *)malloc(sizeof(*new))))
		return(error_fatal());
	new->args = NULL;
	new->length = 0;
	new->next = NULL;
	new->previous = NULL;
	new->type = END; //DONT FORGET TO SET END
	//if existing list, link it at the end of where it points
	if (*list)
	{
		(*list)->next = new; //add new at end
		new->previous = *list; //make previoyus point to previous end
	}
	*list = new; //point to head to newm we move up to next cmd
	return(add_arg(new, arg));
}

//exec
int exec(t_list *cmd, char **env)
{
	pid_t pid;
	int ret = EXIT_FAILURE;
	int status;
	int pipe_open = 0;

	if (cmd->type == PIPE || (cmd->previous && cmd->previous->type == PIPE))
	{
		pipe_open = 1;
		if (pipe(cmd->pipes))
			return (error_fatal());
	}
	pid = fork();
	if (pid < 0)
		return (error_fatal());
	else if (pid == 0)
	{
		if (cmd->type == PIPE && dup2(cmd->pipes[SIDEIN], STDOUT) < 0)
			return (error_fatal());
		if (cmd->previous && cmd->previous->type == PIPE && dup2(cmd->previous->pipes[SIDEOUT], STDIN) < 0)
			return (error_fatal());
		if ((ret = execve(cmd->args[0], cmd->args, env)) < 0)
		{
			show_error("error: cannot execute ");
			show_error(cmd->args[0]);
			show_error("\n");
		}
		exit(ret);
	}
	else
	{
		waitpid(pid, &status, 0);
		if (pipe_open)
		{
			close(cmd->pipes[SIDEIN]);
			if (!cmd->next || cmd->type == BREAK)
				close(cmd->pipes[SIDEOUT]);
		}
		if (cmd->previous && cmd->previous->type == PIPE)
			close(cmd->previous->pipes[SIDEOUT]);
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
	}
	return (ret);
}

int exec_cmds(t_list **cmds, char **env)
{
	t_list *cur;
	int ret = EXIT_SUCCESS;

	while (*cmds)
	{
		cur = *cmds;
		if (strcmp("cd", cur->args[0]) == 0)
		{
			ret = EXIT_SUCCESS;
			if (cur->length < 2)
				ret = show_error("error: cd: bad argument\n");
			else if (chdir(cur->args[1]))
			{
				ret = show_error("error: cd: cannot change directory to ");
				show_error(cur->args[1]);
				show_error("\n");
			} 
		}
		else
		{
			ret = exec(cur, env);
		}
		if (!(*cmds)->next)
			break;
		*cmds = (*cmds)->next;	
	}
	return(ret);
}

//parse, % possibilities, 3 specific returns, 1 general
int parse_arg(t_list **cmds, char *arg)
{
	int is_break = (strcmp(";", arg) == 0);
	if (is_break && !*cmds)
		return(EXIT_SUCCESS);
	else if (!is_break && (!*cmds || (*cmds)->type > END)) //ABOVE, if NOT END
		return (list_push(cmds, arg));
	else if (strcmp("|", arg) == 0)
		(*cmds)->type = PIPE;
	else if (is_break)
		(*cmds)->type = BREAK;
	else
		return(add_arg(*cmds, arg));
	return (EXIT_SUCCESS);
}


int main(int argc, char **argv, char **envp)
{
	t_list *cmds = NULL;
	int i = 1;
	int ret = EXIT_SUCCESS;

	while (i < argc)
		parse_arg(&cmds, argv[i++]);
	if (cmds)
	{
		list_rewind(&cmds);
		ret = exec_cmds(&cmds, envp);
	}
	list_clear(&cmds);
	while (1);
	return (ret);

	//system("leaks micro");
}











