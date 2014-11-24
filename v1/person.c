#include "person.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

Person* p = NULL;
int fd = -1;
int find = -1;

static void print_usage(const char *prog)
{
	fprintf(stderr, "usage: %s [-f file] [-w] [-s value] attr_name\n", prog);
	fprintf(stderr, "  -f file  : person file, default is './person.dat'\n");
	fprintf(stderr, "  -w       : watch mode\n");
	fprintf(stderr, "  -s value : set the value for the given 'attr_name'\n");
}

void sig_action(int signo, siginfo_t* siginfo, void* dumy)
{
	int offset;
	const char* attr_name;

	switch(signo)
	{
	case SIGINT:
	case SIGTERM:
		p->watchers[find] = 0;
		munmap(p, sizeof(Person));
		close(fd);
		exit(0);
		break;

	case SIGUSR1:
		offset = siginfo->si_value.sival_int;
		//printf("offset : %d\n", offset);

		attr_name = person_lookup_attr_with_offset(offset);
		//printf("attr_name : %s\n", attr_name);
		
		if(person_attr_is_integer(attr_name))
		{
			int* ptr = (int*)((char*)p + offset);
			printf("%s: '%d' from '%d'\n", attr_name, *ptr, siginfo->si_pid);
		}
		else
		{
			char* ptr = (char*)((char*)p + offset);
			printf("%s: '%s' from '%d'\n", attr_name, ptr, siginfo->si_pid);
		}

		break;
	}
}

void do_watch_mode()
{
	int i;
	struct sigaction act_new;
	struct sigaction act_old;

	act_new.sa_sigaction = sig_action;
	sigemptyset(&act_new.sa_mask);  
	act_new.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &act_new, &act_old); 
	sigaction(SIGTERM, &act_new, &act_old); 
	sigaction(SIGUSR1, &act_new, &act_old); 

	find = 0;
	for(i = 0; i < NOTIFY_MAX; i++)
	{
		if(p->watchers[i] == 0)
		{
			find = i;
			break;
		}	
	}

	p->watchers[find] = getpid();
	//printf("pid : %d\n", p->watchers[find]);

	printf("watching...\n");
	while(1)
	{
		pause();
	}
}

void memory_map(const char* fname)
{
	Person person;
	int nwrite = 0;

	fd = open(fname, O_RDWR, 0644);
	if(fd == -1) 
	{
		fd = open(fname, O_RDWR|O_CREAT, 0644);
		if(fd == -1) 
		{
			perror("File Open Error");
			exit(-1);
		}

		memset(&person, 0, sizeof(Person));
		nwrite = write(fd, &person, sizeof(Person));
		if(nwrite != sizeof(Person))
		{
			perror("File Write Error");
			exit(-1);
		}
	}

	p = (Person*)mmap(0, sizeof(Person), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if(p == MAP_FAILED)
	{
		perror("mmap error");
		exit(-1);
	}
}

void set_and_notify(const char* attr_name, const char* set_value)
{
	int i;
	int offset = -1;
	offset = person_get_offset_of_attr(attr_name);
	//printf("offset of %s : %d\n", attr_name, offset);
	if(offset == -1)
	{
		printf("invalid attr name '%s'\n", attr_name);
		exit(-1);
	}

	if(person_attr_is_integer(attr_name))
	{
		int* ptr = (int*)((char*)p + offset);
		if(set_value != NULL) *ptr = atoi(set_value);
		printf("%d\n", *ptr);
	}
	else
	{
		char* ptr = (char*)((char*)p + offset);
		if(set_value != NULL) strcpy(ptr, set_value);
		printf("%s\n", ptr);
	}

	if(set_value == NULL) return;

	for(i = 0; i < NOTIFY_MAX; i++)
	{
		int pid;
		pid = p->watchers[i];
		//printf("pid : %d\n", pid);
		if(pid != 0)
		{
			int ret;
			union sigval val;
			val.sival_int = offset;
			ret = sigqueue(pid, SIGUSR1, val); 
			if(ret != 0)
			{
				perror("sigqueue error");
			}
		}
	}
}

int main(int argc, char **argv)
{
	const char *file_name;
	const char *set_value;
	const char *attr_name;
	int watch_mode;

	/* Parse command line arguments. */
	file_name = "./person.dat";
	set_value = NULL;
	watch_mode = 0;
	while (1)
	{
		int opt;

		opt = getopt(argc, argv, "ws:f:");
		if(opt < 0)
			break;

		switch (opt)
		{
		case 'w':
			watch_mode = 1;
			break;
		case 's':
			set_value = optarg;
			break;
		case 'f':
			file_name = optarg;
			break;
		default:
			print_usage(argv[0]);
			return -1;
			break;
		}
	}

	if(!watch_mode && optind >= argc)
	{
		print_usage(argv[0]);
		return -1;
	}

	attr_name = argv[optind];

	memory_map(file_name);

	if(watch_mode)
	{
		do_watch_mode();
	}
	else
	{
		set_and_notify(attr_name, set_value);
	}

	munmap(p, sizeof(Person));
	close(fd);

	return 0;
}




