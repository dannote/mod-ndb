/* Copyright (C) 2006 - 2009 Sun Microsystems
 All rights reserved. Use is subject to license terms.
 
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "mod_ndb.h"


char * result_buffer::init(request_rec *r, size_t size) {
  alloc_sz = size;
  sz = 0;
  if(buff) free(buff);
  buff = (char *) malloc(alloc_sz);
  if(r && !buff) log_err(r->server, "mod_ndb result_buffer::init() out of memory");
  return buff;
}


/* prepare(size_t len): make room for *len* new characters in the buffer.
   Expect them to be placed there one at a time using putc or out(fmt, ...).
   It is OK to prepare more space than you will actually use.
*/
   
bool result_buffer::prepare(size_t len) {
  char *old_buff = buff;
  register size_t new_sz = sz + len;
  
  if(new_sz > alloc_sz) {
    register int factor = (new_sz / alloc_sz) + 1;
    alloc_sz *= factor;
    buff = (char *) realloc(old_buff, alloc_sz);
    if(! buff) {
      free(old_buff);
      return false;
    }
  }
  return true;
}
  

void result_buffer::out(size_t len, const char *s) {
  if(len == 0 || (! this->prepare(len))) return;
  char *dst = buff + sz;
  sz += len;
  while(len--) *dst++ = *s++;
}


void result_buffer::out(const char *fmt, ... ) {
  va_list args;
  size_t old_size = sz;
  bool try_again;
  char *old_buff;

  do {    
    try_again = false;
    va_start(args,fmt);
    sz += vsnprintf((buff + sz), alloc_sz - sz, fmt, args);
    va_end(args);
    
    if(sz >= alloc_sz) {
      try_again = true;
      // The write was truncated.  Get a bigger buffer and do it again
      alloc_sz *= 4;
      sz = old_size;
      old_buff = buff;
      buff = (char *) realloc(old_buff, alloc_sz);
      if(! buff) {
        free(old_buff);
        return;
      }
    }
  } while(try_again);
}


void result_buffer::read_blob(NdbBlob *blob) {
  unsigned long long size64 = 0;

  blob->getLength(size64);
  unsigned int size = (unsigned int) size64;

  prepare(size);
  blob->readData((void *) (buff + sz), size);
  sz += size;
}


/* Take an existing result buffer and use it instead.
   This is used in raw output, to treat the existing BLOB buffer 
   as the output buffer, instead of copying.
*/
void result_buffer::overlay(result_buffer *other) {
  if(alloc_sz) free(buff);

  alloc_sz = other->alloc_sz;  // other->buff will be freed here,
  other->alloc_sz = 0;         // not there.
  
  sz = other->sz;
  buff = other->buff;
}


result_buffer::~result_buffer() {
  if(alloc_sz) free(buff);
}
