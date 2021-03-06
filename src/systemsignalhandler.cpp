/**
 * systemsignalhandler.cpp - 
 * @author: Jonathan Beard
 * @version: Sat Dec  6 18:19:13 2014
 * 
 * Copyright 2014 Jonathan Beard
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "systemsignalhandler.hpp"
#include "fifo.hpp"
#include "rafttypes.hpp"

NoSignalHandlerFoundException::NoSignalHandlerFoundException( 
   const std::string message )
{
   (this)->message = message;
}

const char*
NoSignalHandlerFoundException::what() const noexcept
{
   return( message.c_str() );
}

void
SystemSignalHandler::addHandler( const raft::signal signal,
                                 sighandler         handler )
{
   handlers[ signal ] = handler;
}

raft::kstatus
SystemSignalHandler::callHandler( const raft::signal signal,
                                  FIFO               &fifo,
                                  raft::kernel       *kernel,
                                  void *data )
{
   auto ret_func( handlers.find( signal ) );
   if( ret_func == handlers.end() )
   {
      throw NoSignalHandlerFoundException( "No handler found for: " +
                                             std::to_string( signal ) );
   }
   else
   {
      return( 
      (*ret_func).second( fifo,
                          kernel,                         
                          signal, 
                          data ) ); 
   }
}
