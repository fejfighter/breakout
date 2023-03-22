#include <stdint.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <pty.h>
#include <ctype.h>

#include <systemd/sd-bus.h>
#define _cleanup_(f) __attribute__((cleanup(f)))

/* This is equivalent to:
 * busctl call org.freedesktop.systemd1 /org/freedesktop/systemd1 \
 *       org.freedesktop.systemd1.Manager GetUnitByPID $$
 *
 * Compile with 'cc -lsystemd print-unit-path.c'
 */

#define DESTINATION "org.freedesktop.Flatpak"
#define PATH        "/org/freedesktop/Flatpak/Development"
#define INTERFACE   "org.freedesktop.Flatpak.Development"
#define MEMBER      "HostCommand"
#define SIGNAL      "HostCommandExited"
#define ARGV_0      "breakout"


int signal_end(sd_bus_message *m,
		   void *userdata,
		   sd_bus_error *ret_error) {
    uint32_t pid, retval;

    sd_bus_message_read(m, "uu", &pid, &retval);
    sd_event_exit((sd_event*)userdata, retval);

    return retval;
}

static struct termios orig;

void reset_stdin(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
}

#include <endian.h>
#include <errno.h>

int send_out(sd_event_source *s,
	      int fd,
	      uint32_t revents,
	      void *userdata) {
    char c = 'a';
    ssize_t n =1;
    int master = *(int*)(userdata);


    n = read(STDIN_FILENO, &c, 1);
    if (n < 0) {
	if (errno == EAGAIN)
	    return 0;
	return -errno;
    }
    /* if (iscntrl(c)) { */
    /*   printf("%d\n", c); */
    /* } else { */
    /*   printf("%d ('%c')\n", c, c); } */


    write(master, &c, n);
    fsync(master);
    return 0;
}

int write_out(sd_event_source *s,
	      int fd,
	      uint32_t revents,
	      void *userdata) {

    void *buffer;
    ssize_t n;
    int sz;

    /* UDP enforces a somewhat reasonable maximum datagram size of 64K, we can just allocate the buffer on the stack */
    if (ioctl(fd, FIONREAD, &sz) < 0)
	return -2;
    buffer = alloca(sz);

    n = read(fd, buffer, sz);
    if (n < 0) {
	if (errno == EAGAIN)
	    return 0;

	return -errno;
    }

    fwrite(buffer, 1, n, stdout);
    fflush(stdout);
    return 0;
}


static int log_error(int error, const char *message) {
    //errno = -error;
  fprintf(stderr, "%s: %m\n", message);
  return error;
}

int main(int argc, char **argv) {

    bool symlink = false;
    char* arg0 = basename(argv[0]);

    //printf("%s, == %s\n", ARGV_0, arg0);
   if (strncmp(ARGV_0, arg0, strlen(arg0))) {
	symlink = true;
    }
    else if(argc == 1)
    {
	printf(" Nothing listed\n");
	return -1;
    }

  sd_bus *system = NULL;
  sd_bus_error err = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = NULL;
  sd_bus_message* call = NULL;
  int con_err = 0;
  sd_event *ev = NULL;

  con_err = sd_bus_default_user(&system);
  if (con_err < 0)
  {
    printf("Failed connection %d\n", con_err);
  }



  sd_event_new(&ev);
  sd_bus_attach_event(system, ev, 0);

  int build_err= 0;

  build_err = sd_bus_match_signal(system, NULL,
				  DESTINATION,
				  PATH,
				  INTERFACE,
				  SIGNAL,
				  signal_end,
				  ev);
  if(build_err < 0) {
    printf("err: %s %s\n", "not", INTERFACE);

  }


  char cwd[100];
  getcwd(cwd,100);
  uint32_t pid = 0;
  uint32_t flags = 0;


  build_err = sd_bus_message_new_method_call(system, &call,
					     DESTINATION,
					     PATH,
					     INTERFACE,
					     MEMBER);
  if(build_err < 0)
    return log_error(build_err, "couldn't constrcut call");

  if(build_err < 0)
	printf("0010error1 %d: %p\n", build_err, call);

  build_err = sd_bus_message_append_array(call, 'y', cwd, sizeof(cwd));
  if(build_err < 0)
	printf("001error1 %d: %p\n", build_err, call);
  sd_bus_message_close_container(call);

  build_err = 0;
  sd_bus_message_open_container(call, 'a', "ay");

  if (symlink)
  {
      build_err = sd_bus_message_append_array(call, 'y', arg0, strlen(arg0)+1);
      //printf("%d -%s-%ld-\n", 0, arg0, strlen(arg0)+1);

  }
  for ( int i = 1; i < argc; i++)
  {
      build_err = sd_bus_message_append_array(call, 'y', argv[i], strlen(argv[i])+1);
      //printf("%d -%s-%ld-\n", i, argv[i], strlen(argv[i])+1);
      if(build_err < 0)
	  printf("002error1 %d\n", build_err);
  }
  sd_bus_message_close_container(call);


  build_err = sd_bus_message_open_container(call, 'a', "{uh}");
  if(build_err < 0)
    printf("004a error1 %d\n", build_err);


  int32_t master, slave;
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  tcgetattr(STDIN_FILENO, &orig);
  cfmakeraw(&term);
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
  atexit(reset_stdin);
  build_err = openpty(&master, &slave, NULL, NULL, NULL);
  if(build_err != 0)
   printf("005errorptr %d\n", build_err);



  build_err = sd_bus_message_append(call, "{uh}", STDIN_FILENO, slave);
  build_err = sd_bus_message_append(call, "{uh}", STDOUT_FILENO, slave);
  build_err = sd_bus_message_append(call, "{uh}", STDERR_FILENO, slave);

  sd_event_source *ev_src = NULL;
  sd_event_add_io(ev, &ev_src, master, EPOLLIN|EPOLLRDHUP, write_out, NULL);
  sd_event_add_io(ev, &ev_src, STDIN_FILENO, EPOLLIN, send_out, &master);

  if(build_err < 0)
    printf("005error1 %d\n", build_err);
  sd_bus_message_close_container(call);


  sd_bus_message_open_container(call, 'a', "{ss}");
  build_err = sd_bus_message_append(call, "{ss}", "DEBUG", "1");
  if(build_err < 0)
    printf("007error1 %d\n", build_err);
  sd_bus_message_close_container(call);

  build_err = sd_bus_message_append(call, "u", flags);

  if(build_err < 0)
    printf("008error1 %d\n", build_err);


  build_err = sd_bus_call(system,
			  call,
			  0,
			  &err,
			  &reply);
  if(build_err < 0) {
    printf("err: %s %s\n", err.name , err.message);
    return log_error(build_err, "error at call");
  }

  sd_bus_message_read(reply, "u", &pid);

  return   sd_event_loop(ev);
}
