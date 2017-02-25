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
 * IPCProxy.h
 *
 *  Created on: Feb 24, 2017
 *      Author: yukai
 *		 Email: yukai2014@gmail.com
 * 
 * Description:
 *
 */

#ifndef PLog_SRC_IPCPROXY_H_
#define PLog_SRC_IPCPROXY_H_

#include <iostream>
#include <assert.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/shm.h>
#include <thread>
#include <sys/sendfile.h>
#include <unistd.h>
using std::cerr;
using std::cout;
using std::endl;


namespace plog{

static const int buffer_max_size = 30000;

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

class IPCProxy{
 protected:
  IPCProxy(){}
  virtual ~IPCProxy(){}
 public:
  virtual int Send(/*int fd,*/ const void* buffer, int len) = 0;
  virtual int Receive(/*int in_fd, int out_fd, void* buffer, int len*/) = 0;

 public:
  int32_t FlushAllToFile(const int fd, const void* buffer, ssize_t length) {
    assert (length >= 0);
    if (length == 0) return 0;

    int64_t all_written_len = 0;
    while(all_written_len < length) {
      ssize_t written_len = write(fd,
                                  static_cast<const char*>(buffer)+all_written_len,
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

  // get the length of string end with '\0'
  int GetRealLen(const void* _data) {
    const char* data = static_cast<const char*>( _data);
    int len = 0;
    for (; data[len] != '\0'; ++len);
    return len;
  }
};

class PipeProxy : public IPCProxy {
  enum PipeFlag { READ = 0, WRITE};

 public:
  PipeProxy(){
    cout<<"PipeProxy()"<<endl;
    log_fd_ = open("proc_log", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    cout<<"log file fd is:"<<log_fd_<<endl;
    if (log_fd_ < 3) {
      perror("failed to open log file");
      exit(-1);
    }

    int fd[2];
    if (0 == pipe(fd)) {
      cout<<"create pipe successfully"<<endl;
      PipeProxy::in_fd_ = fd[PipeFlag::READ];
      PipeProxy::out_fd_ = fd[PipeFlag::WRITE];
    } else {
      perror("failed to create pipe.");
    }

    if (0 == fork()) {
      pthread_t thread_id = pthread_self();
      cout<<thread_id<<". i'm in plog server"<<endl;
      while(1){
        int ret=  Receive();
        if (ret < 0) {perror("something error"); }
        else if (ret == 0) { cout<<"pipe closed "<<endl; break ;}
        else { /* correct action, so do nothing */}
      }
      exit(0);
    }
  }

 public:
  virtual int Send(const void* buffer, int len) {
    assert(out_fd_ >= 3);
    close(in_fd_);
    assert(len>0 && "argument len must greater than zero");
    int real_len = GetRealLen(buffer);
    *const_cast<int*>(static_cast<const int*>(buffer-sizeof(int))) = real_len;  // the length variable is behind the buffer
    return FlushAllToFile(out_fd_, buffer-sizeof(int), real_len + sizeof(int));
  }

  virtual int Receive(/*void* buffer, int len*/){
    if (in_fd_ < 3){
      cout<<"in_fd_: "<<in_fd_<<endl;
      assert(false&&"in_fd_ invalid ");
    }
    close(out_fd_);

    pthread_t thread_id = pthread_self();
    int read_bytes = 0, send_bytes = 0;
    char length[sizeof(int)];
    int pos = 0;

    if (0 == (read_bytes=read(in_fd_,length, sizeof(length)))) {
      cout<<thread_id<<" meet EOF. stored "<<pos<<" bytes in buffer:"<<buffer_<<endl;
      send_bytes = 0;
    } else if (0 > read_bytes) {
      perror("failed to read buffer length from pipe.");
      send_bytes = -1;
    } else {
      //      cout<<"received buf header: "<<static_cast<int>(static_cast<char*>(length)[0])<<", "
      //                                   <<static_cast<int>(static_cast<char*>(length)[1])<<", "
      //                                   <<static_cast<int>(static_cast<char*>(length)[2])<<", "
      //                                   <<static_cast<int>(static_cast<char*>(length)[3])<<endl;
      int buf_length = (*static_cast<int*>(static_cast<void*>(length)));
      cout<<"read-buffer length is:"<<buf_length<<endl;

      if (buf_length + 1 > buffer_max_size ) { // consider the last '\n' by default
        cerr<<"too large buf_length:"<<buf_length + 1<<endl;
        assert(false);
        send_bytes = -1;
      }

      send_bytes = splice( in_fd_, NULL, log_fd_, NULL,
                           buf_length, SPLICE_F_MOVE|SPLICE_F_MORE);
      if (send_bytes < 0) {
        perror("sendfile error");
//        if (write(log_fd_, "wolaixie",9) < 0) {perror("failed to write log file");}
//        else {cout<<"write is ok"<<endl;}
        exit(-1);
      }
      else cout<<"succeed to splice data whose length is "<<buf_length<<endl;
    }
    return send_bytes;
  }

 private:
  static int in_fd_ ;
  static int out_fd_ ;
  static int log_fd_;
  static int initialized_;
  static char buffer_[buffer_max_size];
};

class SHMProxy: public IPCProxy{
 public:
  SHMProxy():IPCProxy(){

  }
  virtual ~SHMProxy(){}
  virtual int Send(const void* buffer, int len) {}
  virtual int Receive(/*void* buffer, int len*/){}
};

int PipeProxy::in_fd_ = -1;
int PipeProxy::out_fd_ = -1;
int PipeProxy::log_fd_ = -1;
int PipeProxy::initialized_ = 0;
char PipeProxy::buffer_[buffer_max_size];

enum IPCType {PIPE = 0, SHAREMEMORY =1 };

static IPCProxy* GetProxy(IPCType type) {
  if (type == PIPE) {
    static PipeProxy pp;
    return &pp;
  }
  else if (type == SHAREMEMORY) {
    static SHMProxy sp;
    return &sp;
  }
  else {
    assert(false && "not supported type");
    return (IPCProxy*)NULL;
  }
}

}// namespace plog


#endif /* PLog_SRC_IPCPROXY_H_ */
