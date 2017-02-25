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
 * LogMessage.h
 *
 *  Created on: Feb 24, 2017
 *      Author: yukai
 *		 Email: yukai2014@gmail.com
 * 
 * Description:
 *
 */

#ifndef PLog_SRC_LOGMESSAGE_H_
#define PLog_SRC_LOGMESSAGE_H_
#include <assert.h>
#include <strstream>

#include "./IPCProxy.h"
using std::ostrstream;

namespace plog{

enum LEVEL{DEBUG, INFO, WARNING, ERROR, FATAL};

thread_local char buffer[buffer_max_size];

class LogMessage: public ostrstream {
 public:
  // reserve first four char for length and last two char for '\n' and '\0'
  LogMessage(const char* file_name, int line, int level)
   :ostrstream(buffer+sizeof(int), buffer_max_size - 2 - sizeof(int)) {
       proxy_ = GetProxy(PIPE);
  }

  ~LogMessage() {
    char*ptr = str()+pcount();
    *ptr++= '\n';
    *ptr = '\0';
    SendToServer(str(), pcount()+2);
  }

 private:
  void SendToServer(void* buffer, int64_t size) {
//    cout<<"calling sendtoserver()"<<endl;
    assert(size>0 && "size must greater than zero");
    if (proxy_->Send(buffer, size) <0) {
      cout<<"failed to send to server"<<endl;
    }
  }

 private:
  static IPCProxy* proxy_;
};

IPCProxy* LogMessage::proxy_ = NULL;

} // namespace plog

#endif /* PLog_SRC_LOGMESSAGE_H_ */
