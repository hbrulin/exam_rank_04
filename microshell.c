#include <stdlib.h>
#include <unistd.h>
#include <string.h>

///defines

#define SIDE_OUT 0
#define SIDE_IN 1

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define END 0
#define PIPE 1
#define BREAK 2

///structs

typedef struct s_list
{
	char **args;
	int length;
	int type;
	int pipes[2];
	struct s_list *previous;
	struct s_list *next;
}				t_list;

///utils
int ft_strlen(char const *s)
{
	int i = 0;
	while (s[i])
		i++;
	return (i);
}

int show_error(char const *s)
{
	if (s)
		write(STDERR, s, ft_strlen(s));
	return (EXIT_FAILURE);
}

int exit_fatal(void)
{
	show_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}

void *exit_fatal_ptr(void)
{
	exit_fatal();
	exit(EXIT_FAILURE); //nécessaire??
	return (NULL);
}


char *ft_strdup(char const *s)
{
	char *dst;
	int i;
	if (!(dst = (char *)malloc(sizeof(*dst) * (ft_strlen(s) + 1))))
		return (exit_fatal_ptr());
	i = 0;
	while (s[i])
	{
		dst[i] = s[i];
		i++;
	}
	dst[i] = 0;
	return (dst);
}



/// list fts
int list_rewind(t_list **list)
{
	while (*list && (*list)->previous)
		*list = (*list)->previous;
	return(EXIT_SUCCESS);
}

int list_clear(t_list **cmds)
{
	t_list *tmp;
	int i;
	list_rewind(cmds);
	while (*cmds)
	{
		tmp = (*cmds)->next;
		i = 0;
		while (i < (*cmds)->length)
			free((*cmds)->args[i++]);
		free((*cmds)->args);
		free(*cmds);
		*cmds = tmp;
	}
	*cmds = NULL;
	return (EXIT_SUCCESS);
}

int add_arg(t_list *cmd, char *arg)
{
	char **tmp = NULL;
	int i = 0;

	if (!(tmp = (char **)malloc(sizeof(*tmp) * (cmd->length + 2)))) //+ 2 for extra arg + Null termination
		return (exit_fatal());
	//There are existing args in a cmd if it was only terminated by END, not pipe or BREAK. We keep what's in the node and add new word.
	while (i < cmd->length)
	{
		tmp[i] = cmd->args[i];
		i++;
	}
	if (cmd->length > 0)
		free(cmd->args); //NECESSARY TO TABDEL?
	cmd->args = tmp;
	cmd->args[i++] = ft_strdup(arg);
	cmd->args[i] = 0;
	cmd->length++;
	return (EXIT_SUCCESS);
}


int list_push(t_list **list, char *arg)
{
	t_list *new;

	if (!(new = (t_list *)malloc(sizeof(*new))))
		return (exit_fatal());
	new->args = NULL;
	new->length = 0;
	new->type = END;
	new->previous = NULL;
	new->next = NULL;
	if (*list)
	{
		(*list)->next = new;
		new->previous = *list;
	}
	*list = new;
	return (add_arg(new, arg));
}


///parsing
int parse_arg(t_list **cmds, char *arg)
{
	int is_break;

	is_break = (strcmp(";", arg) == 0); //is_break is 1 if there is a break
	if (is_break && !*cmds)
		return (EXIT_SUCCESS);
	else if (!is_break && (!*cmds || (*cmds)->type > END)) //END = null termination. if there was a pipe or a break before, add node.
		return (list_push(cmds, arg)); //add a node if the arg is not a break
	else if (strcmp("|", arg) == 0)
		(*cmds)->type = PIPE; //end of node of previous iteration
	else if (is_break)
		(*cmds)->type = BREAK; //idem
	else
		return(add_arg(*cmds, arg)); //idem, since it was only null terminated, following args belong to current node pointed by *cmd, that is being pushed along everytime that there is a break or pipe
	return(EXIT_SUCCESS);
}


///execution
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
			return (exit_fatal());
	}
	pid = fork();
	if (pid < 0)
		return (exit_fatal());
	else if (pid == 0)
	{
		if (cmd->type == PIPE && dup2(cmd->pipes[SIDE_IN], STDOUT) < 0)
			return (exit_fatal());
		if (cmd->previous && cmd->previous->type == PIPE && dup2(cmd->previous->pipes[SIDE_OUT], STDIN) < 0)
			return (exit_fatal());
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
			close(cmd->pipes[SIDE_IN]);
			if (!cmd->next || cmd->type == BREAK)
				close(cmd->pipes[SIDE_OUT]);
		}
		if (cmd->previous && cmd->previous->type == PIPE)
			close(cmd->previous->pipes[SIDE_OUT]);
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
				ret = show_error("error: cd: bad arguments\n");
			else if (chdir(cur->args[1]))
			{
				ret = show_error("error: cd: cannot change directory to ");
				show_error(cur->args[1]);
				show_error("\n");
			}
		}
		else
			ret = exec(cur, env);
		if (!(*cmds)->next)
			break;
		*cmds = (*cmds)->next;
	}
	return (ret);
}

///main

int main(int argc, char **argv, char **env)
{
	t_list *cmds;
	int i;
	int ret;

	ret = EXIT_SUCCESS;
	cmds = NULL;
	i = 1;

	while (i < argc)
		parse_arg(&cmds, argv[i++]);
	if (cmds)
	{
		list_rewind(&cmds); //don't forget to rewind
		ret = exec_cmds(&cmds, env);
	}
	list_clear(&cmds);
	return (ret);
}