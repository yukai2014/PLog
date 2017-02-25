/*
 * Copyright [2012-2015] YuKai
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ProcessLogTest.cpp
 *
 *  Created on: Jun 10, 2016
 *      Author: yukai
 *		 Email: yukai2014@gmail.com
 * 
 * Description:
 *
 */

#include <iostream>
#include <strstream>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <memory.h>
#include <assert.h>
#include <cstdlib>
#include <limits.h>
#include <memory>
#include <execinfo.h>

#include "./logging.h"
#include "../src/Logging.h"

using claims::common::Logging;
using namespace std;

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// #define DIRECT_CALL_OLD_SIGNAL_HANDLER

#define GETCURRENTTIME(t) \
    timeval t;              \
    gettimeofday(&t, NULL);

const int REPEAT_TIME = 300;
static char to_send[508] = "        yukaizhenshishuaisdffffffffffffffwefwwehagweviwbo bwuehwhef2wuegf9qgfob2wbf2gefgwqeofiyg";


class A{
 public:
  A(){
    cout<<"A"<<endl;
  }
  ~A(){
    cout<<"~A"<<endl;
  }
};

class B{
 public:
  B(){
    cout<<"B"<<endl;
  }
  ~B(){
    cout<<"~B"<<endl;
  }
};

class C{
 public:
  C(){
    cout<<"C"<<endl;
  }
  ~C(){
    cout<<"~C"<<endl;
  }
};

class D{
 public:
  D(){
    cout<<"D"<<endl;
  }
  ~D(){
    cout<<"~D"<<endl;
  }
};


void f(C c,D d){
  B b;
  A a;
  B* pb=&b;
}


class LoggerStream : public ostrstream {
 public:
  LoggerStream(char * buf, int len)
 : ostrstream(buf, len),
   buf_(buf),
   len_(len) {
  }

  ~LoggerStream() {
    // do the real fucking output
    cout << buf_;
  }

 private:
  char *buf_;
  int len_;
};

class TestDestruction{
 public:
  TestDestruction(int i):i_(i){
    cout<<"start"<<i<<endl;
  }
  ~TestDestruction(){
    cout<<"end "<<-i_<<endl;
  }

  void f(){
    cout<<'f'<<endl;
  }
  int i_;
};

/*
 * @param start: the timeval of start time
 * @return : the elapsed time(ms) from start, accurate to us!
 */
static double GetElapsedTime(struct timeval start) {
  GETCURRENTTIME(end);
  return (end.tv_usec - start.tv_usec) / 1000.0 +
      (end.tv_sec - start.tv_sec) * 1000.0;
}


pthread_spinlock_t spin_lock;

int fd[2];

enum PipeFlag{
  READ=0, WRITE
};


int32_t FlushAllToFile(const int fd, void* buffer, ssize_t length) {
  assert (length > 0);
  int64_t all_written_len = 0;
  while(all_written_len < length) {
    ssize_t written_len = write(fd,
                                static_cast<char*>(buffer)+all_written_len,
                                length - all_written_len);
    if (unlikely(written_len < 0)) {
      cerr<<"failed to write (buffer:"<<buffer<<", pos:"<<all_written_len<<") to fd:"<<fd<<". "<<strerror(errno)<<endl;
      return -1;
    } else {
      all_written_len += written_len;
    }
  }
  return 0;
}

