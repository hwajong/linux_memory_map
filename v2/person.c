#include "person.h"
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define FILE_MODE 0644

// 전역변수 선언 
static Person *person = NULL;
static int fd_mp = -1;
static int idx = -1;

static void print_usage(const char *prog) {
	fprintf(stderr, "usage: %s [-f file] [-w] [-s value] attr_name\n", prog);
	fprintf(stderr, "  -f file  : person file, default is './person.dat'\n");
	fprintf(stderr, "  -w       : watch mode\n");
	fprintf(stderr, "  -s value : set the value for the given 'attr_name'\n");
}

/*
   sighandler
   시그널을 받으면 호출되는 함수 정의.
*/
static void sighandler(int signo, siginfo_t * siginfo, void *d) {
	int offset;
	const char *attr_name;

	switch (signo) {
	case SIGUSR1:
		// 전달받은 offset 값으로 속성값을 찾고 출력한다.
		offset = siginfo->si_value.sival_int;
		attr_name = person_lookup_attr_with_offset(offset);

		// 값이 정수일 경우
		if(person_attr_is_integer(attr_name)) {
			int *ptr_int = (int *)((char *)person + offset);
			printf("%s: '%d' from '%d'\n", attr_name, *ptr_int, siginfo->si_pid);
		}
		// 값이 문자열일 경우 
		else {
			char *ptr_char = (char *)((char *)person + offset);
			printf("%s: '%s' from '%d'\n", attr_name, ptr_char, siginfo->si_pid);
		}
		break;

	case SIGINT:
	case SIGTERM:
		// 저장했던 pid 를 지운다.
		person->watchers[idx] = 0;

		// 메모리맵 해제
		munmap(person, sizeof(Person));

		// 파일close
		if(fd_mp) {
			close(fd_mp);
		}

		exit(0);
		break;
	}
}

/*
  watch 모드로 동작한다.
*/
static void watch_mode_proc() {
	struct sigaction new_sa, old_sa;
	int i;

	new_sa.sa_sigaction = sighandler;
	sigemptyset(&new_sa.sa_mask);
	new_sa.sa_flags = SA_SIGINFO;

	// sigaction 핸들러 등록
	sigaction(SIGUSR1, &new_sa, &old_sa);
	sigaction(SIGINT, &new_sa, &old_sa);
	sigaction(SIGTERM, &new_sa, &old_sa);

	// pid 저장을 위해 watchers 배열에서 값이 0인 인덱스를 찾는다.
	// 없으면 0 번째에 저장.
	idx = 0;
	for(i = 0; i < NOTIFY_MAX; i++) {
		if(person->watchers[i] == 0) {
			idx = i;
			break;
		}
	}

	// pid 저장
	person->watchers[idx] = getpid();

	printf("watching...\n");
	while (1) {
		// 시그널을 받을 때까지 기다린다.
		pause();
	}
}

/*
  입력받은 속성값을 메모리맵에 저장한 후 wait 모드로 동작하고 있는
  프로세스 모두에게 시그널을 보낸다.
*/
void set_mode_proc(const char *attr_name, const char *value) {
	int i;
	int offset = -1;

	offset = person_get_offset_of_attr(attr_name);
	if(offset == -1) {
		printf("invalid attr name '%s'\n", attr_name);
		exit(1);
	}

	if(person_attr_is_integer(attr_name)) {
		int *ptr_int = (int *)((char *)person + offset);
		if(value != NULL) {
			*ptr_int = atoi(value);
		}
		printf("%d\n", *ptr_int);
	}
	else {
		char *ptr_char = (char *)((char *)person + offset);
		if(value != NULL) {
			strcpy(ptr_char, value);
		}
		printf("%s\n", ptr_char);
	}

	if(value == NULL) {
		return;
	}

	//wait 모드로 동작하고 있는 프로세스 모두에게 시그널을 보낸다.
	for(i = 0; i < NOTIFY_MAX; i++) {
		int pid;
		pid = person->watchers[i];
		if(pid != 0) {
			int result;
			union sigval sig_val;
			sig_val.sival_int = offset;
			result = sigqueue(pid, SIGUSR1, sig_val);
			if(result != 0) {
				perror("sigqueue");
				exit(1);
			}
		}
	}
}

/*
  파일을 열고 메모리맵을 설정한다.
*/
static void setup_memory_map(const char *file_name) {
	Person person_empty;
	int n = 0;

	fd_mp = open(file_name, O_RDWR, FILE_MODE);
	if(fd_mp == -1) {
		// 지정한 파일이 없으면 파일을 만들고
		// Person 구조체 변수를 초기화 한후 write 한다.
		fd_mp = open(file_name, O_RDWR | O_CREAT, FILE_MODE);

		memset(&person_empty, 0, sizeof(Person));
		n = write(fd_mp, &person_empty, sizeof(Person));
		if(n != sizeof(Person)) {
			perror("write");
			exit(1);
		}
	}

	// memory map 연결 
	person = (Person *) mmap(0, sizeof(Person), PROT_WRITE | PROT_READ, MAP_SHARED, fd_mp, 0);
	if(person == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
}

int main(int argc, char **argv) {
	const char *file_name;
	const char *set_value;
	const char *attr_name;
	int watch_mode;

	/* Parse command line arguments. */
	file_name = "./person.dat";
	set_value = NULL;
	watch_mode = 0;
	while (1) {
		int opt;

		opt = getopt(argc, argv, "ws:f:");
		if(opt < 0)
			break;

		switch (opt) {
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

	if(!watch_mode && optind >= argc) {
		print_usage(argv[0]);
		return -1;
	}

	attr_name = argv[optind];

	// 파일을 열고 메모리맵을 설정한다.
	setup_memory_map(file_name);

	if(watch_mode)
		watch_mode_proc();
	else
		set_mode_proc(attr_name, set_value);

	munmap(person, sizeof(Person));
	close(fd_mp);

	return 0;
}
