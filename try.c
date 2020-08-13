#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SIDE_IN 0
#define SIDE_OUT 1

#define END 0
#define PIPE 1
#define BREAK 2

typedef struct s_list
{
	char **args;
	int type;
	int length;
	int pipes[2];
	struct s_list *next;
	struct s_list *previous;
}				t_list;

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
	return (EXIT_FAILURE);
}

int error_fatal()
{
	show_error("error: fatal\n");
	exit(EXIT_FAILURE);
	return(EXIT_FAILURE);
}

void *error_fatal_ptr()
{
	error_fatal();
	exit(EXIT_FAILURE);
	return(NULL);
}

char *ft_strdup(char *s)
{
	char *dst = NULL;
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

int list_rewind(t_list **list)
{
	while (*list && (*list)->previous)
		*list = (*list)->previous;
	return(EXIT_SUCCESS);
}

int list_clear(t_list **list)
{
	t_list *tmp = NULL;
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
	return (EXIT_SUCCESS);
}

int add_arg(t_list *cmd, char *arg)
{
	char **tmp = NULL;
	int i = 0;
	if(!(tmp = (char **)malloc(sizeof(*tmp) *(cmd->length + 2))))
		return(error_fatal());
	while (i < cmd->length)
	{
		tmp[i] = cmd->args[i];
		i++;
	}
	if (cmd->length > 0)
		free(cmd->args);
	cmd->args = tmp;
	cmd->args[i++] = ft_strdup(arg);
	cmd->args[i] = 0;
	cmd->length++;
	return(EXIT_SUCCESS);
}

int push_list(t_list **list, char *arg)
{
	t_list *new = NULL;
	if(!(new = (t_list *)malloc(sizeof(*new))))
		return(error_fatal());
	new->args = NULL;
	new->type = END;
	new->length = 0;
	new->next = NULL;
	new->previous = NULL;
	if (*list)
	{
		(*list)->next = new;
		new->previous = *list;
	}
	*list = new;
	return(add_arg(new, arg));
}


int exec(t_list *cmd, char **env)
{
	pid_t pid;
	int ret = EXIT_FAILURE;
	int status;
	int pipe_open = 0;

	if (cmd->type == PIPE || (cmd->previous && cmd->previous->type == PIPE))
	{
		pipe_open = 1;
		if (pipe(cmd->pipes)) //no comparison!!
			return(error_fatal());
	}
	pid = fork();
	if (pid < 0)
		return(error_fatal());
	else if (pid == 0)
	{
		if (cmd->type == PIPE && dup2(cmd->pipes[SIDE_IN], STDOUT) < 0)
			return(error_fatal());
		if (cmd->previous && cmd->previous->type == PIPE && dup2(cmd->previous->pipes[SIDE_OUT], STDIN) < 0)
			return(error_fatal());
		if ((ret = execve(cmd->args[0], cmd->args, env)) < 0) //ret here
		{
			show_error("error: cannot execute ");
			show_error(cmd->args[0]);
			show_error("\n");
		}
		exit(ret);
	}
	else
	{
		waitpid(pid, &status, 0); // DONT FORGET
		if (pipe_open)
		{
			close(cmd->pipes[SIDE_IN]);
			if (!cmd->next || cmd->type == BREAK)
				close(cmd->pipes[SIDE_OUT]);
		}
		if (cmd->previous && cmd->previous->type == PIPE)
			close(cmd->previous->pipes[SIDE_OUT]); //close previous not current
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
	}
	return(ret);

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
			if (cur->length < 2)
			{
				ret = show_error("error: cd: bad arguments\n"); // dont forget ret =
			}
			if (chdir(cur->args[1]))
			{
				ret = show_error("error: cd: canot change directory to "); //dont forget ret =
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

int parse_args(t_list **cmds, char *arg)
{
	int is_break;
	
	is_break = (strcmp(";", arg) == 0);

	if (is_break && !*cmds)
		return(EXIT_SUCCESS);
	else if (!is_break && (!*cmds || (*cmds)->type > END))
		return(push_list(cmds, arg));
	else if (strcmp("|", arg) == 0)
		(*cmds)->type =PIPE;
	else if (is_break)
		(*cmds)->type = BREAK;
	else
	{
		return(add_arg(*cmds, arg));
	}
	return(EXIT_SUCCESS);
}
int main(int argc, char **argv, char **env)
{
	t_list *cmds = NULL; //set TO NULL!!!
	int ret = EXIT_SUCCESS;
	int i = 1;

	while (i < argc)
		parse_args(&cmds, argv[i++]);
	if (cmds)
	{
		list_rewind(&cmds);
		ret = exec_cmds(&cmds, env);
	}
	list_clear(&cmds);
	return (ret);
}























