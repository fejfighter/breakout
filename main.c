#include <stdint.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <pty.h>


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

    printf("handle signal %d\n\n\n", *(uint8_t*)(userdata));
    *(uint8_t*)(userdata) = 0;
    //printf("return %s", ret_error->name);
    
    sd_bus_message_read(m, "uu", &pid, &retval);
    printf("returned %u %u\n", pid, retval);
    //exit(0);
    return retval;
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
    }

  sd_bus *system = NULL;
  sd_bus_error err = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = NULL;
  sd_bus_message* call = NULL;
  int con_err = 0;
  
  con_err = sd_bus_default_user(&system);
  if (con_err < 0)
  {
    printf("Failed connection %d\n", con_err);
  }

  uint8_t loop = 1;
  int build_err= 0;
  
  build_err = sd_bus_match_signal(system, NULL,
				  DESTINATION,
				  PATH,
				  INTERFACE,
				  SIGNAL,
				  signal_end, &loop);
  if(build_err < 0) {
    printf("err: %s %s\n", "not", INTERFACE);

  }
  printf("sleep: %d\n", loop);

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


  // Temporarily redirect stdout to the slave, so that the command executed in
  // the subprocess will write to the slave.
  /* int _stdout = dup(STDOUT_FILENO); */
  /* dup2(slave, STDOUT_FILENO); */
  
  //build_err = openpty(&master, &slave, NULL, NULL, NULL);
  //if(build_err != 0)
  //  printf("005errorptr %d\n", build_err);
  

  build_err = sd_bus_message_append(call, "{uh}", STDIN_FILENO, STDIN_FILENO);
  build_err = sd_bus_message_append(call, "{uh}", STDOUT_FILENO, STDOUT_FILENO);
  build_err = sd_bus_message_append(call, "{uh}", STDERR_FILENO, STDERR_FILENO);
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

  //printf("reply: %u\n", pid);
//  sd_bus_slot *slot = NULL;
  /* fd_set rfds; */
  /* struct timeval tv ={0, 0}; */
  /* char buf[4097]; */
  /* ssize_t size; */
  /* size_t count = 0; */

  while (loop)
  {

      
   build_err = sd_bus_process(system, NULL);

   if (build_err < 0) {
       log_error(build_err, "Failed to process requests: %m");
       return -1;
   }
   /* FD_ZERO(&rfds); */
   /*  FD_SET(master, &rfds); */
   /*  if (select(master + 1, &rfds, NULL, NULL, &tv)) { */
   /*    size = read(master, buf, 4096); */
   /*    buf[size] = '\0'; */
   /*    count += size; */
   /*  } */

   
   if (build_err == 0) {
       build_err = sd_bus_wait(system, UINT64_MAX);
       if (build_err < 0) {
	   log_error(build_err, "Failed to wait: %m");

       }
       continue;
   }
  }
  // Read from master as we wait for the child process to exit.
  //
  // We don't wait for it to exit and then read at once, because otherwise the
  // command being executed could potentially saturate the slave's buffer and
  // stall.
  /* while (1) { */
  /*     //if (waitpid(pid, NULL, WNOHANG) == pid) { */
  /*     if (! loop) { */
  /*     break; */
  /*   } */
  /* } */

  // Child process terminated; we flush the output and restore stdout.
  fsync(STDOUT_FILENO);
  /* dup2(_stdout, STDOUT_FILENO); */

  /* // Read until there's nothing to read, by which time we must have read */
  /* // everything because the child is long gone. */
  /* while (1) { */
  /*   FD_ZERO(&rfds); */
  /*   FD_SET(master, &rfds); */
  /*   if (!select(master + 1, &rfds, NULL, NULL, &tv)) { */
  /*     // No more to read. */
  /*     break; */
  /*   } */
  /*   size = read(master, buf, 4096); */
  /*   buf[size] = '\0'; */
  /*   count += size; */
  /* } */

  // Close both ends of the pty.
    
  sd_bus_unref(system);
  return 0;
}
