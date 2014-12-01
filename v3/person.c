#include "person.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

// 글로벌 변수 정의
Person *gpPerson = NULL;
int gFd = 0;
int gIndex = 0;

static void print_usage(const char *prog)
{
	fprintf(stderr, "usage: %s [-f file] [-w] [-s value] attr_name\n", prog);
	fprintf(stderr, "  -f file  : person file, default is './person.dat'\n");
	fprintf(stderr, "  -w       : watch mode\n");
	fprintf(stderr, "  -s value : set the value for the given 'attr_name'\n");
}

// 시그널을 받으면 호출되는 함수 정의.
void sigact(int signo, siginfo_t *siginfo, void *context)
{
	switch (signo)
	{
	case SIGUSR1:
		{
			// 파라미터로 받은 offset 값으로 속성명을 찾는다.
			int offset = siginfo->si_value.sival_int;
			const char *attr_name = person_lookup_attr_with_offset(offset);

			// 속성 값이 정수이면 
			if(person_attr_is_integer(attr_name))
			{
				int *p = (int *)((char *)gpPerson + offset);
				printf("%s: '%d' from '%d'\n", attr_name, *p, siginfo->si_pid);
			}
			// 속성 값이 스트링이면 
			else
			{
				char *p = (char *)((char *)gpPerson + offset);
				printf("%s: '%s' from '%d'\n", attr_name, p, siginfo->si_pid);
			}
		}
		break;

	case SIGINT:
	case SIGTERM:
		// 메모리맵 watchers 배열에 저장했던 pid 를 0으로 셋팅한다.
		gpPerson->watchers[gIndex] = 0;

		// um-map.
		munmap(gpPerson, sizeof(Person));

		// close file.
		if(gFd) close(gFd);

		exit(0);
		break;
	}
}

// watch 모드로 동작 구현.
// 메모리맵에 pid 를 저장하고 시그널을 기다린다.
void watchMode()
{
	// sigaction 설정.
	struct sigaction sa, sa_old;
	sa.sa_sigaction = sigact;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	// sigaction 시그널 핸들러 등록.
	sigaction(SIGUSR1, &sa, &sa_old);
	sigaction(SIGINT, &sa, &sa_old);
	sigaction(SIGTERM, &sa, &sa_old);

	// pid 가 저장되지 않은 watchers 배열을 찾는다.
	for(int i = 0; i < NOTIFY_MAX; i++)
	{
		if(!gpPerson->watchers[i])
		{
			gIndex = i;
			break;
		}
	}

	// pid 를 저장한다. 
	gpPerson->watchers[gIndex] = getpid();

	printf("watching...\n");
	while (1)
	{
		// wait for signal
		pause();
	}
}

// aname 속성값을 찾아 있으면 val 로 셋팅.
// 대기 모드로 동작하고 있는 프로세스에게 시그널 전성.
void sigMode(const char *aname, const char *val)
{
	// offset 검색
	int offset = person_get_offset_of_attr(aname);
	if(offset == -1)
	{
		printf("invalid attr name '%s'\n", aname);
		exit(1);
	}

	// 속성값이 정수값 이면
	if(person_attr_is_integer(aname))
	{
		int *p = (int *)((char *)gpPerson + offset);
		if(val != NULL) *p = atoi(val);
		printf("%d\n", *p);
	}
	// 속성값이 스트링값 이면
	else
	{
		char *p = (char *)((char *)gpPerson + offset);
		if(val != NULL) strcpy(p, val);
		printf("%s\n", p);
	}

	// set 모드가 아닐경우 바로 리턴.
	if(val == NULL) return;

	// 대기 모드로 동작하고 있는 프로세스에게 시그널 전성.
	for(int i = 0; i < NOTIFY_MAX; i++)
	{
		int pid = gpPerson->watchers[i];
		if(pid != 0)
		{
			union sigval sig_val;
			sig_val.sival_int = offset;
			int rc = sigqueue(pid, SIGUSR1, sig_val);
			if(rc != 0)
			{
				perror("sigqueue");
				exit(1);
			}
		}
	}
}

// 파일을 열고 메모리맵을 매핑한다.
void openMemoryMap(const char *file)
{
	const int mode = 0644;
	gFd = open(file, O_RDWR, mode);
	if(gFd == -1)
	{
		// 지정한 파일이 없으면 파일을 만들고
		// Person 구조체 변수를 초기화 한후 write 한다.
		gFd = open(file, O_RDWR | O_CREAT, mode);

		Person p;
		memset(&p, 0, sizeof(p));
		int n = write(gFd, &p, sizeof(p));
		if(n != sizeof(p))
		{
			perror("write");
			exit(1);
		}
	}

	// 파일과 메모리맵을 매핑한다.
	gpPerson = (Person *) mmap(0, sizeof(Person), PROT_WRITE | PROT_READ, MAP_SHARED, gFd, 0);
	if(gpPerson == MAP_FAILED)
	{
		perror("mmap");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	const char *file_name;
	const char *set_value;
	const char *attr_name;
	int        watch_mode;

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
		return 0;
	}

	attr_name = argv[optind];

	openMemoryMap(file_name);

	if(watch_mode) watchMode();
	else sigMode(attr_name, set_value);

	munmap(gpPerson, sizeof(Person));
	close(gFd);

	return 0;
}
