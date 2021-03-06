/*
 * pointer.cpp - 
 * @author: Jonathan Beard
 * @version: Thu May 15 09:58:51 2014
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
#include <cstdint>
#include <cstddef>
#include "pointer.hpp"

Pointer::Pointer(const std::size_t cap, 
                 const wrap_t wrap_set ) : Pointer( cap )
{
    wrap_a = wrap_set;
    wrap_b = wrap_set;
}


Pointer::Pointer( Pointer * const other, 
                  const std::size_t new_cap ) : Pointer( new_cap )
{
   const auto val(  Pointer::val( other ) );
   a = val;
   b = val;
}

std::size_t 
Pointer::val( Pointer * const ptr ) 
{
   struct{
      std::uint64_t a;
      std::uint64_t b;
   }copy;
   do{
      copy.a = ptr->a;
      copy.b = ptr->b;
   }while( copy.a !=  copy.b );
   return( copy.b );
}

void
Pointer::inc( Pointer * const ptr ) 
{
   ptr->a = ( ptr->a + 1 ) % ptr->max_cap;
   ptr->b = ( ptr->b + 1 ) % ptr->max_cap;
   if( ptr->b == 0 )
   {
      ptr->wrap_a++;
      ptr->wrap_b++;
   }
}

void
Pointer::incBy( Pointer * const ptr, 
                const std::size_t in )
{
   ptr->a = ( ptr->a + in ) % ptr->max_cap;
   ptr->b = ( ptr->b + in ) % ptr->max_cap;
   if( ptr->b < in )
   {
      ptr->wrap_a++;
      ptr->wrap_b++;
   }
}

std::size_t 
Pointer::wrapIndicator( Pointer * const ptr ) 
{
   struct{
      std::uint64_t a;
      std::uint64_t b;
   }copy;
   do{
      copy.a = ptr->wrap_a;
      copy.b = ptr->wrap_b;
   }while( copy.a != copy.b );
   return( copy.b );
}