void LogServer(){
  //  usleep(1);
  pthread_t thread_id = pthread_self();
  cout<<thread_id<<". i'm in server"<<endl;

  close(fd[WRITE]);
  long buf_length;
  char length[sizeof(int)];
  const long max_buf_len = 100000;
  char buf[max_buf_len];
  ssize_t read_bytes = 0 ;
  int time = 0;
  int pos = 0;
  while(1) {
    if (0 == (read_bytes=read(fd[READ],length, sizeof(length)))) {
      cout<<thread_id<<" meet EOF. stored "<<pos<<" bytes in buffer:"<<buf<<endl;
      return;
    } else if (0 > read_bytes) {
      perror("failed to read buffer length from pipe.");
    } else {
      //      cout<<"received buf header: "<<static_cast<int>(static_cast<char*>(length)[0])<<", "
      //                                   <<static_cast<int>(static_cast<char*>(length)[1])<<", "
      //                                   <<static_cast<int>(static_cast<char*>(length)[2])<<", "
      //                                   <<static_cast<int>(static_cast<char*>(length)[3])<<endl;
      buf_length = (*static_cast<int*>(static_cast<void*>(length)));
      //cout<<"read-buffer length is:"<<buf_length<<endl;

      if (buf_length + 1 > max_buf_len ) { // consider the last '\n' by default
        cerr<<"too large buf_length:"<<buf_length + 1<<endl;
        exit(1);
      }
      if (pos+buf_length + 1 > max_buf_len){   // consider the last '\n' by default
        cout<<thread_id<<" buffer has stored "<<pos<<" bytes and is almost full"<<endl;
        static int proc_log_fd = open("proc_log", O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (proc_log_fd == -1) {
          perror("failed to open log file");
          return;
        }
        if (unlikely(0 != FlushAllToFile(proc_log_fd, buf, pos))){
          cerr<<"failed to Flush buffer to log file"<<endl;
          close(proc_log_fd);
          return ;
        }else {
          cout<<"flush all data in buffer to log file"<<endl;
          memset(buf, 0, sizeof(char)*pos);
          pos =0;
        }
      }
      read_bytes=read(fd[READ], buf+pos, buf_length);
      pos+=buf_length;
      buf[pos++] = '\n';
    }

    /*{
      read_bytes=read(fd[READ], buf, sizeof(to_send));
      if (unlikely(read_bytes <= 0)){
        return;
      }
    }*/
  }
}

void LogClient(){
  pthread_t thread_id = pthread_self();
  cout<<thread_id<<". i'm in client"<<endl;
//  close(fd[READ]);
//  int buf_length = sizeof(to_send);
//  *static_cast<int*>(static_cast<void*>(to_send)) = buf_length-sizeof(buf_length);
//  cout<<"In client, buf_length:"<<buf_length<<endl;
//  cout<<"buf header: "
//      <<static_cast<int>(to_send[0])<<", "
//      <<static_cast<int>(to_send[1])<<", "
//      <<static_cast<int>(to_send[2])<<", "
//      <<static_cast<int>(to_send[3])<<endl;
//  ssize_t written_bytes = 0;
  GETCURRENTTIME(start);
  for(int time = 0; time < REPEAT_TIME; ++time) {
    pthread_spin_lock(&spin_lock);
    PLog(plog::INFO)<<thread_id<<" zuimeibushi"<<3<<"xiayutian";
    pthread_spin_unlock(&spin_lock);
  }
  cout<<thread_id<<" of plog use "<<GetElapsedTime(start)<< " ms"<<endl;
}

void GlogClient(){
  pthread_t thread_id = pthread_self();
  cout<<thread_id<<". i'm in glog client"<<endl;
  GETCURRENTTIME(start);
  for(int time = 0; time < REPEAT_TIME; ++time) {
    LOG(INFO)<<to_send+8<<std::endl;
  }

  cout<<thread_id<<" of glog use "<<GetElapsedTime(start)<< " ms"<<endl;
}

typedef void (*sig_handler) (int);
static sig_handler old_sig_handler[_NSIG] = {0};

void PrintStackTrace(){
  const int SIZE = 100;
  int j, nptrs;
  void *buffer[100];
  char **strings;

  nptrs = backtrace(buffer, SIZE);
  printf("backtrace() returned %d addresses\n", nptrs);

  /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
                would produce similar output to the following: */

  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  for (j = 0; j < nptrs; j++)
    printf("%s\n", strings[j]);

  free(strings);
}

void Output(int i){
  cout<<"output some stack info... "<<endl;
  PrintStackTrace();
}

void SetDefaultSignalHandler(int sig_num){
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = SIG_DFL;
  if (sigaction(sig_num, &sa, NULL) != 0){
    cerr<<"failed to set default signal handler of signal:"<<sig_num<<". "<<strerror(errno)<<endl;
    exit(EXIT_FAILURE);
  }
}

void SetPreviousSignalHandler(int sig_num) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler= old_sig_handler[sig_num];
  if (!sa.sa_handler || 0 != sigaction(sig_num, &sa, NULL)){
    cerr<<"failed to set previous signal handler of signal:"<<sig_num<<". "<<strerror(errno)<<endl;
    cout<<"going to set default signal handler"<<endl;
    SetDefaultSignalHandler(sig_num);
    //    exit(EXIT_FAILURE);
  }
}

void SIGINTHandler(int sig_num) {
#ifndef DIRECT_CALL_OLD_SIGNAL_HANDLER
  SetPreviousSignalHandler(sig_num);
  cout<<"reset signal handler to previous one"<<endl;
#endif
  cout<<"hello, there is SIGINT handler"<<endl;
  cout<<"signal is:"<<sig_num<<endl;

  PrintStackTrace();

#ifdef DIRECT_CALL_OLD_SIGNAL_HANDLER
  assert(0 != old_sig_handler[sig_num]);
  (*old_sig_handler[sig_num])(sig_num);
#endif

#ifndef DIRECT_CALL_OLD_SIGNAL_HANDLER
  kill(getpid(), sig_num);
#endif
}

