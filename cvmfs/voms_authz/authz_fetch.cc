/**
 * This file is part of the CernVM File System.
 */

#define __STDC_FORMAT_MACROS
#include "authz_fetch.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <cassert>

#include "clientctx.h"
#include "logging.h"
#include "platform.h"
#include "util_concurrency.h"
#include "util/posix.h"

using namespace std;  // NOLINT


AuthzExternalFetcher::AuthzExternalFetcher(
  const string &fqrn,
  const string &progname)
  : fqrn_(fqrn)
  , progname_(progname)
  , fd_send_(-1)
  , fd_recv_(-1)
  , pid_(-1)
  , fail_state_(false)
{
  InitLock();
}

AuthzExternalFetcher::AuthzExternalFetcher(
  const string &fqrn,
  int fd_send,
  int fd_recv)
  : fqrn_(fqrn)
  , fd_send_(fd_send)
  , fd_recv_(fd_recv)
  , pid_(-1)
  , fail_state_(false)
{
  InitLock();
}


AuthzExternalFetcher::~AuthzExternalFetcher() {
  int retval = pthread_mutex_destroy(&lock_);
  assert(retval == 0);

  if (fd_send_ >= 0)
    close(fd_send_);
  if (fd_recv_ >= 0)
    close(fd_recv_);

  if (pid_ > 0) {
    uint64_t now = platform_monotonic_time();
    int statloc;
    do {
      retval = waitpid(pid_, &statloc, WNOHANG);
      if (platform_monotonic_time() > (now + kChildTimeout)) {
        LogCvmfs(kLogAuthz, kLogSyslogWarn | kLogDebug,
                 "authz helper %s unresponsive, killing", progname_.c_str());
        kill(pid_, SIGKILL);
        break;
      }
    } while (retval == 0);
  }
}


void AuthzExternalFetcher::EnterFailState() {
  LogCvmfs(kLogAuthz, kLogSyslogErr | kLogDebug,
           "authz helper %s enters fail state, no more authentication",
           progname_.c_str());
  fail_state_ = true;
}


/**
 * Uses execve to start progname_.  The started program has stdin and stdout
 * connected to fd_send_ and fd_recv_ and the CVMFS_... environment variables
 * set.  Special care must be taken when we call fork here in an unknown state
 * of the client.  Therefore we can't use ManagedExec (we can't use malloc).
 *
 * A failed execve is not caught by this routine.  It will be caught in the
 * next step, when mother and child start talking.
 */
void AuthzExternalFetcher::ExecHelper() {
  int pipe_send[2];
  int pipe_recv[2];
  MakePipe(pipe_send);
  MakePipe(pipe_recv);
  char *argv0 = strdupa(progname_.c_str());
  char *argv[] = {argv0, NULL};
  char *envp[] = {NULL};
  int max_fd = sysconf(_SC_OPEN_MAX);
  assert(max_fd > 0);
  LogCvmfs(kLogAuthz, kLogDebug | kLogSyslog, "starting authz helper %s",
           argv0);

  pid_t pid = fork();
  if (pid == 0) {
    // Child process, close file descriptors and run the helper
    int retval = dup2(pipe_send[0], 0);
    assert(retval == 0);
    retval = dup2(pipe_recv[1], 1);
    assert(retval == 1);
    for (int fd = 2; fd < max_fd; fd++)
      close(fd);

    execve(argv0, argv, envp);
    syslog(LOG_USER | LOG_ERR, "failed to start authz helper %s (%d)",
           argv0, errno);
    abort();
  }
  assert(pid > 0);
  close(pipe_send[0]);
  close(pipe_recv[1]);

  // Don't receive a signal if the helper terminates
  signal(SIGPIPE, SIG_IGN);
  pid_ = pid;
  fd_send_ = pipe_send[1];
  fd_recv_ = pipe_recv[0];
}


AuthzStatus AuthzExternalFetcher::FetchWithinClientCtx(
  const std::string &membership,
  AuthzToken *authz_token,
  unsigned *ttl)
{
  assert(ClientCtx::GetInstance()->IsSet());
  uid_t uid;
  gid_t gid;
  pid_t pid;
  ClientCtx::GetInstance()->Get(&uid, &gid, &pid);

  MutexLockGuard lock_guard(lock_);
  if (fail_state_)
    return kAuthzNoHelper;


  if (fd_send_ < 0)
    ExecHelper();
  assert((fd_send_ >= 0) && (fd_recv_ >= 0));


  return kAuthzUnknown;
}


/**
 * Establish communication link with a forked authz helper.
 */
bool AuthzExternalFetcher::Handshake() {
  string json_msg = string("{") +
    "\"cvmfs_authz_v1\": {" +
    "\"revision\": 1, " +
    "\"fqrn\": \"" + fqrn_ + "\", " +
    "}}";
  bool retval = Send(json_msg);
  if (!retval)
    return false;
  return false;
}


void AuthzExternalFetcher::InitLock() {
  int retval = pthread_mutex_init(&lock_, NULL);
  assert(retval == 0);
}


bool AuthzExternalFetcher::Send(const string &msg) {
  // Line format: 4 byte protocol version, 4 byte length, message
  struct {
    uint32_t version;
    uint32_t length;
  } header;
  header.version = kProtocolVersion;
  header.length = msg.length();
  unsigned raw_length = sizeof(header) + msg.length();
  unsigned char *raw_msg = reinterpret_cast<unsigned char *>(
    alloca(raw_length));
  memcpy(raw_msg, &header, sizeof(header));
  memcpy(raw_msg + sizeof(header), msg.data(), header.length);

  bool retval = SafeWrite(fd_send_, raw_msg, raw_length);
  if (!retval)
    EnterFailState();
  return retval;
}


bool AuthzExternalFetcher::Recv(string *msg) {
  uint32_t version;
  ssize_t retval = SafeRead(fd_recv_, &version, sizeof(version));
  if (retval != int(sizeof(version))) {
    EnterFailState();
    return false;
  }
  if (version != kProtocolVersion) {
    LogCvmfs(kLogAuthz, kLogSyslogErr | kLogDebug,
             "authz helper uses unknown protocol version %u", version);
    EnterFailState();
    return false;
  }

  uint32_t length;
  retval = SafeRead(fd_recv_, &length, sizeof(length));
  if (retval != int(sizeof(length))) {
    EnterFailState();
    return false;
  }

  msg->clear();
  char buf[kPageSize];
  unsigned nbytes = 0;
  while (nbytes < length) {
    retval = SafeRead(fd_recv_, buf, kPageSize);
    if (retval < 0) {
      LogCvmfs(kLogAuthz, kLogSyslogErr | kLogDebug,
               "read failure from authz helper %s", progname_.c_str());
      EnterFailState();
      return false;
    }
    nbytes += retval;
    msg->append(buf, retval);
    if (nbytes < kPageSize)
      break;
  }
  if (nbytes < length) {
    LogCvmfs(kLogAuthz, kLogSyslogErr | kLogDebug,
             "short read from authz helper %s", progname_.c_str());
    EnterFailState();
    return false;
  }

  return true;
}
