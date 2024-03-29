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

#define FORM_TABLE_SIZE 16

class BLOB;   // forward declaration

class query_source : public apache_object {
 protected:
  BLOB * form_table[FORM_TABLE_SIZE];
  
 public:
  request_rec *r;
  int req_method;
  const char *content_type;
  bool keep_tx_open;
  const unsigned char *databuffer;

  void set_item(const char *name, const char *val) {
    set_item(name, val, strlen(val));
  };
  void set_item(const char *name, len_string *ls) {
    set_item(name, ls->string, ls->len);
  };
  void set_item(const char *, const char *, size_t); 
  len_string * get_item(const char *);

  virtual int get_form_data() = 0;
  virtual ~query_source() {};
};


class HTTP_query_source : public query_source {
 public:
  HTTP_query_source(request_rec *req) {
    r = req;
    req_method = r->method_number;
    content_type = ap_table_get(r->headers_in, "Content-Type");
    keep_tx_open = false;
  };
    
  int get_form_data();
  
};


class Apache_subrequest_query_source : public query_source {
  public:
  Apache_subrequest_query_source(request_rec *);
  int get_form_data();
};