void ResetSignalHandler(int sig_num, sig_handler new_handler) {
  struct sigaction old_sa;
  if (sigaction(sig_num, NULL, &old_sa ) != 0) {
    cerr<<"failed to get old signal handler of signal:"<<sig_num<<". "<<strerror(errno)<<endl;
    exit(EXIT_FAILURE);
  }
  old_sig_handler[sig_num] = old_sa.sa_handler;

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = new_handler;
  if (sigaction(sig_num, &sa, NULL) != 0) {
    cerr<<"failed to set new signal handler of signal:"<<sig_num<<". "<<strerror(errno)<<endl;
    exit(EXIT_FAILURE);
  } else {
    cout<<"reset signal handler for signal "<<sig_num<<endl;
  }
}

void ErrorFunction(){
  cout<<"error is coming.."<<endl;
  int i = 1/0;
  char* p = 0;

  cout<<i<<" "<<*p<<endl;
}

void ErrorFunction1(){
  int i = 0;
  i+=2;
  ErrorFunction();
}

int main(int argc, char** argv) {
  {
    char x = true;
    cout<<x<<endl;    //
    cout<<static_cast<char>(true)<<endl;  //
    //cout<<reinterpret_cast<char>(true)<<endl; // ERROR: invalid cast from type ‘bool’ to type ‘char’
    char xx = 155;
    int xxx = xx;
    cout<<xxx<<endl;  // -101
    cout<<static_cast<int>(static_cast<char>(155))<<endl; // -101
    //cout<<reinterpret_cast<int>(static_cast<char>(155))<<endl;    // ERROR: invalid cast from type ‘char’ to type ‘int’

    int big = -(INT_MAX - 10000);
    cout<<static_cast<long>(big)<<endl; // -2147473647
  }
  {
    C c;
    D d;
    D d1;
    //    f(c,d);
    TestDestruction(1).f(); // temporary object' life time is only this statement.
    TestDestruction td1(3);
    TestDestruction td2(23);
  }
  {
    char* buffer = new char[100];
    LoggerStream(buffer, 100)<<"1"<<sizeof(buffer)<<'a'<<endl;  // 18a
    LoggerStream(buffer, 100)<<"2"<<2*sizeof(buffer)<<'b'<<endl;    // 216b
    LoggerStream(buffer, 100)<<"3"<<3*sizeof(buffer)<<'c'<<endl;    // 324c
  }
  cout << "yukai"<<endl;
  int old_umask = umask(0);
  cout<<old_umask<<endl;
  if (unlikely(0 != umask(old_umask))) {
    cout<<"someone else modified the umask."<<endl;
    return 0;
  }
//  if (0 == pipe(fd)) {
//    cout<<"create pipe successfully"<<endl;
//  } else {
//    perror("failed to create pipe.");
//  }
//
//  if (0 == fork()) {
//    LogServer();
//    return 0;
//  }
  usleep(1);
  pthread_spin_init(&spin_lock, 0);

  std::thread th1(LogClient);
  std::thread th2(LogClient);
  std::thread th3(LogClient);

  th1.join();
  th2.join();
  th3.join();
  pthread_spin_destroy(&spin_lock);


  //  LogClient();
  cout<<"all client threads are going to die!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<endl;

  PLog(plog::INFO)<<"this is plog ";
  PLog(plog::INFO)<<"this is plog ";
  PLog(plog::INFO)<<"this is plog ";
  PLog(plog::INFO)<<"this is plog ";

  /////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////

  Logging claims_logging(argv[0]);
  {
    std::thread th1(GlogClient);
    std::thread th2(GlogClient);
    std::thread th3(GlogClient);

    th1.join();
    th2.join();
    th3.join();
  }
  //    sleep(100);
  //  ErrorFunction1();

  /////////////////////////
  cout<<"process id:"<<getpid()<<endl;
  cout<<"========reset signal handler function=========="<<endl;
//  ResetSignalHandler(SIGINT, SIGINTHandler);
//  ResetSignalHandler(SIGILL, SIGINTHandler);
//  ResetSignalHandler(SIGSEGV, SIGINTHandler);
//  ResetSignalHandler(SIGTERM, SIGINTHandler);
//  ResetSignalHandler(SIGFPE, SIGINTHandler);

//  sleep(100);
//  ErrorFunction1();
  //  sleep(3);
  //  std::thread th11([](){int i = 1/1;});
  //  std::thread th112(ErrorFunction1);
  //  int i = 1/0;
//  sleep(3);
  return 0;
}
